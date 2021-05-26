#include "shadowmap.h"

ShadowMap::ShadowMap(void) {}

void ShadowMap::init(float z_near, float z_far) {
  cascade_splits[0] = z_near;
  cascade_splits[1] = 400.0f,
  cascade_splits[2] = 1000.0f;
  cascade_splits[3] = z_far;

  // Layered texture
  glGenTextures(NUM_CASCADES, &shadow_tex);
  glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_tex);
  glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH_COMPONENT32F, resolution, resolution, NUM_CASCADES);

  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);

  // Create the FBO
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);

  for (uint i = 0 ; i < NUM_CASCADES; i++) {
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_tex, 0, i);
  }

  glDrawBuffer(GL_NONE);

  bool isComplete = checkFramebufferComplete();

  // restore default FBO
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
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

  glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, shadow_tex, 0, cascade_index);
}

void ShadowMap::begin(GLuint tex, mat4 proj_matrix, mat4 light_view_matrix) {
  GLint shader_program = 0;
  glGetIntegerv(GL_CURRENT_PROGRAM, &shader_program);

  for (uint i = 0 ; i < NUM_CASCADES ; i++) {
    vec4 vView(0.0f, 0.0f, cascade_splits[i + 1], 1.0f);
    vec4 vClip = proj_matrix * vView;

    mat4 light_proj_matrix = getLightProjMatrix(i);

    gpu::setUniformSlow(shader_program, ("gCascadeEndClipSpace[" + std::to_string(i) + "]").c_str(), vClip.z);
    gpu::setUniformSlow(shader_program, ("gLightWVP[" + std::to_string(i) + "]").c_str(), light_proj_matrix * light_view_matrix);
  }

  glActiveTexture(tex);
  glBindTexture(GL_TEXTURE_2D_ARRAY, shadow_tex);
}

void ShadowMap::debugProjs(mat4 view_matrix, mat4 proj_matrix, mat4 light_view_matrix) {
  float fovy = 2.0 * atan(1.0 / proj_matrix[1][1]);
  float ar = proj_matrix[1][1] / proj_matrix[0][0];

  for (uint i = 0; i < NUM_CASCADES; i++) {
    mat4 proj = perspective(fovy, ar, cascade_splits[i], cascade_splits[i + 1]);

    OrthoProjInfo info = shadow_ortho_info[i];

    mat4 light_proj_matrix = ortho(info.l, info.r, info.b, info.t, info.n, info.f);

    DebugDrawer::instance()->drawPerspectiveFrustum(view_matrix, proj, vec3(1, 0, 0));
    DebugDrawer::instance()->drawOrthographicFrustum(light_view_matrix, light_proj_matrix, vec3(1, 1, 0));
  }
}

void ShadowMap::calculateLightProjMatrices(mat4 view_matrix, mat4 light_view_matrix, int width, int height, float fovy) {
  mat4 view_inverse = inverse(view_matrix);

  float ar = width / (float)height;
  
  float tanHalfHFov = glm::tan(glm::radians(fovy / 2.0f)) * ar;
  float tanHalfVFov = glm::tan(glm::radians(fovy / 2.0));

  for (uint i = 0; i < NUM_CASCADES; i++) {
    float xn = cascade_splits[i] * tanHalfHFov;
    float xf = cascade_splits[i + 1] * tanHalfHFov;
    float yn = cascade_splits[i] * tanHalfVFov;
    float yf = cascade_splits[i + 1] * tanHalfVFov;

    vec4 frustum_corners[NUM_FRUSTUM_CORNERS] = {
        // near face
        view_inverse * vec4(xn, yn, -cascade_splits[i], 1.0), 
        view_inverse * vec4(-xn, yn, -cascade_splits[i], 1.0),
        view_inverse * vec4(xn, -yn, -cascade_splits[i], 1.0), 
        view_inverse * vec4(-xn, -yn, -cascade_splits[i], 1.0),

        // far face
        view_inverse * vec4(xf, yf, -cascade_splits[i + 1], 1.0), 
        view_inverse * vec4(-xf, yf, -cascade_splits[i + 1], 1.0),
        view_inverse * vec4(xf, -yf, -cascade_splits[i + 1], 1.0), 
        view_inverse * vec4(-xf, -yf, -cascade_splits[i + 1], 1.0)};

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

    shadow_ortho_info[i].r = maxX;
    shadow_ortho_info[i].l = minX;
    shadow_ortho_info[i].b = minY;
    shadow_ortho_info[i].t = maxY;
    shadow_ortho_info[i].f = -(maxZ + this->bias);
    shadow_ortho_info[i].n = -(minZ - this->bias);
  }
}

void ShadowMap::gui(SDL_Window* window) {
  if (ImGui::CollapsingHeader("Cascading Shadow Map")) {
    ImGui::Checkbox("Use polygon offset", &this->use_polygon_offset);
    ImGui::DragFloat("Polygon offset factor", &this->polygon_offset_factor);
    ImGui::DragFloat("Polygon offset units", &this->polygon_offset_units);
    ImGui::DragFloat("Bias", &this->bias);

    ImGui::Text("Debug");

    // for (uint i = 0 ; i < NUM_CASCADES; i++) {
    //   ImGui::Image((void*)(intptr_t) shadow_maps[i], ImVec2(128, 128), ImVec2(0, 1), ImVec2(1, 0));

    //   float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
    //   ImGui::SameLine(0.0f, spacing);
    // }

    ImGui::NewLine();
  }
}


mat4 ShadowMap::getLightProjMatrix(uint cascade_index) {
  OrthoProjInfo info = shadow_ortho_info[cascade_index];

  return ortho(info.l, info.r, info.b, info.t, info.n, info.f);
}