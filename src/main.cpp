#include <imgui.h>
#include <imgui_internal.h>

#include <ImGuizmo.h>
#include <glad/glad.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include "gpu.h"
using namespace glm;

#include "camera.h"
#include "core.h"
#include "debug.h"
#include "fbo.h"
#include "hdr.h"
#include "model.h"
#include "shadowmap.h"
#include "terrain.h"
#include "water.h"

constexpr vec3 worldUp(0.0f, 1.0f, 0.0f);

struct App {
  struct Window {
    SDL_Window* handle = nullptr;
    int width = 0;
    int height = 0;
  } window;

  struct Input {
    ivec2 prev_mouse_pos = {-1, -1};
    bool is_mouse_dragging = false;
  } input;

  Camera camera;

  struct EnvironmentMap {
    float multiplier = 1.5f;
    const std::string base_name = "001";
    GLuint environmentMap, irradianceMap, reflectionMap;
  } environment_map;

  struct Models {
    gpu::Model* fighter = nullptr;
    gpu::Model* landingpad = nullptr;
    gpu::Model* sphere = nullptr;
  } models;

  Terrain terrain;
  ShadowMap shadow_map;
  Water water;

  float current_time = 0.0f;
  float previous_time = 0.0f;
  float delta_time = 0.0f;

  bool show_ui = false;

  GLuint shader_program;         // Shader for rendering the final image
  GLuint simple_shader_program;  // Shader used to draw the shadow map
  GLuint background_program;
  GLuint debug_program;
  GLuint postfx_program;

  bool fighter_draggable = false;
  mat4 fighter_model_matrix = translate(vec3(0, 500, 0));

  mat4 static_camera_proj;
  mat4 static_camera_view;
  vec3 static_camera_pos;
  bool static_set = false;

  FboInfo postfx_fbo;

  struct DrawScene {};

  void loadShaders(bool is_reload) {
    GLuint shader = gpu::loadShaderProgram("resources/shaders/simple.vert",
                                           "resources/shaders/simple.frag", is_reload);
    if (shader != 0) simple_shader_program = shader;

    shader = gpu::loadShaderProgram("resources/shaders/background.vert",
                                    "resources/shaders/background.frag", is_reload);
    if (shader != 0) background_program = shader;

    shader = gpu::loadShaderProgram("resources/shaders/shading.vert",
                                    "resources/shaders/shading.frag", is_reload);
    if (shader != 0) shader_program = shader;

    shader = gpu::loadShaderProgram("resources/shaders/debug.vert", "resources/shaders/debug.frag",
                                    is_reload);
    if (shader != 0) debug_program = shader;

    shader = gpu::loadShaderProgram("resources/shaders/postfx.vert",
                                    "resources/shaders/postfx.frag", is_reload);
    if (shader != 0) postfx_program = shader;

    this->terrain.loadShader(is_reload);
    this->water.loadShader(is_reload);

    DebugDrawer::instance()->loadShaders(is_reload);
  }

  void init() {
    window.handle = gpu::init_window_SDL("OpenGL Project");

    glEnable(GL_DEPTH_TEST);  // enable Z-buffering
    glEnable(GL_CULL_FACE);   // enables backface culling

    loadShaders(false);

    // Load models and set up model matrices
    models.fighter = gpu::loadModelFromOBJ("resources/models/NewShip.obj");
    models.landingpad = gpu::loadModelFromOBJ("resources/models/landingpad.obj");
    models.sphere = gpu::loadModelFromOBJ("resources/models/sphere.obj");

    // Load environment map
    {
      const int roughnesses = 8;
      std::vector<std::string> filenames;
      filenames.reserve(roughnesses);
      for (int i = 0; i < roughnesses; i++) {
        filenames.push_back("resources/envmaps/" + environment_map.base_name + "_dl_"
                            + std::to_string(i) + ".hdr");
      }
      environment_map.reflectionMap = gpu::loadHdrMipmapTexture(filenames);
      environment_map.environmentMap
          = gpu::loadHdrTexture("resources/envmaps/" + environment_map.base_name + ".hdr");
      environment_map.irradianceMap = gpu::loadHdrTexture(
          "resources/envmaps/" + environment_map.base_name + "_irradiance.hdr");
    }

    shadow_map.init(camera.projection.near, camera.projection.far);
    terrain.init();
    water.init();
  }

