#include "shadowmap.h"

ShadowMap::ShadowMap(void) {}

void ShadowMap::init(float z_near, float z_far) {
  this->fbo_old.resize(this->resolution, this->resolution);

  m_cascadeEnd[0] = z_near;
  m_cascadeEnd[1] = 400.0f,
  m_cascadeEnd[2] = 1000.0f;
  m_cascadeEnd[3] = z_far;

  glBindTexture(GL_TEXTURE_2D, this->fbo_old.depthBuffer);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Create the depth buffer
  glGenTextures(NUM_CASCADES, shadow_maps);

  for (uint i = 0 ; i < NUM_CASCADES; i++) {
      glBindTexture(GL_TEXTURE_2D, shadow_maps[i]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

      glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, resolution, resolution, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
      // glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, resolution, resolution, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  }


  // Layered texture
  /*
  glGenTextures(1, &shadow_tex);
  glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_tex);
  glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH_COMPONENT32F, resolution, resolution, NUM_CASCADES);
  */





  // glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  // glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_maps[0], 0);

  // Create the FBO
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  // glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_tex, 0);

  for (uint i = 0 ; i < NUM_CASCADES; i++) {
    // glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, shadow_maps[i], 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_maps[i], 0);
  }

  glDrawBuffer(GL_NONE);

  // std::array<GLenum, 8> attachments(
  //     {GL_COLOR_ATTACHMENT0});
  // glDrawBuffers(NUM_CASCADES, &attachments[0]);

  // Disable writes to the color buffer
  // glDrawBuffer(GL_NONE);
  // glReadBuffer(GL_NONE);

  bool isComplete = checkFramebufferComplete();

  // restore default FBO
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // for (unsigned int i = 0; i < NUM_CASCADES; ++i)
  // {
  //     GLfloat p = (i + 1) / static_cast<GLfloat>(NUM_CASCADES);
  //     GLfloat log = minZ * std::pow(ratio, p);
  //     GLfloat uniform = minZ + range * p;
  //     GLfloat d = lambda * (log - uniform) + uniform;
  //     cascadeSplits[i] = (d - nearClip) / clipRange;
  // }
}

bool ShadowMap::checkFramebufferComplete() const {
  // Check that our FBO is correctly set up, this can fail if we have
  // incompatible formats in a buffer, or for example if we specify an
  // invalid drawbuffer, among things.
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    gpu::fatal_error("Framebuffer not complete");
  }

  return (status == GL_FRAMEBUFFER_COMPLETE);
}

void ShadowMap::bindWrite(uint cascade_index) {
  assert(cascade_index < NUM_CASCADES);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_maps[cascade_index], 0);
}

void ShadowMap::bindRead(GLuint tex0, GLuint tex1, GLuint tex2) {
  glActiveTexture(tex0);
  glBindTexture(GL_TEXTURE_2D, shadow_maps[0]);

  glActiveTexture(tex1);
  glBindTexture(GL_TEXTURE_2D, shadow_maps[1]);

  glActiveTexture(tex2);
  glBindTexture(GL_TEXTURE_2D, shadow_maps[2]);
}

void ShadowMap::setUniforms(GLuint shader_program, mat4 proj_matrix, mat4 light_view_matrix) {
  GLint prev_program = 0;
  glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);

  glUseProgram(shader_program);
  for (uint i = 0 ; i < NUM_CASCADES ; i++) {
    vec4 vView(0.0f, 0.0f, m_cascadeEnd[i + 1], 1.0f);
    vec4 vClip = proj_matrix * vView;

    gpu::setUniformSlow(shader_program, ("gCascadeEndClipSpace[" + std::to_string(i) + "]").c_str(), vClip.z);

    OrthoProjInfo info = m_shadowOrthoProjInfo[i];
    mat4 light_proj_matrix = ortho(info.l, info.r, info.b, info.t, info.n, info.f);

    gpu::setUniformSlow(shader_program, ("gLightWVP[" + std::to_string(i) + "]").c_str(), light_proj_matrix * light_view_matrix);
  }

  GLuint uUniform = glGetUniformLocation(shader_program, "gShadowMap[0]");
  glActiveTexture(GL_TEXTURE10);
  glBindTexture(GL_TEXTURE_2D, shadow_maps[0]);
  glUniform1i(uUniform, 10);

  uUniform = glGetUniformLocation(shader_program, "gShadowMap[1]");
  glActiveTexture(GL_TEXTURE11);
  glBindTexture(GL_TEXTURE_2D, shadow_maps[1]);
  glUniform1i(uUniform, 11);

  uUniform = glGetUniformLocation(shader_program, "gShadowMap[2]");
  glActiveTexture(GL_TEXTURE12);
  glBindTexture(GL_TEXTURE_2D, shadow_maps[2]);
  glUniform1i(uUniform, 12);

  glUseProgram(prev_program);
}

