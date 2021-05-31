#pragma once

#include <glad/glad.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "gpu.h"

struct ShaderInput {
  std::filesystem::path filepath;
  /**
   * Describes what type of shader this is
   * - GL_VERTEX_SHADER
   * - GL_FRAGMENT_SHADER
   * - GL_TESS_CONTROL_SHADER
   * - GL_TESS_EVALUATION_SHADER
   * */
  GLenum type;
};

static bool readShaderSource(const std::filesystem::path& filepath, std::ifstream& in,
                             std::stringstream& out, int level) {
  std::string line;
  std::string include_keyword = "#include \"";
  auto line_number = 0;
  while (std::getline(in, line)) {
    if (line.find(include_keyword, 0) == 0) {
      auto end = line.find_last_of('\"');
      if (end == std::string::npos) {
        std::cout << filepath.u8string() << ":" << line_number << ": GLSL: Invalid include format: " << line
                  << "\n";
        return false;
      }
      auto include_file = line.substr(include_keyword.size(), end - include_keyword.size());

      std::filesystem::path include_filepath = filepath.parent_path() / include_file;
      std::ifstream s_file(include_filepath);
      if (!s_file.is_open()) {
        std::cout << filepath.u8string() << ": GLSL: Could not find include file " << include_filepath.u8string() << "\n";
        return false;
      }

      {
        out << "#line 0 " << level + 1 << "\n";
        auto success = readShaderSource(include_filepath, s_file, out, level + 1);
        if (!success) {
          return false;
        }
      }

      line_number += 2;
      out << "#line " << line_number << " " << level << "\n";
      continue;
    }
    out << line << "\n";
    line_number += 1;
  }

  return true;
}

template <auto N>
GLuint loadShaderProgram(const std::array<ShaderInput, N>& shaders, bool allow_errors) {
  GLuint gl_program = glCreateProgram();

  for (auto& s : shaders) {
    GLuint gl_shader = glCreateShader(s.type);

    std::ifstream s_file(s.filepath);
    std::stringstream shader_source;
    auto success = readShaderSource(s.filepath, s_file, shader_source, 0);
    if (!success) {
      if (!allow_errors) {
        gpu::fatal_error("Preprocessor error", s.filepath);
      }
      return 0;
    }

    std::string full_source{std::istreambuf_iterator<char>(shader_source),
                            std::istreambuf_iterator<char>()};
    const char* c_str = full_source.c_str();
    glShaderSource(gl_shader, 1, &c_str, nullptr);

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
  if (!gpu::linkShaderProgram(gl_program, allow_errors)) {
    return 0;
  }

  return gl_program;
}
