#include <glad/glad.h>
#include <imgui.h>
#include <imgui_internal.h>

// clang-format: off
#include <ImGuizmo.h>
// clang-format: on

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
#include "postfx.h"
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
    gpu::Model* material_test = nullptr;
    gpu::Model* sphere = nullptr;
  } models;

  Terrain terrain;
  ShadowMap shadow_map;
  Water water;
  PostFX postfx;

  float current_time = 0.0f;
  float previous_time = 0.0f;
  float delta_time = 0.0f;

  bool show_ui = false;

  GLuint shader_program;         // Shader for rendering the final image
  GLuint simple_shader_program;  // Shader used to draw the shadow map
  GLuint background_program;
  GLuint debug_program;

  bool fighter_draggable = false;
  mat4 fighter_model_matrix = translate(vec3(0, 500, 0));
  mat4 material_test_matrix = translate(vec3(50, 500, 0));

  struct DebugLight {
    mat4 model_matrix = glm::translate(vec3(50.0, 505, 0.0));
    vec3 color = vec3(1.0);
    float intensity = 30.0;
  } debug_light;

  mat4 static_camera_proj;
  mat4 static_camera_view;
  vec3 static_camera_pos;
  vec3 static_camera_world_pos;
  bool static_camera_enabled;
  bool static_camera_set = false;

  struct DrawScene {};

  void loadShaders(bool is_reload) {
    GLuint shader = gpu::loadShaderProgram("resources/shaders/simple.vert",
                                           "resources/shaders/simple.frag", is_reload);
    if (shader != 0) simple_shader_program = shader;

    shader = gpu::loadShaderProgram("resources/shaders/background.vert",
                                    "resources/shaders/background.frag", is_reload);
    if (shader != 0) background_program = shader;

    std::array<ShaderInput, 2> program_shading({
        ShaderInput{"resources/shaders/shading.vert", GL_VERTEX_SHADER},
        ShaderInput{"resources/shaders/shading.frag", GL_FRAGMENT_SHADER},
    });
    shader = loadShaderProgram(program_shading, is_reload);
    if (shader != 0) shader_program = shader;

    shader = gpu::loadShaderProgram("resources/shaders/debug.vert", "resources/shaders/debug.frag",
                                    is_reload);
    if (shader != 0) debug_program = shader;

    this->terrain.loadShader(is_reload);
    this->water.loadShader(is_reload);
    this->postfx.loadShader(is_reload);

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
    models.material_test = gpu::loadModelFromOBJ("resources/models/materialtest.obj");
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

    shadow_map.init(camera.projection);
    terrain.init();
    water.init();
    postfx.init();
  }

  void deinit() {
    terrain.deinit();
    water.deinit();
    shadow_map.deinit();
    postfx.deinit();

    gpu::freeModel(models.fighter);
    gpu::freeModel(models.landingpad);
    gpu::freeModel(models.material_test);
    gpu::freeModel(models.sphere);
  }

  void debugDrawLight(const glm::mat4& view_matrix, const glm::mat4& proj_matrix,
                      const glm::vec3& world_space_light_pos) {
    mat4 modelMatrix = glm::translate(world_space_light_pos);
    glUseProgram(shader_program);
    gpu::setUniformSlow(shader_program, "modelViewProjectionMatrix",
                        proj_matrix * view_matrix * modelMatrix);
    gpu::setUniformSlow(shader_program, "modelViewMatrix", view_matrix * modelMatrix);
    gpu::setUniformSlow(shader_program, "normalMatrix",
                        inverse(transpose(view_matrix * modelMatrix)));
    gpu::render(models.sphere);
  }

  void drawBackground(const mat4& view_matrix, const mat4& proj_matrix) {
    glUseProgram(background_program);
    gpu::setUniformSlow(background_program, "environment_multiplier", environment_map.multiplier);
    gpu::setUniformSlow(background_program, "inv_PV", inverse(proj_matrix * view_matrix));
    gpu::setUniformSlow(background_program, "camera_pos", camera.getWorldPos());
    gpu::drawFullScreenQuad();
  }

  void shadowPass(GLuint current_program, const mat4& view_matrix, const mat4& proj_matrix,
                  const mat4& light_view_matrix) {
    shadow_map.calculateLightProjMatrices(view_matrix, light_view_matrix, window.width,
                                          window.height, camera.projection.fovy);

    vec3 cam_pos = static_camera_enabled ? static_camera_world_pos : camera.getWorldPos();
    vec3 center = static_camera_enabled ? static_camera_pos : camera.position;

    glUseProgram(current_program);

    for (uint i = 0; i < NUM_CASCADES; i++) {
      // Bind and clear the current cascade
      shadow_map.bindWrite(i);
      glClear(GL_DEPTH_BUFFER_BIT);
      glViewport(0, 0, shadow_map.resolution, shadow_map.resolution);

      mat4 light_proj_matrix = shadow_map.shadow_projections[i];

      // Terrain
      terrain.begin(true);

      glDisable(GL_CULL_FACE);
      terrain.render(light_proj_matrix, light_view_matrix, center, cam_pos, mat4(), water.height,
                     environment_map.multiplier);
      glEnable(GL_CULL_FACE);

      glUseProgram(current_program);

      // vec3 light_pos = -terrain.sun.direction * 500.F;
      vec4 view_light_pos = view_matrix * vec4(vec3(debug_light.model_matrix[3]), 1);

      ImGuizmo::Manipulate(&view_matrix[0][0], &proj_matrix[0][0], ImGuizmo::TRANSLATE,
                           ImGuizmo::WORLD, &debug_light.model_matrix[0][0], nullptr, nullptr);
      // ImGuizmo::DrawCubes(&view_matrix[0][0], &proj_matrix[0][0], &debug_light_matrix[0][0], 1);
      // ImGuizmo::DrawCubes(&view_matrix[0][0], &proj_matrix[0][0], &(glm::translate(vec3(0, 500,
      // 0)) * glm::scale(terrain.sun.direction * 20.F))[0][0], 1);

      gpu::setUniformSlow(current_program, "viewSpaceLightPosition", vec3(view_light_pos));

      // Fighter
      gpu::setUniformSlow(current_program, "modelViewProjectionMatrix",
                          light_proj_matrix * light_view_matrix * fighter_model_matrix);
      gpu::setUniformSlow(current_program, "modelViewMatrix",
                          light_view_matrix * fighter_model_matrix);
      gpu::setUniformSlow(current_program, "normalMatrix",
                          inverse(transpose(light_view_matrix * fighter_model_matrix)));

      gpu::render(models.fighter);

      // Material test
      gpu::setUniformSlow(current_program, "modelViewProjectionMatrix",
                          light_proj_matrix * light_view_matrix * material_test_matrix);
      gpu::setUniformSlow(current_program, "modelViewMatrix",
                          light_view_matrix * material_test_matrix);
      gpu::setUniformSlow(current_program, "normalMatrix",
                          inverse(transpose(light_view_matrix * material_test_matrix)));
      gpu::render(models.material_test);
    }
  }

  void renderPass(GLuint current_program, const mat4& view_matrix, const mat4& proj_matrix,
                  const mat4& light_view_matrix) {
    glUseProgram(current_program);

    vec3 cam_pos = static_camera_enabled ? static_camera_world_pos : camera.getWorldPos();
    vec3 center = static_camera_enabled ? static_camera_pos : camera.position;

    // Environment
    gpu::setUniformSlow(current_program, "environment_multiplier", environment_map.multiplier);

    // camera
    gpu::setUniformSlow(current_program, "viewInverse", inverse(view_matrix));

    // Terrain
    mat4 lightMatrix = mat4(1);

    terrain.begin(false);

    // Bind shadow map textures
    shadow_map.begin(10, camera.projection, proj_matrix, light_view_matrix);

    terrain.render(proj_matrix, view_matrix, center, cam_pos, lightMatrix, water.height,
                   environment_map.multiplier);

    glUseProgram(current_program);

    // Fighter
    gpu::setUniformSlow(current_program, "modelViewProjectionMatrix",
                        proj_matrix * view_matrix * fighter_model_matrix);
    gpu::setUniformSlow(current_program, "modelViewMatrix", view_matrix * fighter_model_matrix);
    gpu::setUniformSlow(current_program, "normalMatrix",
                        inverse(transpose(view_matrix * fighter_model_matrix)));

    gpu::render(models.fighter);

    // Material test
    gpu::setUniformSlow(current_program, "environment_multiplier", environment_map.multiplier);
    gpu::setUniformSlow(current_program, "point_light_color", debug_light.color);
    gpu::setUniformSlow(current_program, "point_light_intensity_multiplier", debug_light.intensity);
    gpu::setUniformSlow(current_program, "modelViewProjectionMatrix",
                        proj_matrix * view_matrix * material_test_matrix);
    gpu::setUniformSlow(current_program, "modelViewMatrix", view_matrix * material_test_matrix);
    gpu::setUniformSlow(current_program, "normalMatrix",
                        inverse(transpose(view_matrix * material_test_matrix)));
    gpu::render(models.material_test);

    water.render(&terrain, window.width, window.height, current_time, proj_matrix, view_matrix,
                 center, camera.projection, environment_map.multiplier);
  }

  void update(void) { terrain.update(delta_time, current_time); }

  void display(void) {
    SDL_GetWindowSize(window.handle, &window.width, &window.height);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window.handle);
    ImGui::NewFrame();

    ImGuizmo::BeginFrame();
    ImGuizmo::SetRect(0, 0, window.width, window.height);

    // setup matrices
    mat4 proj_matrix = camera.getProjMatrix(window.width, window.height);
    mat4 view_matrix = camera.getViewMatrix();

    if (fighter_draggable) {
      ImGuizmo::Manipulate(&view_matrix[0][0], &proj_matrix[0][0], ImGuizmo::TRANSLATE,
                           ImGuizmo::LOCAL, &fighter_model_matrix[0][0], nullptr, nullptr);
    }

    if (!static_camera_set && static_camera_enabled) {
      static_camera_proj = camera.getProjMatrix(window.width, window.height);
      static_camera_view = view_matrix;
      static_camera_world_pos = camera.getWorldPos();
      static_camera_pos = camera.position;
      static_camera_set = true;
    }

    // Draw from cascaded light sources
    mat4 lightViewMatrix = lookAt(vec3(0), -terrain.sun.direction, worldUp);

    // vec3 look = -terrain.sun.direction;
    // mat4 lightViewMatrix = inverse(
    //     mat4(
    //          1,       0,        0,      0,
    //          0,       0,       -1,      0,
    //         -look.x, -look.y, look.z, 0,
    //          0,       1000,     0,       1
    //     )
    // );

    mat4 cam_proj_matrix = static_camera_enabled ? static_camera_proj : proj_matrix;
    mat4 cam_view_matrix = static_camera_enabled ? static_camera_view : view_matrix;

    shadowPass(shader_program, cam_view_matrix, proj_matrix, lightViewMatrix);

    // Bind the environment map(s) to unused texture units
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, environment_map.environmentMap);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, environment_map.irradianceMap);
    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, environment_map.reflectionMap);
    glActiveTexture(GL_TEXTURE0);

    // Draw into postfx FBO
    postfx.bind(window.width, window.height);

    glBindFramebuffer(GL_FRAMEBUFFER, postfx.screen_fbo.framebufferId);
    glViewport(0, 0, postfx.screen_fbo.width, postfx.screen_fbo.height);
    glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawBackground(view_matrix, proj_matrix);
    renderPass(shader_program, view_matrix, proj_matrix, lightViewMatrix);

    if (shadow_map.debug_show_projections) {
      DebugDrawer::instance()->setCamera(view_matrix, proj_matrix);
      DebugDrawer::instance()->drawLine(vec3(0), vec3(0, 500, 0), vec3(1, 0, 0));
      shadow_map.debugProjs(cam_view_matrix, cam_proj_matrix, lightViewMatrix);
    } else if (static_camera_enabled) {
      DebugDrawer::instance()->setCamera(view_matrix, proj_matrix);
      DebugDrawer::instance()->drawPerspectiveFrustum(static_camera_view, static_camera_proj,
                                                      vec3(1, 0, 0));
    }

    postfx.unbind();
    postfx.render(camera.projection, view_matrix, proj_matrix, current_time, &water, &terrain.sun);
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
      static_camera_world_pos = camera.getWorldPos();
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
        ImGui::Checkbox("Static camera [C]", &static_camera_enabled);

        camera.gui();
      }

      // Light and environment map
      if (ImGui::CollapsingHeader("Light sources")) {
        ImGui::SliderFloat("Environment multiplier", &environment_map.multiplier, 0.0f, 10.0f);
        ImGui::ColorEdit3("Point light color", &debug_light.color.x);
        ImGui::SliderFloat("Point light intensity multiplier", &debug_light.intensity, 0.0f,
                           10000.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
      }

      terrain.gui(&camera);
      shadow_map.gui(window.handle);
      water.gui();
      postfx.gui();
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