void ShadowMap::debugProjs(mat4 view_matrix, mat4 proj_matrix, mat4 light_view_matrix) {
  float fovy = 2.0 * atan(1.0 / proj_matrix[1][1]);
  float ar = proj_matrix[1][1] / proj_matrix[0][0];

  for (uint i = 0; i < NUM_CASCADES; i++) {
    mat4 proj = perspective(fovy, ar, m_cascadeEnd[i], m_cascadeEnd[i + 1]);

    OrthoProjInfo info = m_shadowOrthoProjInfo[i];

    mat4 light_proj_matrix = ortho(info.l, info.r, info.b, info.t, info.n, info.f);

    DebugDrawer::instance()->drawPerspectiveFrustum(view_matrix, proj, vec3(1, 0, 0));
    DebugDrawer::instance()->drawOrthographicFrustum(light_view_matrix, light_proj_matrix, vec3(1, 1, 0));
  }
}

void ShadowMap::calcOrthoProjs(mat4 view_matrix, mat4 light_view_matrix, int width, int height, float fovy) {
  mat4 view_inverse = inverse(view_matrix);

  float ar = width / (float)height;
  
  float tanHalfHFov = glm::tan(glm::radians(fovy / 2.0f)) * ar;
  float tanHalfVFov = glm::tan(glm::radians(fovy / 2.0));

  for (uint i = 0; i < NUM_CASCADES; i++) {
    float xn = m_cascadeEnd[i] * tanHalfHFov;
    float xf = m_cascadeEnd[i + 1] * tanHalfHFov;
    float yn = m_cascadeEnd[i] * tanHalfVFov;
    float yf = m_cascadeEnd[i + 1] * tanHalfVFov;

    vec4 frustum_corners[NUM_FRUSTUM_CORNERS] = {
        // near face
        view_inverse * vec4(xn, yn, -m_cascadeEnd[i], 1.0), 
        view_inverse * vec4(-xn, yn, -m_cascadeEnd[i], 1.0),
        view_inverse * vec4(xn, -yn, -m_cascadeEnd[i], 1.0), 
        view_inverse * vec4(-xn, -yn, -m_cascadeEnd[i], 1.0),

        // far face
        view_inverse * vec4(xf, yf, -m_cascadeEnd[i + 1], 1.0), 
        view_inverse * vec4(-xf, yf, -m_cascadeEnd[i + 1], 1.0),
        view_inverse * vec4(xf, -yf, -m_cascadeEnd[i + 1], 1.0), 
        view_inverse * vec4(-xf, -yf, -m_cascadeEnd[i + 1], 1.0)};

    vec4 frustum_corners_l[NUM_FRUSTUM_CORNERS];

    float minX, maxX, minY, maxY, minZ, maxZ;
    minX = minY = minZ = std::numeric_limits<float>::max();
    maxX = maxY = maxZ = std::numeric_limits<float>::lowest();

    for (uint j = 0; j < NUM_FRUSTUM_CORNERS; j++) {
      // Get the frustum coordinate in world space
       vec4 vW = frustum_corners[j];

      // Transform the frustum coordinate from world to light space
      frustum_corners_l[j] = light_view_matrix * vW;

      minX = min(minX, frustum_corners_l[j].x);
      maxX = max(maxX, frustum_corners_l[j].x);
      minY = min(minY, frustum_corners_l[j].y);
      maxY = max(maxY, frustum_corners_l[j].y);
      minZ = min(minZ, frustum_corners_l[j].z);
      maxZ = max(maxZ, frustum_corners_l[j].z);
    }

    m_shadowOrthoProjInfo[i].r = maxX;
    m_shadowOrthoProjInfo[i].l = minX;
    m_shadowOrthoProjInfo[i].b = minY;
    m_shadowOrthoProjInfo[i].t = maxY;
    m_shadowOrthoProjInfo[i].f = -maxZ;
    m_shadowOrthoProjInfo[i].n = -minZ;
  }
}

void ShadowMap::gui(SDL_Window* window) {
  if (ImGui::CollapsingHeader("Cascading Shadow Map")) {
    ImGui::Checkbox("Use polygon offset", &this->use_polygon_offset);
    ImGui::DragFloat("Polygon offset factor", &this->polygon_offset_factor);
    ImGui::DragFloat("Polygon offset units", &this->polygon_offset_units);

    ImGui::Text("Debug");

    for (uint i = 0 ; i < NUM_CASCADES; i++) {
      ImGui::Image((void*)(intptr_t) shadow_maps[i], ImVec2(128, 128), ImVec2(0, 1), ImVec2(1, 0));

      float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
      ImGui::SameLine(0.0f, spacing);
    }

    ImGui::NewLine();
  }
}