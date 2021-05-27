#pragma once

#include <imgui.h>

#include <glm/detail/type_vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>

using namespace glm;

enum CameraMode {
  Fly,
  Orbit,
};

struct Camera {
  struct Projection {
    float fovy = 70.0f;
    float far = 5000.0f;
    float near = 1.0f;
  } projection;

  vec3 position = vec3(-70.0, 500.0, 70.0);
  vec3 direction = normalize(vec3(0.0) - position);
  vec3 world_up = vec3(0.0, 1.0, 0.0);
  float speed = 10;
  float rotation_speed = 0.12f;

  CameraMode mode = CameraMode::Fly;

  void key_event(const uint8_t *key_state, float delta_time) {
    auto camera_right = cross(direction, world_up);

    auto movement_dir = vec3(0.0);
    auto speed_multiplier = 1.0f;
    if (key_state[SDL_SCANCODE_W]) {
      movement_dir += direction;
    }
    if (key_state[SDL_SCANCODE_S]) {
      movement_dir -= direction;
    }
    if (key_state[SDL_SCANCODE_A]) {
      movement_dir -= camera_right;
    }
    if (key_state[SDL_SCANCODE_D]) {
      movement_dir += camera_right;
    }
    if (key_state[SDL_SCANCODE_Q]) {
      movement_dir -= world_up;
    }
    if (key_state[SDL_SCANCODE_E]) {
      movement_dir += world_up;
    }

    if (key_state[SDL_SCANCODE_LCTRL]) {
      speed_multiplier = 50.0;
    }
    else if (key_state[SDL_SCANCODE_LSHIFT]) {
      speed_multiplier = 15.0;
    }

    if (length(movement_dir) > 0.01) {
      movement_dir = normalize(movement_dir);
      position += speed * speed_multiplier * delta_time * movement_dir;
    }
  }

  void drag_event(int delta_x, int delta_y, float delta_time) {
    auto yaw = rotate(rotation_speed * delta_time * -delta_x, world_up);
    auto pitch
        = rotate(rotation_speed * delta_time * -delta_y, normalize(cross(direction, world_up)));
    direction = vec3(pitch * yaw * vec4(direction, 0.0f));
  }

  mat4 getViewMatrix() { return lookAt(position, position + direction, world_up); }

  mat4 getProjMatrix(int window_width, int window_height) {
    return perspective(radians(projection.fovy), float(window_width) / float(window_height),
                       projection.near, projection.far);
  }

  void gui() {
    ImGui::SliderFloat("Movement Speed", &this->speed, 80.0, 350.0);
    ImGui::SliderFloat("Rotate Speed", &this->rotation_speed, 0.05, 2.0);
  }
};
