#pragma once

#include <glad/glad.h>
#include <imgui.h>

#include "fbo.h"
#include "glm/ext/matrix_transform.hpp"
#include "gpu.h"
#include "model.h"
#include "shader.h"

struct Water {
  static constexpr std::array<const char*, 4> DebugNames{
      {"Off", "SSR Reflection", "SSR Refraction", "SSR Refraction Misses"}};
  enum WaterDebugFlags {
    Off,
    SSR_Reflection,
    SSR_Refraction,
    SSR_Refraction_Misses,
  };

  void init() {
    indices_count = gpu::createSubdividedPlane(1, 0, &vao, &positions_bo, &indices_bo);
    loadShader(false);
    dudv_map.load("resources/textures/", "water_dudv_tile.jpg", 3);
  }

  void deinit() {
    glDeleteTextures(1, &dudv_map.gl_id);

    glDeleteBuffers(1, &this->positions_bo);
    glDeleteBuffers(1, &this->indices_bo);
    glDeleteVertexArrays(1, &this->vao);

    glDeleteFramebuffers(1, &this->screen_fbo.framebufferId);
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

  void render(Terrain* terrain, int width, int height, float current_time,
              glm::mat4 projection_matrix, glm::mat4 view_matrix, glm::vec3 center,
              Projection projection, float environment_multiplier) {
    if (screen_fbo.width != width || screen_fbo.height != height) {
      screen_fbo.resize(width, height);
    }

    GLint prev_fbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_fbo);

    glBlitNamedFramebuffer(prev_fbo, screen_fbo.framebufferId, 0, 0, screen_fbo.width,
                           screen_fbo.height, 0, 0, screen_fbo.width, screen_fbo.height,
                           GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    GLint prev_program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);

    float s = (terrain->terrain_size / (terrain->terrain_subdivision + 1));
    auto model_matrix = glm::translate(glm::vec3(glm::floor((center.x) / s) * s, this->height,
                                                 glm::floor((center.z) / s) * s)
                                       - glm::vec3(1, 0, 1) * size / 2.0f)
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

    glBindTextureUnit(0, screen_fbo.colorTextureTargets[0]);
    glBindTextureUnit(1, screen_fbo.depthBuffer);
    glBindTextureUnit(2, dudv_map.gl_id);

    glUseProgram(this->shader_program);
    gpu::setUniformSlow(this->shader_program, "debug_flag", debug_flag);
    gpu::setUniformSlow(this->shader_program, "current_time", current_time);
    gpu::setUniformSlow(this->shader_program, "model_matrix", model_matrix);
    gpu::setUniformSlow(this->shader_program, "view_matrix", view_matrix);
    gpu::setUniformSlow(this->shader_program, "inv_view_matrix", glm::inverse(view_matrix));
    gpu::setUniformSlow(this->shader_program, "projection_matrix", projection_matrix);
    gpu::setUniformSlow(this->shader_program, "pixel_projection", pixel_projection);

    gpu::setUniformSlow(this->shader_program, "sun.direction", terrain->sun.direction);
    gpu::setUniformSlow(this->shader_program, "sun.color", terrain->sun.color);

    gpu::setUniformSlow(this->shader_program, "water.height", this->height);
    gpu::setUniformSlow(this->shader_program, "water.foam_distance", foam_distance);
    gpu::setUniformSlow(this->shader_program, "water.wave_speed", wave_speed);
    gpu::setUniformSlow(this->shader_program, "water.wave_strength", wave_strength);
    gpu::setUniformSlow(this->shader_program, "water.wave_scale", wave_scale);
    gpu::setUniformSlow(this->shader_program, "water.size", this->size);
    ssr_reflection.upload(this->shader_program, "ssr_reflection", screen_fbo.width,
                          screen_fbo.height, projection);
    ssr_refraction.upload(this->shader_program, "ssr_refraction", screen_fbo.width,
                          screen_fbo.height, projection);

    gpu::setUniformSlow(this->shader_program, "environment_multiplier", environment_multiplier);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indices_count, GL_UNSIGNED_SHORT, 0);
    glBindVertexArray(0);

