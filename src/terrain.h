#pragma once

#include <SDL.h>

#include <imgui.h>

#include <ImGuizmo.h>
#include <glad/glad.h>

#include <stb_image.h>

#include <array>
#include <fstream>
#include <glm/detail/type_vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>
#include <sstream>
#include <vector>

#include "camera.h"
#include "debug.h"
#include "gpu.h"
#include "model.h"
#include "shader.h"

struct TerrainNoise {
  int num_octaves = 7;
  float amplitude = 900.0;
  float frequency = 0.105;
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
  glm::vec3 direction = glm::vec3(0.702, -0.713, 0.0);
  glm::vec3 color = glm::vec3(1.0, 0.4745, 0.062745);
  float intensity = 1;

  glm::vec3 orbit_axis = glm::vec3(0.702, 0, -0.713);
  float orbit_speed = 0.f;

  glm::mat4 matrix = inverse(glm::lookAt(vec3(0), -direction, vec3(0, 1, 0)));

  bool gui(Camera* camera) {
    auto did_change = false;


    if (ImGui::CollapsingHeader("Force orbit")) {
      mat4 cube_view, cube_proj;

      DebugDrawer::instance()->beginGizmo(camera->getViewMatrix(), vec2(256, 256), cube_view,
                                          cube_proj);
      ImGuizmo::Manipulate(&cube_view[0][0], &cube_proj[0][0], ImGuizmo::ROTATE, ImGuizmo::LOCAL,
                          &matrix[0][0], nullptr, nullptr);
      DebugDrawer::instance()->endGizmo();

      direction = vec3(matrix[2][0], matrix[2][1], matrix[2][2]);
    }

    did_change |= ImGui::DragFloat("Intensity", &intensity, 0.05);
    did_change |= ImGui::DragFloat("Orbit speed", &orbit_speed, 10.f);
    did_change |= ImGui::ColorPicker3("Color", &color.x);

    // glm::mat4 viewMatrix = glm::mat4(1);
    // glm::mat4 projMatrix = glm::perspective(glm::radians(45.0f), float(1000) / float(1000), 0.0f,
    // 100.0f);

    // ImGuiIO& io = ImGui::GetIO();
    // float viewManipulateRight = io.DisplaySize.x;
    // float viewManipulateTop = 0;

    // ImGuizmo::Manipulate(&viewMatrix[0][0], &projMatrix[0][0], ImGuizmo::ROTATE, ImGuizmo::LOCAL,
    // &matrix[0][0], nullptr, nullptr); ImGuizmo::ViewManipulate(&viewMatrix[0][0], 10.0f,
    // ImVec2(viewManipulateRight - 128, viewManipulateTop), ImVec2(128, 128), 0x10101010);

    direction = glm::normalize(direction);
    return did_change;
  }
};

struct Terrain {
  float terrain_size = 4096.0 * 2.0;
  int terrain_subdivision = 24;
  int indices_count;

  bool wireframe = false;
  bool simple = false;

  TerrainNoise noise;
  Sun sun;

  float tess_multiplier = 16.0;

  GLuint shader_program;
  GLuint shader_program_simple;

  glm::mat4 model_matrix = glm::mat4(1.0);

  gpu::Texture albedos;
  gpu::Texture normals;
  gpu::Texture displacements;

  std::array<float, 4> texture_start_heights{150, 270, 500, 500};
  std::array<float, 4> texture_blends{0.0, 80, 415, 610};
  std::array<float, 4> texture_sizes{10, 10, 7, 10};
  std::array<float, 4> texture_displacement_weights{1, 1, 1, 1};

  // Buffers on GPU
  uint32_t positions_bo;
  uint32_t indices_bo;

  // Vertex Array Object
  uint32_t vao;

  void init();
  void deinit();

  void update(float delta_time, float current_time);

  void loadShader(bool is_reload);
  void buildMesh(bool is_reload);

  void begin(bool simple);
  void render(glm::mat4 projection_matrix, glm::mat4 view_matrix, glm::vec3 camera_position,
              glm::mat4 light_matrix, float water_height);
  void gui(Camera* camera);
};
