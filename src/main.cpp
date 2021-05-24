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

  struct Light {
    vec3 position = {0, 0, 0};
    vec3 color = vec3(1.f, 1.f, 1.f);
    float intensity = 10000.0f;
  } light;

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
  Water water;

  float current_time = 0.0f;
  float previous_time = 0.0f;
  float delta_time = 0.0f;

  bool show_ui = false;

  GLuint shader_program;         // Shader for rendering the final image
  GLuint simple_shader_program;  // Shader used to draw the shadow map
  GLuint background_program;

  mat4 fighter_model_matrix = mat4(1.0f);

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

    this->terrain.loadShader(true);
    this->water.loadShader(true);
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

    terrain.init();
    water.init();
  }

  void deinit() {
    terrain.deinit();
    water.deinit();
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

  void drawScene(GLuint currentShaderProgram, const mat4& viewMatrix, const mat4& projectionMatrix,
                 const mat4& lightViewMatrix, const mat4& lightProjectionMatrix) {
    glUseProgram(currentShaderProgram);
    // Light source
    vec4 viewSpaceLightPosition = viewMatrix * vec4(light.position, 1.0f);
    gpu::setUniformSlow(currentShaderProgram, "point_light_color", light.color);
    gpu::setUniformSlow(currentShaderProgram, "point_light_intensity_multiplier", light.intensity);
    gpu::setUniformSlow(currentShaderProgram, "viewSpaceLightPosition",
                        vec3(viewSpaceLightPosition));
    gpu::setUniformSlow(currentShaderProgram, "viewSpaceLightDir",
                        normalize(vec3(viewMatrix * vec4(-light.position, 0.0f))));

    // Environment
    gpu::setUniformSlow(currentShaderProgram, "environment_multiplier", environment_map.multiplier);

    // camera
    gpu::setUniformSlow(currentShaderProgram, "viewInverse", inverse(viewMatrix));

    // Terrain
    terrain.render(projectionMatrix, viewMatrix, camera.position);

    // Fighter
    gpu::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
                        projectionMatrix * viewMatrix * fighter_model_matrix);
    gpu::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * fighter_model_matrix);
    gpu::setUniformSlow(currentShaderProgram, "normalMatrix",
                        inverse(transpose(viewMatrix * fighter_model_matrix)));

    gpu::render(models.fighter);
  }

  void display(void) {
    SDL_GetWindowSize(window.handle, &window.width, &window.height);

    // setup matrices
    mat4 projMatrix = perspective(radians(45.0f), float(window.width) / float(window.height),
                                  projection.near, projection.far);
    mat4 viewMatrix = camera.getViewMatrix();

    vec4 lightStartPosition = vec4(40.0f, 40.0f, 0.0f, 1.0f);
    light.position = vec3(rotate(current_time, worldUp) * lightStartPosition);
    mat4 lightViewMatrix = lookAt(light.position, vec3(0.0f), worldUp);
    mat4 lightProjMatrix = perspective(radians(45.0f), 1.0f, 25.0f, 100.0f);

    // Bind the environment map(s) to unused texture units
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, environment_map.environmentMap);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, environment_map.irradianceMap);
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, environment_map.reflectionMap);
    glActiveTexture(GL_TEXTURE0);

    // Draw from camera
    water.begin(window.width, window.height);
    {
#if 0
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, window.width, window.height);
    glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif

      drawBackground(viewMatrix, projMatrix);
      drawScene(shader_program, viewMatrix, projMatrix, lightViewMatrix, lightProjMatrix);
      debugDrawLight(viewMatrix, projMatrix, vec3(light.position));
    }
    water.end();
    water.render(current_time, projMatrix, viewMatrix, camera.position, projection.near);
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
      if (ImGui::CollapsingHeader("Light sources")) {
        ImGui::SliderFloat("Environment multiplier", &environment_map.multiplier, 0.0f, 10.0f);
        ImGui::ColorEdit3("Point light color", &light.color.x);
        ImGui::SliderFloat("Point light intensity multiplier", &light.intensity, 0.0f, 10000.0f,
                           "%.3f", ImGuiSliderFlags_Logarithmic);
      }

      terrain.gui(window.handle);
      water.gui();
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