  void deinit() {
    terrain.deinit();
    water.deinit();
    shadow_map.deinit();
    gpu::freeModel(models.fighter);
    gpu::freeModel(models.landingpad);
    gpu::freeModel(models.sphere);

    glDeleteFramebuffers(1, &postfx_fbo.framebufferId);
  }

  void debugDrawLight(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
                      const glm::vec3& worldSpaceLightPos) {
    mat4 modelMatrix = glm::translate(worldSpaceLightPos);
    glUseProgram(shader_program);
    gpu::setUniformSlow(shader_program, "modelViewProjectionMatrix",
                        projectionMatrix * viewMatrix * modelMatrix);
    gpu::setUniformSlow(shader_program, "modelViewMatrix", viewMatrix * modelMatrix);
    gpu::setUniformSlow(shader_program, "normalMatrix",
                        inverse(transpose(viewMatrix * modelMatrix)));
    gpu::render(models.sphere);
  }

  void drawBackground(const mat4& viewMatrix, const mat4& projectionMatrix) {
    glUseProgram(background_program);
    gpu::setUniformSlow(background_program, "environment_multiplier", environment_map.multiplier);
    gpu::setUniformSlow(background_program, "inv_PV", inverse(projectionMatrix * viewMatrix));
    gpu::setUniformSlow(background_program, "camera_pos", camera.position);
    gpu::drawFullScreenQuad();
  }

  void shadowPass(GLuint current_program, const mat4& view_matrix, const mat4& proj_matrix,
                  const mat4& light_view_matrix) {
    shadow_map.calculateLightProjMatrices(view_matrix, light_view_matrix, window.width,
                                          window.height, camera.projection.fovy);

    glUseProgram(current_program);

    for (uint i = 0; i < NUM_CASCADES; i++) {
      // Bind and clear the current cascade
      shadow_map.bindWrite(i);
      glClear(GL_DEPTH_BUFFER_BIT);
      glViewport(0, 0, shadow_map.resolution, shadow_map.resolution);

      mat4 light_proj_matrix = shadow_map.getLightProjMatrix(i);

      // Terrain
      terrain.begin(true);
      terrain.setPolyOffset(shadow_map.polygon_offset_factor, shadow_map.polygon_offset_units);

      glDisable(GL_CULL_FACE);
      terrain.render(light_proj_matrix, light_view_matrix, vec3(0), mat4(), water.height);
      glEnable(GL_CULL_FACE);

      terrain.setPolyOffset(0, 0);

      glUseProgram(current_program);

      // Fighter
      gpu::setUniformSlow(current_program, "modelViewProjectionMatrix",
                          light_proj_matrix * light_view_matrix * fighter_model_matrix);
      gpu::setUniformSlow(current_program, "modelViewMatrix",
                          light_view_matrix * fighter_model_matrix);
      gpu::setUniformSlow(current_program, "normalMatrix",
                          inverse(transpose(light_view_matrix * fighter_model_matrix)));

      gpu::render(models.fighter);
    }
  }

