#pragma once

#include <SDL.h>
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

#include "camera.h"
#include "debug.h"
#include "fbo.h"

using namespace glm;
using std::string;

#define NUM_CASCADES 3
#define NUM_FRUSTUM_CORNERS 8

enum ShadowClampMode { Edge = 1, Border = 2 };
class ShadowMap {
public:
  int resolution = 1024 * 4;
  float bias = 4098;
  float blend_distance = 150.0;

  GLuint fbo;
  GLuint shadow_tex;
  OrthoProjInfo shadow_ortho_info[NUM_CASCADES];
  mat4 shadow_projections[NUM_CASCADES];
  float cascade_splits[NUM_CASCADES + 1];

  // Debug
  bool debug_show_splits = false;
  bool debug_show_blend = false;
  bool debug_show_projections = false;

  ShadowMap(void);

  // Init shadow map
  void init(Projection projection);

  void calculateSplits(Projection projection);

  bool checkFramebufferComplete() const;

  // Bind shadow map
  void bindWrite(uint cascade_index);

  // Set necessary uniforms
  void begin(uint tex_index, Projection projection, mat4 proj_matrix, mat4 light_view_matrix);

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
