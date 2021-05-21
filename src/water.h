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

  void render(glm::mat4 projection_matrix, glm::mat4 view_matrix, glm::vec3 camera_position) {
    GLint prev_program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);

    glUseProgram(this->shader_program);
    glBindVertexArray(this->vao);
    glDrawElements(GL_TRIANGLES, this->indices_count, GL_UNSIGNED_SHORT, 0);
    glBindVertexArray(0);

    glUseProgram(prev_program);
  }

  float water_size = 512.0;
  int indices_count;

  GLuint shader_program;

  FboInfo reflectionFbo;
  FboInfo refractionFbo;

  // Buffers on GPU
  uint32_t positions_bo;
  uint32_t indices_bo;

  // Vertex Array Object
  uint32_t vao;
};