    glUseProgram(prev_program);
  }

  void gui() {
    if (ImGui::CollapsingHeader("Water")) {
      ImGui::Combo("Debug", &debug_flag, &DebugNames[0], DebugNames.size());

      ImGui::DragFloat("Water size", &size, 4, 0, FLT_MAX);
      ImGui::DragFloat("Water Level Height", &height, 0.1, 0, FLT_MAX);
      ImGui::DragFloat("Water Foam Distance", &foam_distance, 0.1, 0, FLT_MAX);
      ImGui::DragFloat("Wave speed", &wave_speed, 0.003, 0, FLT_MAX);
      ImGui::DragFloat("Wave strength", &wave_strength, 0.003, 0, FLT_MAX);
      ImGui::DragFloat("Wave scale", &wave_scale, 0.1, 0, FLT_MAX);

      if (ImGui::CollapsingHeader("SSR Reflection")) {
        ssr_reflection.gui();
      }
      if (ImGui::CollapsingHeader("SSR Refraction")) {
        ssr_refraction.gui();
      }

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
    Projection projection;

    float z_thickness = 0.01;
    float stride = 15.00;
    float jitter = 0.5;
    float max_steps = 50.0;
    float max_distance = 500.0;

    ScreenSpaceReflection() = default;
    ScreenSpaceReflection(float z_thickness, float stride, float max_steps)
        : z_thickness(z_thickness), stride(stride), max_steps(max_steps) {}

    void upload(GLuint program, std::string uniform_name, int width, int height,
                Projection projection) {
      this->depth_buffer_size = glm::ivec2(width, height);
      this->projection = projection;

      gpu::setUniformSlow(program, (uniform_name + ".depth_buffer_size").c_str(),
                          depth_buffer_size);
      gpu::setUniformSlow(program, (uniform_name + ".z_near").c_str(), projection.near);
      gpu::setUniformSlow(program, (uniform_name + ".z_far").c_str(), projection.far);
      gpu::setUniformSlow(program, (uniform_name + ".z_thickness").c_str(), z_thickness);
      gpu::setUniformSlow(program, (uniform_name + ".stride").c_str(), stride);
      gpu::setUniformSlow(program, (uniform_name + ".jitter").c_str(), jitter);
      gpu::setUniformSlow(program, (uniform_name + ".max_steps").c_str(), max_steps);
      gpu::setUniformSlow(program, (uniform_name + ".max_distance").c_str(), max_distance);
    }

    void gui() {
      ImGui::Text("Buffer size: %dx%d", depth_buffer_size.x, depth_buffer_size.y);
      ImGui::Text("Near: %f, Far: %d", projection.near, projection.far);
      ImGui::DragFloat("z thickness", &z_thickness, 0.0001, 0, FLT_MAX);
      ImGui::DragFloat("stride", &stride, 0.0001, 0, FLT_MAX);
      ImGui::SliderFloat("jitter", &jitter, 0.0, 1.0);
      ImGui::DragFloat("Max steps", &max_steps, 0.1, 0.0, FLT_MAX);
      ImGui::DragFloat("Max distance", &max_distance, 0.1, 0.0, FLT_MAX);
    }
  };

  int debug_flag = WaterDebugFlags::Off;

  int indices_count;

  // Height of the water level
  float height = 140;
  float size = 4096 * 2.0;
  float foam_distance = 30.f;
  float wave_speed = 0.045f;
  float wave_strength = 0.053f;
  float wave_scale = 406;
  ScreenSpaceReflection ssr_reflection;
  ScreenSpaceReflection ssr_refraction = ScreenSpaceReflection(20, 10, 20);
  gpu::Texture dudv_map;

  GLuint shader_program;

  FboInfo screen_fbo;

  // Buffers on GPU
  uint32_t positions_bo;
  uint32_t indices_bo;

  // Vertex Array Object
  uint32_t vao;
};
