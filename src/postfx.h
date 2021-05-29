#pragma once

#include <glad/glad.h>
#include <imgui.h>

#include "camera.h"
#include "fbo.h"
#include "glm/ext/matrix_transform.hpp"
#include "gpu.h"
#include "shader.h"
#include "terrain.h"
#include "water.h"

struct PostFX {
  void init() {}

  void deinit() { glDeleteFramebuffers(1, &this->screen_fbo.framebufferId); }

  void loadShader(bool is_reload) {
    std::array<ShaderInput, 2> program_shaders({
        ShaderInput{"resources/shaders/postfx.vert", GL_VERTEX_SHADER},
        ShaderInput{"resources/shaders/postfx.frag", GL_FRAGMENT_SHADER},
    });

    auto program = loadShaderProgram(program_shaders, is_reload);
    if (program != 0) {
      if (is_reload) {
        glDeleteProgram(this->shader_program);
      }

      this->shader_program = program;
    }
  }

  void bind(int width, int height) {
    if (screen_fbo.width != width || screen_fbo.height != height) {
      screen_fbo.resize(width, height);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, screen_fbo.framebufferId);
  }

  void unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

  void render(Projection projection, mat4 view_matrix, mat4 proj_matrix, float current_time,
              Water* water, Sun* sun) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glUseProgram(shader_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, screen_fbo.colorTextureTargets[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, screen_fbo.depthBuffer);

    gpu::setUniformSlow(shader_program, "viewMatrix", view_matrix);
    gpu::setUniformSlow(shader_program, "projMatrix", proj_matrix);
    gpu::setUniformSlow(shader_program, "currentTime", current_time);
    gpu::setUniformSlow(shader_program, "water.height", water->height);
    gpu::setUniformSlow(shader_program, "sun.direction", sun->direction);
    gpu::setUniformSlow(shader_program, "sun.color", sun->color);
    gpu::setUniformSlow(shader_program, "sun.intensity", sun->intensity);
    gpu::setUniformSlow(shader_program, "postfx.z_near", projection.near);
    gpu::setUniformSlow(shader_program, "postfx.z_far", projection.far);
    gpu::setUniformSlow(shader_program, "postfx.debug_mask", debug_mask);

    gpu::drawFullScreenQuad();
  }

  void gui() {
    if (ImGui::CollapsingHeader("Post FX")) {
      ImGui::Image((void*)(intptr_t)screen_fbo.colorTextureTargets[0], ImVec2(252, 252),
                   ImVec2(0, 1), ImVec2(1, 0));

      ImGui::SameLine();

      ImGui::Image((void*)(intptr_t)screen_fbo.depthBuffer, ImVec2(252, 252), ImVec2(0, 1),
                   ImVec2(1, 0));

      ImGui::NewLine();

      ImGui::Text("Debug");
      ImGui::Combo("Show mask", &debug_mask, &DebugMasks[0], DebugMasks.size());
    }
  }

  GLuint shader_program;
  FboInfo screen_fbo;

  static constexpr std::array<const char*, 3> DebugMasks{{"Off", "Horizon mask", "God ray mask"}};

  int debug_mask = 0;
};
