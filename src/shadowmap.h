#pragma once

#include <glad/glad.h>
#include <string>
#include <cassert>
#include <limits>
#include <iostream>
#include <vector>

#include <stb_image.h>
#include <stdint.h>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include "fbo.h"
#include "debug.h"

using namespace glm;
using std::string;

#define NUM_CASCADES 3
#define NUM_FRUSTUM_CORNERS 8

enum ShadowClampMode
{
  Edge = 1,
  Border = 2
};

struct OrthoProjInfo
{
    float r;        // right
    float l;        // left
    float b;        // bottom
    float t;        // top
    float n;        // z near
    float f;        // z far
};


class ShadowMap {
public:
  int resolution = 2024;
  bool use_polygon_offset = true;
  float polygon_offset_factor = 1.125f;
  float polygon_offset_units = 2.0f;

  GLuint fbo;
  GLuint shadow_maps[NUM_CASCADES];
  OrthoProjInfo shadow_ortho_info[NUM_CASCADES];
  float cascadeSplits[NUM_CASCADES + 1];

  ShadowMap(void);

  // Init shadow map
  void init(float z_near, float z_far);
  
  bool checkFramebufferComplete() const;

  // Bind shadow map
  void bindWrite(uint cascade_index);
  void bindRead(GLuint tex0, GLuint tex1, GLuint tex2);

  // Set necessary uniforms
  void setUniforms(GLuint shader_program, mat4 proj_matrix, mat4 light_view_matrix);

  // Calculate ortho projections
  void calculateLightProjMatrices(mat4 view_matrix, mat4 light_view_matrix, int width, int height, float fovy);
  mat4 getLightProjMatrix(uint cascade_index);

  // Debug
  void debugProjs(mat4 view_matrix, mat4 proj_matrix, mat4 light_view_matrix);
  void gui(SDL_Window* window);
};