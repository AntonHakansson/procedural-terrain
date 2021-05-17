#pragma once

#include <glad/glad.h>

#include <array>

#include "gpu.h"

struct ShaderInput {
  std::string filepath;
  /**
   * Describes what type of shader this is
   * - GL_VERTEX_SHADER
   * - GL_FRAGMENT_SHADER
   * - GL_TESS_CONTROL_SHADER
   * - GL_TESS_EVALUATION_SHADER
   * */
  GLenum type;
};

template <auto N>
GLuint loadShaderProgram(const std::array<ShaderInput, N>& shaders, bool allow_errors) {
  GLuint gl_program = glCreateProgram();

  for (auto& s : shaders) {
    GLuint gl_shader = glCreateShader(s.type);

    std::ifstream s_file(s.filepath);
    std::string s_src((std::istreambuf_iterator<char>(s_file)), std::istreambuf_iterator<char>());

    const char* source = s_src.c_str();

    glShaderSource(gl_shader, 1, &source, nullptr);

    glCompileShader(gl_shader);

    int compileOk = 0;
    glGetShaderiv(gl_shader, GL_COMPILE_STATUS, &compileOk);
    if (!compileOk) {
      std::string err = gpu::GetShaderInfoLog(gl_shader);
      if (allow_errors) {
        gpu::non_fatal_error(err, s.filepath);
      } else {
        gpu::fatal_error(err, s.filepath);
      }
      return 0;
    }

    glAttachShader(gl_program, gl_shader);
    glDeleteShader(gl_shader);
  }
  if (!allow_errors) {
    CHECK_GL_ERROR();
  }
  if (!gpu::linkShaderProgram(gl_program, allow_errors)) return 0;

  return gl_program;
}
