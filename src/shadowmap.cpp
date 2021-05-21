#include "shadowmap.h"

#include <stb_image.h>
#include <stdint.h>

#include <glm/glm.hpp>
#include <iostream>
#include <vector>

using namespace glm;
using std::string;


ShadowMap::ShadowMap(void) {}

void ShadowMap::init() {
  this->fbo.resize(this->resolution, this->resolution);

  glBindTexture(GL_TEXTURE_2D, this->fbo.depthBuffer);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void ShadowMap::bind() {
  glBindFramebuffer(GL_FRAMEBUFFER, this->fbo.framebufferId);
}

void ShadowMap::bindDepthBuffer(GLuint texture) {
  glActiveTexture(texture);
  glBindTexture(GL_TEXTURE_2D, this->fbo.depthBuffer);
}

void ShadowMap::bindColorBuffer(GLuint texture) {
  glActiveTexture(texture);
  glBindTexture(GL_TEXTURE_2D, this->fbo.colorTextureTargets[0]);
}

void ShadowMap::begin() {
  if (this->fbo.width != this->resolution
      || this->fbo.height != this->resolution) {
    this->fbo.resize(this->resolution, this->resolution);
  }

  if (this->clamp_mode == ShadowClampMode::Edge) {
    glBindTexture(GL_TEXTURE_2D, this->fbo.depthBuffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }

  if (this->clamp_mode == ShadowClampMode::Border) {
    glBindTexture(GL_TEXTURE_2D, this->fbo.depthBuffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    vec4 border(this->clamp_border_shadowed ? 0.f : 1.f);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, &border.x);
  }

  // Draw shadow map
  this->bind();

  glViewport(0, 0, this->fbo.width, this->fbo.height);
  glClearColor(1, 1, 1, 1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (this->use_polygon_offset) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(this->polygon_offset_factor,
                    this->polygon_offset_units);
  }
}

void ShadowMap::end() {
  // gpu::Material& screen = models.landingpad->m_materials[8];
  // screen.m_emission_texture.gl_id = shadow_map_fb.colorTextureTargets[0];

  if (this->use_polygon_offset) {
    glDisable(GL_POLYGON_OFFSET_FILL);
  }
}