  void renderPass(GLuint current_program, const mat4& viewMatrix, const mat4& projMatrix,
                  const mat4& lightViewMatrix) {
    glUseProgram(current_program);

    // Environment
    gpu::setUniformSlow(current_program, "environment_multiplier", environment_map.multiplier);

    // camera
    gpu::setUniformSlow(current_program, "viewInverse", inverse(viewMatrix));

    // Terrain
    mat4 lightMatrix = mat4(1);

    terrain.begin(false);

    // Bind shadow map textures
    shadow_map.begin(10, projMatrix, lightViewMatrix);

    terrain.render(projMatrix, viewMatrix, camera.position, lightMatrix, water.height);
    water.render(window.width, window.height, current_time, projMatrix, viewMatrix, camera.position,
                 camera.projection.near, camera.projection.far);

    glUseProgram(current_program);

    // Fighter
    gpu::setUniformSlow(current_program, "modelViewProjectionMatrix",
                        projMatrix * viewMatrix * fighter_model_matrix);
    gpu::setUniformSlow(current_program, "modelViewMatrix", viewMatrix * fighter_model_matrix);
    gpu::setUniformSlow(current_program, "normalMatrix",
                        inverse(transpose(viewMatrix * fighter_model_matrix)));

    gpu::render(models.fighter);
  }

  void update(void) {
    terrain.update(delta_time, current_time);
  }

