#include "shadowmap.h"

ShadowMap::ShadowMap(void) {}

void ShadowMap::init(float z_near, float z_far) {
  cascade_splits[0] = z_near;
  cascade_splits[1] = 500.0f, cascade_splits[2] = 1000.0f;
  cascade_splits[3] = z_far;

  // Layered texture
  glCreateTextures(GL_TEXTURE_2D_ARRAY, NUM_CASCADES, &shadow_tex);
  glTextureStorage3D(shadow_tex, 1, GL_DEPTH_COMPONENT32F, resolution, resolution, NUM_CASCADES);

  glTextureParameteri(shadow_tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTextureParameteri(shadow_tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTextureParameteri(shadow_tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTextureParameteri(shadow_tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTextureParameteri(shadow_tex, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
  glTextureParameteri(shadow_tex, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

  // Create the FBO
  glCreateFramebuffers(1, &fbo);
  glNamedFramebufferDrawBuffer(fbo, GL_NONE);

  for (uint i = 0; i < NUM_CASCADES; i++) {
    glNamedFramebufferTextureLayer(fbo, GL_DEPTH_ATTACHMENT, shadow_tex, 0, i);
  }

  bool isComplete = checkFramebufferComplete();
}

bool ShadowMap::checkFramebufferComplete() const {
  // Check that our FBO is correctly set up, this can fail if we have
  // incompatible formats in a buffer, or for example if we specify an
  // invalid drawbuffer, among things.
  GLenum status = glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER);
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

void ShadowMap::begin(uint tex_index, mat4 proj_matrix, mat4 light_view_matrix) {
  GLint shader_program = 0;
  glGetIntegerv(GL_CURRENT_PROGRAM, &shader_program);

  for (uint i = 0; i < NUM_CASCADES; i++) {
    vec4 vView(0.0f, 0.0f, cascade_splits[i + 1], 1.0f);
    vec4 vClip = proj_matrix * vView;

    mat4 light_proj_matrix = shadow_projections[i];

    gpu::setUniformSlow(shader_program, ("gCascadeEndClipSpace[" + std::to_string(i) + "]").c_str(),
                        -vClip.z);
    gpu::setUniformSlow(shader_program, ("gLightWVP[" + std::to_string(i) + "]").c_str(),
                        light_proj_matrix * light_view_matrix);
  }

  glBindTextureUnit(tex_index, shadow_tex);
}

void ShadowMap::calculateLightProjMatrices(mat4 view_matrix, mat4 light_view_matrix, int width,
                                           int height, float fovy) {
  mat4 view_inverse = inverse(view_matrix);

  float ar = width / (float)height;

  float tanHalfHFov = glm::tan(glm::radians(fovy / 2.0f)) * ar;
  float tanHalfVFov = glm::tan(glm::radians(fovy / 2.0));

  for (uint i = 0; i < NUM_CASCADES; i++) {
    float xn = cascade_splits[i] * tanHalfHFov;
    float xf = cascade_splits[i + 1] * tanHalfHFov;
    float yn = cascade_splits[i] * tanHalfVFov;
    float yf = cascade_splits[i + 1] * tanHalfVFov;

    vec4 frustum_corners[NUM_FRUSTUM_CORNERS]
        = {// near face
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

    float sizeX = maxX - minX;
    float sizeY = maxY - minY;

    float stepX = sizeX / resolution;
    float stepY = sizeY / resolution;

    shadow_ortho_info[i].r = floor(maxX / stepX) * stepX;
    shadow_ortho_info[i].l = floor(minX / stepX) * stepX;
    shadow_ortho_info[i].b = floor(minY / stepY) * stepY;
    shadow_ortho_info[i].t = floor(maxY / stepY) * stepY;
    shadow_ortho_info[i].f = -(maxZ + this->bias);
    shadow_ortho_info[i].n = -(minZ - this->bias);

    shadow_projections[i] = getLightProjMatrix(i);
  }
}

void ShadowMap::gui(SDL_Window* window) {
  if (ImGui::CollapsingHeader("Cascading Shadow Map")) {
    ImGui::DragFloat("Bias", &this->bias);

    ImGui::Text("Debug");

    ImGui::NewLine();
  }
}

mat4 ShadowMap::getLightProjMatrix(uint cascade_index) {
  OrthoProjInfo info = shadow_ortho_info[cascade_index];

  return ortho(info.l, info.r, info.b, info.t, info.n, info.f);
}

void ShadowMap::debugProjs(mat4 view_matrix, mat4 proj_matrix, mat4 light_view_matrix) {
  float fovy = 2.0 * atan(1.0 / proj_matrix[1][1]);
  float ar = proj_matrix[1][1] / proj_matrix[0][0];

  for (uint i = 0; i < NUM_CASCADES; i++) {
    mat4 proj = perspective(fovy, ar, cascade_splits[i], cascade_splits[i + 1]);

    mat4 light_proj_matrix = shadow_projections[i];

    DebugDrawer::instance()->drawPerspectiveFrustum(view_matrix, proj, vec3(1, 0, 0));
    DebugDrawer::instance()->drawOrthographicFrustum(light_view_matrix, shadow_ortho_info[i],
                                                     vec3((float)i / NUM_CASCADES, 1, 0));
  }
}

void ShadowMap::deinit() {
  glDeleteTextures(NUM_CASCADES, &shadow_tex);
  glDeleteFramebuffers(1, &this->fbo);
}