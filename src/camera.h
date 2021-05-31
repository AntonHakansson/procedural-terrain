#pragma once

#include <imgui.h>

#include <array>
#include <glm/detail/type_vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/mat4x4.hpp>

using namespace glm;

enum CameraMode {
  Fly,
  Orbit,
};

struct OrthoProjInfo {
  float r;  // right
  float l;  // left
  float b;  // bottom
  float t;  // top
  float n;  // z near
  float f;  // z far
};

struct Projection {
  float fovy = 70.0f;
  float far = 10000.0f;
  float near = 1.0f;
};

struct Camera {
  static constexpr std::array<const char *, 2> CameraModes{{"Fly", "Orbit"}};

  Projection projection;

  vec3 world_up = vec3(0.0, 1.0, 0.0);

  vec3 position = vec3(0, 500.0, 70.0);
  vec3 direction = vec3(0, 0, -1);
  float speed = 40;
  float rotation_speed = 0.12f;

  int mode = CameraMode::Fly;

  vec3 orbit_target = vec3(0);
  float orbit_distance = 5000.F;

  void key_event(const uint8_t *key_state, float delta_time) {
    auto speed_multiplier = 1.0f;
    if (key_state[SDL_SCANCODE_LCTRL]) {
      speed_multiplier = 50.0;
    } else if (key_state[SDL_SCANCODE_LSHIFT]) {
      speed_multiplier = 15.0;
    }

    if (mode == CameraMode::Fly) {
      auto camera_right = cross(direction, world_up);
      auto movement_dir = vec3(0.0);
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

      if (length(movement_dir) > 0) movement_dir = normalize(movement_dir);

      if (key_state[SDL_SCANCODE_Q]) {
        movement_dir.y -= 1;
      }
      if (key_state[SDL_SCANCODE_E]) {
        movement_dir.y += 1;
      }

      if (length(movement_dir) > 0.01) {
        position += speed * speed_multiplier * delta_time * movement_dir;
      }
    } else if (mode == CameraMode::Orbit) {
      if (key_state[SDL_SCANCODE_W]) {
        orbit_distance -= speed * speed_multiplier;
      }
      if (key_state[SDL_SCANCODE_S]) {
        orbit_distance += speed * speed_multiplier;
      }
    }
  }

  void drag_event(int delta_x, int delta_y, float delta_time) {
    auto yaw = rotate(rotation_speed * delta_time * -delta_x, world_up);
    auto pitch
        = rotate(rotation_speed * delta_time * -delta_y, normalize(cross(direction, world_up)));
    direction = vec3(pitch * yaw * vec4(direction, 0.0f));
  }

  mat4 getViewMatrix() {
    switch (mode) {
      case CameraMode::Fly: {
        return lookAt(position, position + direction, world_up);
      }

      case CameraMode::Orbit: {
        return lookAt(orbit_target - direction * orbit_distance, orbit_target, world_up);
      }
    }
    assert(0);
  }

  mat4 getProjMatrix(int window_width, int window_height) {
    return perspective(radians(projection.fovy), float(window_width) / float(window_height),
                       projection.near, projection.far);
  }

  vec3 getWorldPos() {
    switch (mode) {
      case CameraMode::Fly: {
        return position;
      }

      case CameraMode::Orbit: {
        return orbit_target - direction * orbit_distance;
      }
    }
    assert(0);
  }

  void gui() {
    ImGui::Combo("Camera mode", &mode, &CameraModes[0], CameraModes.size());

    ImGui::Spacing();

    ImGui::SliderFloat("Movement Speed", &this->speed, 80.0, 350.0);
    ImGui::SliderFloat("Rotate Speed", &this->rotation_speed, 0.05, 2.0);

    ImGui::Spacing();

    ImGui::DragFloat("Vertical FOV", &this->projection.fovy, 0.1, 45, 100);
    ImGui::DragFloat("Near Projection", &this->projection.near, 0.02, 0.2);
    ImGui::DragFloat("Far Projection", &this->projection.far, 200.0, 1000.0);
  }
};
