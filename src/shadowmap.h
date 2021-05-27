#pragma once

#include <glad/glad.h>
#include <stb_image.h>
#include <stdint.h>

#include <cassert>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "debug.h"
#include "fbo.h"

using namespace glm;
using std::string;

#define NUM_CASCADES 3
#define NUM_FRUSTUM_CORNERS 8

enum ShadowClampMode { Edge = 1, Border = 2 };

struct OrthoProjInfo {
  float r;  // right
  float l;  // left
  float b;  // bottom
  float t;  // top
  float n;  // z near
  float f;  // z far
};

class ShadowMap {
public:
  int resolution = 1024 * 4;
  bool use_polygon_offset = true;
  float polygon_offset_factor = 1.125f;
  float polygon_offset_units = 2.0f;
  float bias = 2000;

  GLuint fbo;
  GLuint shadow_tex;
  OrthoProjInfo shadow_ortho_info[NUM_CASCADES];
  float cascade_splits[NUM_CASCADES + 1];

  ShadowMap(void);

  // Init shadow map
  void init(float z_near, float z_far);

  bool checkFramebufferComplete() const;

  // Bind shadow map
  void bindWrite(uint cascade_index);

  // Set necessary uniforms
  void begin(uint tex_index, mat4 proj_matrix, mat4 light_view_matrix);

  // Calculate ortho projections
  void calculateLightProjMatrices(mat4 view_matrix, mat4 light_view_matrix, int width, int height,
                                  float fovy);
  mat4 getLightProjMatrix(uint cascade_index);

  // Debug
  void debugProjs(mat4 view_matrix, mat4 proj_matrix, mat4 light_view_matrix);
  void gui(SDL_Window* window);

  // Deinit
  void deinit();
};