  void display(void) {
    SDL_GetWindowSize(window.handle, &window.width, &window.height);

    if (postfx_fbo.width != window.width || postfx_fbo.height != window.height) {
      postfx_fbo.resize(window.width, window.height);
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window.handle);
    ImGui::NewFrame();

    ImGuizmo::BeginFrame();
    ImGuizmo::SetRect(0, 0, window.width, window.height);

    // setup matrices
    mat4 projMatrix = camera.getProjMatrix(window.width, window.height);
    mat4 viewMatrix = camera.getViewMatrix();

    if (fighter_draggable) {
      ImGuizmo::Manipulate(&viewMatrix[0][0], &projMatrix[0][0], ImGuizmo::TRANSLATE,
                           ImGuizmo::LOCAL, &fighter_model_matrix[0][0], nullptr, nullptr);
    }

    if (!static_set) {
      static_camera_proj = projMatrix;
      static_camera_view = viewMatrix;
      static_camera_pos = camera.position;
      static_set = true;
    }

    // Draw from cascaded light sources
    mat4 lightViewMatrix = lookAt(vec3(0), -terrain.sun.direction, worldUp);
    shadowPass(shader_program, viewMatrix, projMatrix, lightViewMatrix);

    // Bind the environment map(s) to unused texture units
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, environment_map.environmentMap);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, environment_map.irradianceMap);
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, environment_map.reflectionMap);
    glActiveTexture(GL_TEXTURE0);

    // Draw from camera
    glBindFramebuffer(GL_FRAMEBUFFER, postfx_fbo.framebufferId);
    glViewport(0, 0, postfx_fbo.width, postfx_fbo.height);
    glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawBackground(viewMatrix, projMatrix);
    renderPass(shader_program, viewMatrix, projMatrix, lightViewMatrix);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glUseProgram(postfx_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, postfx_fbo.colorTextureTargets[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, postfx_fbo.depthBuffer);

    gpu::setUniformSlow(postfx_program, "viewMatrix", viewMatrix);
    gpu::setUniformSlow(postfx_program, "projMatrix", projMatrix);
    gpu::setUniformSlow(postfx_program, "currentTime", current_time);
    gpu::setUniformSlow(postfx_program, "water.height", water.height);
    gpu::setUniformSlow(postfx_program, "sun.direction", terrain.sun.direction);
    gpu::setUniformSlow(postfx_program, "sun.color", terrain.sun.color);
    gpu::setUniformSlow(postfx_program, "sun.intensity", terrain.sun.intensity);
    gpu::setUniformSlow(postfx_program, "postfx.z_near", camera.projection.near);
    gpu::setUniformSlow(postfx_program, "postfx.z_far", camera.projection.far);

    gpu::drawFullScreenQuad();
  }

  bool handleEvents(void) {
    // check events (keyboard among other)
    SDL_Event event;
    bool quitEvent = false;

    ImGuiIO& io = ImGui::GetIO();

    io.ConfigWindowsMoveFromTitleBarOnly = true;

    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT
          || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE)) {
        quitEvent = true;
      }
      if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g) {
        show_ui = !show_ui;
      }
      if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_r) {
        loadShaders(true);
      }
      if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
          && !io.WantCaptureMouse) {
        input.is_mouse_dragging = true;
        int x, y;
        SDL_GetMouseState(&x, &y);
        input.prev_mouse_pos.x = x;
        input.prev_mouse_pos.y = y;
      }

      if ((SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON(SDL_BUTTON_LEFT)) == 0U) {
        input.is_mouse_dragging = false;
      }

      if (event.type == SDL_MOUSEMOTION && input.is_mouse_dragging) {
        // More info at https://wiki.libsdl.org/SDL_MouseMotionEvent
        int delta_x = event.motion.x - input.prev_mouse_pos.x;
        int delta_y = event.motion.y - input.prev_mouse_pos.y;
        camera.drag_event(delta_x, delta_y, delta_time);
        input.prev_mouse_pos.x = event.motion.x;
        input.prev_mouse_pos.y = event.motion.y;
      }
    }

    // check keyboard state (which keys are still pressed)
    const uint8_t* state = SDL_GetKeyboardState(nullptr);
    camera.key_event(state, delta_time);

    if (state[SDL_SCANCODE_C]) {
      static_camera_proj
          = perspective(radians(camera.projection.fovy), float(window.width) / float(window.height),
                        camera.projection.near, camera.projection.far);

      static_camera_view = camera.getViewMatrix();
      static_camera_pos = camera.position;
    }

    return quitEvent;
  }

  void gui() {
    if (show_ui) {
      ImGuizmo::SetDrawlist();

      float window_width = (float)ImGui::GetWindowWidth();
      float window_height = (float)ImGui::GetWindowHeight();
      ImVec2 window_pos = ImGui::GetWindowPos();

      ImGuizmo::SetRect(window_pos.x, window_pos.y, window_width, window_height);

      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                  1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

      if (ImGui::Button("Reload Shaders")) {
        loadShaders(true);
      }

      ImGui::Checkbox("Fighter Draggable", &fighter_draggable);

      if (ImGui::CollapsingHeader("Camera")) {
        camera.gui();
        ImGui::DragFloat("Near Projection", &camera.projection.near, 0.02, 0.2);
        ImGui::DragFloat("Far Projection", &camera.projection.far, 200.0, 1000.0);
      }

      // Light and environment map
      // if (ImGui::CollapsingHeader("Light sources")) {
      //   ImGui::SliderFloat("Environment multiplier", &environment_map.multiplier, 0.0f, 10.0f);
      //   ImGui::ColorEdit3("Point light color", &light.color.x);
      //   ImGui::SliderFloat("Point light intensity multiplier", &light.intensity, 0.0f, 10000.0f,
      //                      "%.3f", ImGuiSliderFlags_Logarithmic);
      // }

      terrain.gui(&camera);
      shadow_map.gui(window.handle);
      water.gui();

      if (ImGui::CollapsingHeader("Post FX")) {
        ImGui::Image((void*)(intptr_t)postfx_fbo.colorTextureTargets[0], ImVec2(252, 252),
                     ImVec2(0, 1), ImVec2(1, 0));
      }
    }

    // Render the GUI.
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  }
};

int main(int argc, char* argv[]) {
  App* app = new App();

  app->init();
  defer(app->deinit());

  bool stopRendering = false;
  auto startTime = std::chrono::system_clock::now();

  while (!stopRendering) {
    // update currentTime
    std::chrono::duration<float> timeSinceStart = std::chrono::system_clock::now() - startTime;
    app->previous_time = app->current_time;
    app->current_time = timeSinceStart.count();
    app->delta_time = app->current_time - app->previous_time;

    // update logic
    app->update();

    // render to window
    app->display();

    // Render overlay GUI.
    app->gui();

    // Swap front and back buffer. This frame will now been displayed.
    SDL_GL_SwapWindow(app->window.handle);

    // check events (keyboard among other)
    stopRendering = app->handleEvents();
  }

  return 0;
}
