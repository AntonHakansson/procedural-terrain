#pragma once

#include <glad/glad.h>
#include <imgui.h>

#include "fbo.h"
#include "gpu.h"
#include "shader.h"

struct Water {
  void init() {
    indices_count = gpu::createSubdividedPlane(this->water_size, 0, &this->vao, &this->positions_bo,
                                               &this->indices_bo);
    loadShader(false);
  }

  void deinit() {
    glDeleteBuffers(1, &this->positions_bo);
    glDeleteBuffers(1, &this->indices_bo);
    glDeleteVertexArrays(1, &this->vao);
  }

  void loadShader(bool is_reload) {
    if (is_reload) {
      glDeleteProgram(this->shader_program);
    }
    std::array<ShaderInput, 2> program_shaders({
        {"resources/shaders/water.vert", GL_VERTEX_SHADER},
        {"resources/shaders/water.frag", GL_FRAGMENT_SHADER},
    });
    this->shader_program = loadShaderProgram(program_shaders, is_reload);
  }

  void begin(int width, int height) {
    screen_fbo.resize(width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, screen_fbo.framebufferId);
    glViewport(0, 0, width, height);
    glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  void end() {
    {
      GLint current_fbo = 0;
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &current_fbo);
      assert(screen_fbo.framebufferId == current_fbo
             && "Framebuffer target was modified during water pass");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  void render(glm::mat4 projection_matrix, glm::mat4 view_matrix, glm::vec3 camera_position, float z_near) {
    GLint prev_program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, screen_fbo.colorTextureTargets[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, screen_fbo.depthBuffer);

    glUseProgram(this->shader_program);
    gpu::setUniformSlow(this->shader_program, "view_matrix", view_matrix);
    gpu::setUniformSlow(this->shader_program, "projection_matrix", projection_matrix);
    gpu::setUniformSlow(this->shader_program, "camera_position", camera_position);
    gpu::setUniformSlow(this->shader_program, "water.height", height);
    gpu::setUniformSlow(this->shader_program, "water.foam_distance", foam_distance);
    ssr.upload(this->shader_program, screen_fbo.width, screen_fbo.height, z_near);

    gpu::drawFullScreenQuad();

    glUseProgram(prev_program);
  }

  void gui() {
    if (ImGui::CollapsingHeader("Water")) {
      ssr.gui();
      ImGui::DragFloat("Water Level Height", &height, 0.1);
      ImGui::DragFloat("Water Foam Distance", &foam_distance, 0.1);
      ImGui::Text("Color Attachment");
      ImGui::Image((void*)(intptr_t)screen_fbo.colorTextureTargets[0], ImVec2(252, 252), ImVec2(0, 1), ImVec2(1,0));
      ImGui::Text("Depth Attachment");
      ImGui::Image((void*)(intptr_t)screen_fbo.depthBuffer, ImVec2(252, 252), ImVec2(0, 1), ImVec2(1,0));
    }
  }


  struct ScreenSpaceReflection {
    // REVIEW: maybe remove these
    glm::ivec2 depth_buffer_size;
    float z_near;

    float z_thickness = 0.01;
    float stride = 1.05;
    float max_steps = 30.0;
    float max_distance = 200.0;

    void upload(GLuint program, int width, int height, float z_near) {
      this->depth_buffer_size = glm::ivec2(width, height);
      this->z_near = z_near;

      gpu::setUniformSlow(program, "ssr.depth_buffer_size", depth_buffer_size);
      gpu::setUniformSlow(program, "ssr.z_near", z_near);
      gpu::setUniformSlow(program, "ssr.z_thickness", z_thickness);
      gpu::setUniformSlow(program, "ssr.stride", stride);
      gpu::setUniformSlow(program, "ssr.max_steps", max_steps);
      gpu::setUniformSlow(program, "ssr.max_distance", max_distance);
    }

    void gui() {
      ImGui::Text("Buffer size: %dx%d", depth_buffer_size.x, depth_buffer_size.y);
      ImGui::Text("Near: %f", z_near);
      ImGui::DragFloat("z thickness", &z_thickness, 0.1);
      ImGui::DragFloat("stride", &stride, 0.1);
      ImGui::DragFloat("Max steps", &max_steps, 0.1);
      ImGui::DragFloat("Max distance", &max_distance, 0.1);
    }
  } ssr;

  float water_size = 512.0;
  float foam_distance = 10.f;
  int indices_count;

  // Height of the water level
  float height = 0.5;

  GLuint shader_program;

  FboInfo screen_fbo;

  // Buffers on GPU
  uint32_t positions_bo;
  uint32_t indices_bo;

  // Vertex Array Object
  uint32_t vao;
};
