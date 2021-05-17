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

struct TerrainSimplexNoise {
  float amplitude;
  float frequency;
};

struct Terrain {
  float terrain_size;
  int indices_count;
  int terrain_subdivision;

  bool wireframe;

  std::vector<TerrainSimplexNoise> noises;

  float tess_multiplier;

  GLuint shader_program;

  glm::mat4 model_matrix;

  gpu::Material material;
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

  void draw_imgui(SDL_Window* window);
};
