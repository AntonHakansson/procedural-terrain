#pragma once

#include <glad/glad.h>
#include <imgui.h>

#include "fbo.h"
#include "glm/ext/matrix_transform.hpp"
#include "gpu.h"
#include "model.h"
#include "shader.h"

struct Water {
  void init() {
    indices_count = gpu::createSubdividedPlane(1, 0, &vao, &positions_bo, &indices_bo);
    loadShader(false);
    dudv_map.load("resources/textures/", "water_dudv.png", 3);
  }

  void deinit() {
    glDeleteTextures(1, &dudv_map.gl_id);

    glDeleteBuffers(1, &this->positions_bo);
    glDeleteBuffers(1, &this->indices_bo);
    glDeleteVertexArrays(1, &this->vao);
  }

  void loadShader(bool is_reload) {
    std::array<ShaderInput, 2> program_shaders({
        ShaderInput{"resources/shaders/water.vert", GL_VERTEX_SHADER},
        ShaderInput{"resources/shaders/water.frag", GL_FRAGMENT_SHADER},
    });
    auto program = loadShaderProgram(program_shaders, is_reload);
    if (program != 0) {
      if (is_reload) {
        glDeleteProgram(this->shader_program);
      }
      this->shader_program = program;
    }
  }

  void render(int width, int height, float current_time, glm::mat4 projection_matrix,
              glm::mat4 view_matrix, glm::vec3 camera_position, float z_near, float z_far) {
    screen_fbo.resize(width, height);

    GLint prev_fbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_fbo);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, screen_fbo.framebufferId);

    glBlitFramebuffer(0, 0, screen_fbo.width, screen_fbo.height, 0, 0, screen_fbo.width,
                      screen_fbo.height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    // REVIEW: What are the default READ/DRAW framebuffer values?
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prev_fbo);

    GLint prev_program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);

    auto model_matrix = glm::translate(glm::vec3(camera_position.x - (size / 2.0), this->height,
                                                 camera_position.z - (size / 2.0)))
                        * glm::scale(vec3(size, 0, size));
    glm::mat4 pixel_projection;
    {
      float sx = float(screen_fbo.width) / 2.0;
      float sy = float(screen_fbo.height) / 2.0;

      auto warp_to_screen_space = glm::mat4(1.0);
      warp_to_screen_space[0] = glm::vec4(sx, 0, 0, sx);
      warp_to_screen_space[1] = glm::vec4(0, sy, 0, sy);
      warp_to_screen_space = transpose(warp_to_screen_space);

      pixel_projection = warp_to_screen_space * projection_matrix;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, screen_fbo.colorTextureTargets[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, screen_fbo.depthBuffer);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, dudv_map.gl_id);

    glUseProgram(this->shader_program);
    gpu::setUniformSlow(this->shader_program, "current_time", current_time);
    gpu::setUniformSlow(this->shader_program, "model_matrix", model_matrix);
    gpu::setUniformSlow(this->shader_program, "view_matrix", view_matrix);
    gpu::setUniformSlow(this->shader_program, "inv_view_matrix", glm::inverse(view_matrix));
    gpu::setUniformSlow(this->shader_program, "projection_matrix", projection_matrix);
    gpu::setUniformSlow(this->shader_program, "inv_projection_matrix",
                        glm::inverse(projection_matrix));
    gpu::setUniformSlow(this->shader_program, "pixel_projection", pixel_projection);

    gpu::setUniformSlow(this->shader_program, "water.height", this->height);
    gpu::setUniformSlow(this->shader_program, "water.foam_distance", foam_distance);
    gpu::setUniformSlow(this->shader_program, "water.wave_speed", wave_speed);
    gpu::setUniformSlow(this->shader_program, "water.wave_strength", wave_strength);
    gpu::setUniformSlow(this->shader_program, "water.wave_scale", wave_scale);
    gpu::setUniformSlow(this->shader_program, "water.size", this->size);
    ssr.upload(this->shader_program, screen_fbo.width, screen_fbo.height, z_near, z_far);

    /* gpu::drawFullScreenQuad(); */

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indices_count, GL_UNSIGNED_SHORT, 0);
    glBindVertexArray(0);

    glUseProgram(prev_program);
  }

  void gui() {
    if (ImGui::CollapsingHeader("Water")) {
      ImGui::DragFloat("Water size", &size, 4);
      ImGui::DragFloat("Water Level Height", &height, 0.1);
      ImGui::DragFloat("Water Foam Distance", &foam_distance, 0.1);
      ImGui::DragFloat("Wave speed", &wave_speed, 0.003);
      ImGui::DragFloat("Wave strength", &wave_strength, 0.003);
      ImGui::DragFloat("Wave scale", &wave_scale);

      ssr.gui();
      ImGui::Text("Color Attachment");
      ImGui::Image((void*)(intptr_t)screen_fbo.colorTextureTargets[0], ImVec2(252, 252),
                   ImVec2(0, 1), ImVec2(1, 0));
      ImGui::Text("Depth Attachment");
      ImGui::Image((void*)(intptr_t)screen_fbo.depthBuffer, ImVec2(252, 252), ImVec2(0, 1),
                   ImVec2(1, 0));
    }
  }

  struct ScreenSpaceReflection {
    // REVIEW: maybe remove these
    glm::ivec2 depth_buffer_size;
    float z_near;
    float z_far;

    float z_thickness = 0.01;
    float stride = 15.00;
    float jitter = 1;
    float max_steps = 50.0;
    float max_distance = 500.0;

    void upload(GLuint program, int width, int height, float z_near, float z_far) {
      this->depth_buffer_size = glm::ivec2(width, height);
      this->z_near = z_near;
      this->z_far = z_far;

      gpu::setUniformSlow(program, "ssr.depth_buffer_size", depth_buffer_size);
      gpu::setUniformSlow(program, "ssr.z_near", z_near);
      gpu::setUniformSlow(program, "ssr.z_far", z_far);
      gpu::setUniformSlow(program, "ssr.z_thickness", z_thickness);
      gpu::setUniformSlow(program, "ssr.stride", stride);
      gpu::setUniformSlow(program, "ssr.jitter", jitter);
      gpu::setUniformSlow(program, "ssr.max_steps", max_steps);
      gpu::setUniformSlow(program, "ssr.max_distance", max_distance);
    }

    void gui() {
      ImGui::Text("Buffer size: %dx%d", depth_buffer_size.x, depth_buffer_size.y);
      ImGui::Text("Near: %f, Far: %d", z_near, z_far);
      ImGui::DragFloat("z thickness", &z_thickness, 0.0001);
      ImGui::DragFloat("stride", &stride, 0.0001);
      ImGui::SliderFloat("jitter", &jitter, 0.0, 1.0);
      ImGui::DragFloat("Max steps", &max_steps, 0.1);
      ImGui::DragFloat("Max distance", &max_distance, 0.1);
    }
  } ssr;

  int indices_count;

  // Height of the water level
  float height = -100;
  float size = 4096 * 2.0;
  float foam_distance = 30.f;
  float wave_speed = 0.07;
  float wave_strength = 0.2;
  float wave_scale = 500;
  gpu::Texture dudv_map;

  GLuint shader_program;

  FboInfo screen_fbo;

  // Buffers on GPU
  uint32_t positions_bo;
  uint32_t indices_bo;

  // Vertex Array Object
  uint32_t vao;
};
