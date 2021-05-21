#pragma once

#include <glad/glad.h>
#include <imgui.h>

#include <array>
#include <fstream>
#include <glm/detail/type_vec3.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <sstream>
#include <vector>

#include "gpu.h"
#include "model.h"
#include "shader.h"

struct TerrainNoise {
  int num_octaves = 7;
  float amplitude = 100.0;
  float frequency = 0.1;
  float persistence = .08;
  float lacunarity = 8.0;

  bool gui() {
    auto did_change = false;
    did_change |= ImGui::SliderInt("Octaves", &this->num_octaves, 1, 10);
    did_change |= ImGui::DragFloat("Amplitude", &this->amplitude, 1.0f, 0.0f, 10000.f);
    did_change |= ImGui::DragFloat("Frequency", &this->frequency, 0.001f, 0.0f, 10000.f);
    did_change |= ImGui::DragFloat("Persistence", &this->persistence, 0.001f, 0.0f, 10000.f);
    did_change |= ImGui::DragFloat("Lacunarity", &this->lacunarity, 0.05f, 0.0f, 20.f);
    return did_change;
  }
};

struct Sun {
  glm::vec3 direction = glm::vec3(0.0, -1.0, 0.0);
  glm::vec3 color = glm::vec3(1.0, 1.0, 1.0);

  bool gui() {
    auto did_change = false;
    did_change |= ImGui::DragFloat3("Direction", &direction.x, 0.04);
    did_change |= ImGui::ColorPicker3("Color", &color.x);

    direction = glm::normalize(direction);
    return did_change;
  }
};

struct Terrain {
  float terrain_size = 4096.0;
  int terrain_subdivision = 24;
  int indices_count;

  bool wireframe = false;

  TerrainNoise noise;
  Sun sun;

  float tess_multiplier = 16.0;

  GLuint shader_program;

  glm::mat4 model_matrix = glm::mat4(1.0);

  gpu::Texture grass_texture;
  gpu::Texture rock_texture;

  // Buffers on GPU
  uint32_t positions_bo;
  uint32_t normals_bo;
  uint32_t indices_bo;

  // Vertex Array Object
  uint32_t vaob;

  void init();
  void init_mesh(bool is_reload);
  void deinit();

  float getHeight(float x, float z);

  void render(glm::mat4 projection_matrix, glm::mat4 view_matrix, glm::vec3 camera_position);

  void loadShader(bool is_reload);

  void gui(SDL_Window* window);
};
