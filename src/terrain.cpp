#include "terrain.h"

void Terrain::init() {
  this->terrain_size = 2048.f;
  this->terrain_subdivision = 24;

  this->model_matrix = glm::mat4(1.0);

  this->material = gpu::Material{
      .m_name = std::string("Terrain"),
      .m_color = glm::vec3(0.0, 0.2, 0.0),
      .m_reflectivity = 0.0,
      .m_shininess = 0.0,
      .m_metalness = 0.0,
      .m_fresnel = 0.0,
      .m_emission = 0.0,
      .m_transparency = 0.0,
      .m_color_texture = {},
      .m_reflectivity_texture = {},
      .m_shininess_texture = {},
      .m_metalness_texture = {},
      .m_fresnel_texture = {},
      .m_emission_texture = {},
  };

  this->noises.push_back(TerrainSimplexNoise{
      .amplitude = 150.f,
      .frequency = .0005f,
  });

  this->tess_multiplier = 16.0f;

  // Construct a chunk
  this->init_mesh(false);

  // OpenGL Setup
  glPatchParameteri(GL_PATCH_VERTICES, 3);
  loadShader(false);

  this->rock_texture.load("resources/textures/", "rock.jpg", 3);
  this->grass_texture.load("resources/textures/", "grass.jpg", 3);
}

void Terrain::init_mesh(bool is_reload) {
  if (is_reload) {
    glDeleteBuffers(1, &this->positions_bo);
    glDeleteBuffers(1, &this->indices_bo);
    glDeleteVertexArrays(1, &this->vaob);
  }
  this->indices_count = gpu::createSubdividedPlane(this->terrain_size, this->terrain_subdivision, &this->vaob, &this->positions_bo, &this->indices_bo);
}

void Terrain::loadShader(bool is_reload) {
  if (is_reload) {
    glDeleteProgram(this->shader_program);
  }
  const std::array<ShaderInput, 4> program_shaders = {{
      {"resources/shaders/terrain.vert", GL_VERTEX_SHADER},
      {"resources/shaders/terrain.frag", GL_FRAGMENT_SHADER},
      {"resources/shaders/terrain.tcs", GL_TESS_CONTROL_SHADER},
      {"resources/shaders/terrain.tes", GL_TESS_EVALUATION_SHADER},
  }};
  this->shader_program = loadShaderProgram(program_shaders, is_reload);
}

void Terrain::deinit() {
  glDeleteBuffers(1, &this->positions_bo);
  // glDeleteBuffers(1, &this->normals_bo);
  glDeleteBuffers(1, &this->indices_bo);
  glDeleteVertexArrays(1, &this->vaob);
}

