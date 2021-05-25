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
  int clamp_mode = ShadowClampMode::Border;
  bool clamp_border_shadowed = false;
  bool use_polygon_offset = true;
  bool use_soft_falloff = false;
  bool use_hardware_pcfg = false;
  float polygon_offset_factor = 1.125f;
  float polygon_offset_units = 2.0f;
  FboInfo fbo_old;


  GLuint fbo;
  GLuint shadow_tex;
  GLuint shadow_maps[NUM_CASCADES];
  OrthoProjInfo m_shadowOrthoProjInfo[NUM_CASCADES];
  float m_cascadeEnd[NUM_CASCADES + 1];

  ShadowMap(void);

  // init shadow map
  void init(float z_near, float z_far);
  
  bool checkFramebufferComplete() const;

// bind shadow map
  void bindWrite(uint cascade_index);
  void bindRead(GLuint tex0, GLuint tex1, GLuint tex2);

  void setUniforms(GLuint shader_program, mat4 proj_matrix, mat4 light_view_matrix);

  // calc ortho projections
  void calcOrthoProjs(mat4 view_matrix, mat4 light_view_matrix, int width, int height, float fovy);

  void gui(SDL_Window* window);

  void debugProjs(mat4 view_matrix, mat4 proj_matrix, mat4 light_view_matrix);

  void setLightWVP();
};