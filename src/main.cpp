#include <glad/glad.h>
#include <imgui.h>

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
#include "fbo.h"
#include "hdr.h"
#include "model.h"
#include "terrain.h"
#include "shadowmap.h"
#include "debug.h"

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

  struct Projection {
    float far = 2000.0f;
    float near = 5.0f;
  } projection;

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

  float current_time = 0.0f;
  float previous_time = 0.0f;
  float delta_time = 0.0f;

  bool show_ui = false;

  GLuint shader_program;         // Shader for rendering the final image
  GLuint simple_shader_program;  // Shader used to draw the shadow map
  GLuint background_program;
  GLuint debug_program;

  mat4 fighter_model_matrix = mat4(1.0f);

  mat4 static_camera_proj;
  mat4 static_camera_view;
  vec3 static_camera_pos;
  bool static_set = false;

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

    shader = gpu::loadShaderProgram("resources/shaders/debug.vert",
                                    "resources/shaders/debug.frag", is_reload);
    if (shader != 0) debug_program = shader;

    this->terrain.loadShader(is_reload);

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

    shadow_map.init(projection.near, projection.far);
    terrain.init();
  }

  void deinit() {
    gpu::freeModel(models.fighter);
    gpu::freeModel(models.landingpad);
    gpu::freeModel(models.sphere);
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

  void shadowPass(GLuint currentShaderProgram, const mat4& view_matrix, const mat4& proj_matrix,
                  const mat4& light_view_matrix) {
    shadow_map.calculateLightProjMatrices(view_matrix, light_view_matrix, window.width, window.height,
                              45.0f);

    glUseProgram(currentShaderProgram);

    for (uint i = 0 ; i < NUM_CASCADES ; i++) {
      // Bind and clear the current cascade
      shadow_map.bindWrite(i);
      glClear(GL_DEPTH_BUFFER_BIT);
      glViewport(0, 0, shadow_map.resolution, shadow_map.resolution);

      mat4 light_proj_matrix = shadow_map.getLightProjMatrix(i);

      // Terrain
      terrain.render(light_proj_matrix, light_view_matrix, vec3(0), mat4());

      // Fighter
      gpu::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
                          light_proj_matrix * light_view_matrix * fighter_model_matrix);
      gpu::setUniformSlow(currentShaderProgram, "modelViewMatrix", light_view_matrix * fighter_model_matrix);
      gpu::setUniformSlow(currentShaderProgram, "normalMatrix",
                          inverse(transpose(light_view_matrix * fighter_model_matrix)));

      gpu::render(models.fighter);
    }
  }

  void renderPass(GLuint currentShaderProgram, const mat4& viewMatrix, const mat4& projectionMatrix,
                 const mat4& lightViewMatrix) {
    glUseProgram(currentShaderProgram);

    // Environment
    gpu::setUniformSlow(currentShaderProgram, "environment_multiplier", environment_map.multiplier);

    // camera
    gpu::setUniformSlow(currentShaderProgram, "viewInverse", inverse(viewMatrix));

    // Bind shadow map textures
    shadow_map.bindRead(GL_TEXTURE10, GL_TEXTURE11, GL_TEXTURE12);
    shadow_map.setUniforms(terrain.shader_program, projectionMatrix, lightViewMatrix);

    // Terrain
    mat4 lightMatrix = mat4(1);
    terrain.render(projectionMatrix, viewMatrix, camera.position, lightMatrix);

    // Fighter
    gpu::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
                        projectionMatrix * viewMatrix * fighter_model_matrix);
    gpu::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * fighter_model_matrix);
    gpu::setUniformSlow(currentShaderProgram, "normalMatrix",
                        inverse(transpose(viewMatrix * fighter_model_matrix)));

    gpu::render(models.fighter);
  }

  bool rayPlaneIntersection(vec3 ray_origin, vec3 ray_dir, vec3 point_on_plane, vec3 plane_normal, float &hit_depth) {
      float denom = dot(plane_normal, ray_dir);

      if (denom < -1e-6) {
          float t = dot(point_on_plane - ray_origin, plane_normal) / denom;

          hit_depth = t;

          return true;
      }

      return false;
  } 

  void display(void) {
    SDL_GetWindowSize(window.handle, &window.width, &window.height);

    // setup matrices
    mat4 projMatrix = perspective(radians(45.0f), float(window.width) / float(window.height),
                                  projection.near, projection.far);

    mat4 viewMatrix = camera.getViewMatrix();

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
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window.width, window.height);
    glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawBackground(viewMatrix, projMatrix);

    renderPass(shader_program, viewMatrix, projMatrix, lightViewMatrix);
  }

  bool handleEvents(void) {
    // check events (keyboard among other)
    SDL_Event event;
    bool quitEvent = false;

    ImGuiIO& io = ImGui::GetIO();

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
      static_camera_proj = perspective(radians(45.0f), float(window.width) / float(window.height),
                                    projection.near, projection.far);

      static_camera_view = camera.getViewMatrix();
      static_camera_pos = camera.position;
    }

    return quitEvent;
  }

  void gui() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window.handle);
    ImGui::NewFrame();

    if (show_ui) {
      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                  1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

      if (ImGui::Button("Reload Shaders")) {
        loadShaders(true);
      }

      if (ImGui::CollapsingHeader("Camera")) {
        camera.gui();
        ImGui::DragFloat("Near Projection", &projection.near, 0.02, 0.2);
        ImGui::DragFloat("Far Projection", &projection.far, 200.0, 1000.0);
      }

      // Light and environment map
      // if (ImGui::CollapsingHeader("Light sources")) {
      //   ImGui::SliderFloat("Environment multiplier", &environment_map.multiplier, 0.0f, 10.0f);
      //   ImGui::ColorEdit3("Point light color", &light.color.x);
      //   ImGui::SliderFloat("Point light intensity multiplier", &light.intensity, 0.0f, 10000.0f,
      //                      "%.3f", ImGuiSliderFlags_Logarithmic);
      // }

      terrain.gui(window.handle);

      shadow_map.gui(window.handle);
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