void Terrain::render(glm::mat4 projection_matrix, glm::mat4 view_matrix,
                     glm::vec3 camera_position) {
  GLint prev_program = 0;
  glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);

  GLint prev_polygon_mode = 0;
  glGetIntegerv(GL_POLYGON_MODE, &prev_polygon_mode);

  {
    glUseProgram(this->shader_program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->grass_texture.gl_id);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, this->rock_texture.gl_id);

    if (this->wireframe) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    float s = (this->terrain_size / (this->terrain_subdivision + 1));

    this->model_matrix = glm::translate(
        glm::vec3(glm::floor(camera_position.x / s) * s, 0, glm::floor(camera_position.z / s) * s)
        - glm::vec3(1, 0, 1) * this->terrain_size / 2.0f);

    // this->model_matrix = glm::mat4(1.0f);
    gpu::setUniformSlow(this->shader_program, "viewProjectionMatrix",
                        projection_matrix * view_matrix);
    gpu::setUniformSlow(this->shader_program, "modelMatrix", this->model_matrix);
    gpu::setUniformSlow(this->shader_program, "modelViewProjectionMatrix",
                        projection_matrix * view_matrix * this->model_matrix);
    gpu::setUniformSlow(this->shader_program, "modelViewMatrix", view_matrix * this->model_matrix);
    gpu::setUniformSlow(this->shader_program, "normalMatrix",
                        inverse(transpose(view_matrix * this->model_matrix)));

    gpu::setUniformSlow(this->shader_program, "eyeWorldPos", camera_position);

    gpu::setUniformSlow(this->shader_program, "noises_count", (GLint)this->noises.size());
    for (int i = 0; i < this->noises.size(); i++) {
      const auto noise = this->noises[i];
      std::string noise_uniform = "noises[" + std::to_string(i);
      std::string noise_amp = noise_uniform + "].amplitude";
      std::string noise_freq = noise_uniform + "].frequency";
      glUniform1fv(glGetUniformLocation(this->shader_program, noise_amp.c_str()), 1,
                   &noise.amplitude);
      glUniform1fv(glGetUniformLocation(this->shader_program, noise_freq.c_str()), 1,
                   &noise.frequency);
    }

    glUniform1fv(glGetUniformLocation(this->shader_program, "tessMultiplier"), 1,
                 &this->tess_multiplier);

    glUniform1i(glGetUniformLocation(this->shader_program, "has_color_texture"), 0);
    glUniform1i(glGetUniformLocation(this->shader_program, "has_diffuse_texture"), 0);
    glUniform1i(glGetUniformLocation(this->shader_program, "has_reflectivity_texture"), 0);
    glUniform1i(glGetUniformLocation(this->shader_program, "has_metalness_texture"), 0);
    glUniform1i(glGetUniformLocation(this->shader_program, "has_fresnel_texture"), 0);
    glUniform1i(glGetUniformLocation(this->shader_program, "has_shininess_texture"), 0);
    glUniform1i(glGetUniformLocation(this->shader_program, "has_emission_texture"), 0);

    glUniform3fv(glGetUniformLocation(this->shader_program, "material_color"), 1,
                 &material.m_color.x);
    glUniform3fv(glGetUniformLocation(this->shader_program, "material_diffuse_color"), 1,
                 &material.m_color.x);
    glUniform3fv(glGetUniformLocation(this->shader_program, "material_emissive_color"), 1,
                 &material.m_color.x);
    glUniform1fv(glGetUniformLocation(this->shader_program, "material_reflectivity"), 1,
                 &material.m_reflectivity);
    glUniform1fv(glGetUniformLocation(this->shader_program, "material_metalness"), 1,
                 &material.m_metalness);
    glUniform1fv(glGetUniformLocation(this->shader_program, "material_fresnel"), 1,
                 &material.m_fresnel);
    glUniform1fv(glGetUniformLocation(this->shader_program, "material_shininess"), 1,
                 &material.m_shininess);
    glUniform1fv(glGetUniformLocation(this->shader_program, "material_emission"), 1,
                 &material.m_emission);

    // Draw the terrain
    glBindVertexArray(this->vaob);
    glDrawElements(GL_PATCHES, this->indices_count, GL_UNSIGNED_SHORT, 0);
    glBindVertexArray(0);
  }

  glPolygonMode(GL_FRONT_AND_BACK, prev_polygon_mode);

  glUseProgram(prev_program);
}

void Terrain::draw_imgui(SDL_Window* window) {
  if (ImGui::CollapsingHeader("Terrain")) {
    ImGui::Text("Debug");
    ImGui::Checkbox("Wireframe", &this->wireframe);

    ImGui::Separator();

    ImGui::Text("Material");
    ImGui::ColorEdit3("Color", &this->material.m_color.x);
    ImGui::SliderFloat("Reflectivity", &this->material.m_reflectivity, 0.0f, 1.0f);
    ImGui::SliderFloat("Metalness", &this->material.m_metalness, 0.0f, 1.0f);
    ImGui::SliderFloat("Fresnel", &this->material.m_fresnel, 0.0f, 1.0f);
    ImGui::SliderFloat("Shininess", &this->material.m_shininess, 0.0f, 25000.0f, "%.3f",
                       ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Emission", &this->material.m_emission, 0.0f, 10.0f);

    ImGui::Separator();

    {
      ImGui::Text("Mesh");
      bool noise_changed = false;

      ImGui::Text("Triangles: %d", this->indices_count / 3);

      if (ImGui::SliderFloat("Size", &this->terrain_size, 512, 8192)) {
        noise_changed = true;
      };
      if (ImGui::SliderInt("Subdivisions", &this->terrain_subdivision, 0, 256)) {
        noise_changed = true;
      };

      if (noise_changed) {
        this->init_mesh(true);
      }
    }

    ImGui::Separator();

    {
      ImGui::Text("Shader");

      ImGui::SliderFloat("Tessellation Multiplier", &this->tess_multiplier, 0.0f, 64.f);

      {
        float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
        if (ImGui::SmallButton("-")) {
          this->noises.pop_back();
        }
        ImGui::SameLine(0.0f, spacing);
        if (ImGui::SmallButton("+")) {
          this->noises.push_back(TerrainSimplexNoise{
              .amplitude = 0,
              .frequency = 0,
          });
        }
      }

      static int selected_item = 0;
      const std::vector<const char*> items = {"Noise 0", "Noise 1", "Noise 2", "Noise 3", "Noise 4", "Noise 5", "Noise 6", "Noise 7"};
      ImGui::ListBox("Noises", &selected_item, items.data(), this->noises.size());

      ImGui::DragFloat("Amplitude", &this->noises[selected_item].amplitude, 1.0f, 0.0f, 10000.f);
      ImGui::DragFloat("Frequency", &this->noises[selected_item].frequency, 0.001f, 0.0f, 10000.f);
    }

    ImGui::Separator();
  }
}
