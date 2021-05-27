#include "terrain.h"

void Terrain::init() {
  this->buildMesh(false);

  // OpenGL Setup
  glPatchParameteri(GL_PATCH_VERTICES, 3);
  loadShader(false);

  this->rock_texture.load("resources/textures/terrain/rock/", "rock.jpg", 3);
  this->grass_texture.load("resources/textures/terrain/grass/", "grass.jpg", 3);
  this->sand_texture.load("resources/textures/terrain/sand/", "sand.jpg", 3);
  this->snow_texture.load("resources/textures/terrain/snow/", "snow.jpg", 3);

  this->rock_normal.load("resources/textures/terrain/rock/", "rock_normal.jpg", 3);
  this->grass_normal.load("resources/textures/terrain/grass/", "grass_normal.jpg", 3);
  this->sand_normal.load("resources/textures/terrain/sand/", "sand_normal.jpg", 3);
  this->snow_normal.load("resources/textures/terrain/snow/", "snow_normal.jpg", 3);
}

void Terrain::deinit() {
  glDeleteTextures(1, &rock_texture.gl_id);
  glDeleteTextures(1, &grass_texture.gl_id);
  glDeleteTextures(1, &sand_texture.gl_id);
  glDeleteTextures(1, &snow_texture.gl_id);

  glDeleteBuffers(1, &this->positions_bo);
  // glDeleteBuffers(1, &this->normals_bo);
  glDeleteBuffers(1, &this->indices_bo);
  glDeleteVertexArrays(1, &this->vao);
}

void Terrain::loadShader(bool is_reload) {
  std::array<ShaderInput, 4> program_shaders({
      ShaderInput{"resources/shaders/terrain.vert", GL_VERTEX_SHADER},
      ShaderInput{"resources/shaders/terrain.frag", GL_FRAGMENT_SHADER},
      ShaderInput{"resources/shaders/terrain.tcs", GL_TESS_CONTROL_SHADER},
      ShaderInput{"resources/shaders/terrain.tes", GL_TESS_EVALUATION_SHADER},
  });

  auto program = loadShaderProgram(program_shaders, is_reload);
  if (program != 0) {
    if (is_reload) {
      glDeleteProgram(this->shader_program);
    }
    this->shader_program = program;
  }

  std::array<ShaderInput, 4> program_shaders_simple({
      ShaderInput{"resources/shaders/terrain.vert", GL_VERTEX_SHADER},
      ShaderInput{"resources/shaders/simple.frag", GL_FRAGMENT_SHADER},
      ShaderInput{"resources/shaders/terrain.tcs", GL_TESS_CONTROL_SHADER},
      ShaderInput{"resources/shaders/terrain.tes", GL_TESS_EVALUATION_SHADER},
  });

  auto program_simple = loadShaderProgram(program_shaders_simple, is_reload);
  if (program_simple != 0) {
    if (is_reload) {
      glDeleteProgram(this->shader_program_simple);
    }
    this->shader_program_simple = program_simple;
  }
}

void Terrain::buildMesh(bool is_reload) {
  if (is_reload) {
    glDeleteBuffers(1, &this->positions_bo);
    glDeleteBuffers(1, &this->indices_bo);
    glDeleteVertexArrays(1, &this->vao);
  }
  this->indices_count
      = gpu::createSubdividedPlane(this->terrain_size, this->terrain_subdivision, &this->vao,
                                   &this->positions_bo, &this->indices_bo);
}

void Terrain::setPolyOffset(float factor, float units) {
  gpu::setUniformSlow(this->shader_program, "polygon_offset_factor", factor);
  gpu::setUniformSlow(this->shader_program, "polygon_offset_units", units);
}

void Terrain::begin(bool simple) {
  glUseProgram(simple ? this->shader_program_simple : this->shader_program);
  this->simple = simple;
}

void Terrain::render(glm::mat4 projection_matrix, glm::mat4 view_matrix, glm::vec3 camera_position,
                     glm::mat4 light_matrix, float water_height) {
  GLint prev_polygon_mode;

  GLuint shader_program = this->simple ? this->shader_program_simple : this->shader_program;

  {
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->grass_texture.gl_id);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, this->rock_texture.gl_id);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, this->sand_texture.gl_id);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, this->snow_texture.gl_id);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, this->grass_normal.gl_id);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, this->rock_normal.gl_id);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, this->sand_normal.gl_id);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, this->snow_normal.gl_id);

    if (this->wireframe) {
      glGetIntegerv(GL_POLYGON_MODE, &prev_polygon_mode);

      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    float s = (this->terrain_size / (this->terrain_subdivision + 1));

    this->model_matrix = glm::translate(
        glm::vec3(glm::floor(camera_position.x / s) * s, 0, glm::floor(camera_position.z / s) * s)
        - glm::vec3(1, 0, 1) * this->terrain_size / 2.0f);

    // this->model_matrix = glm::mat4(1.0f);
    gpu::setUniformSlow(shader_program, "lightMatrix", light_matrix);
    gpu::setUniformSlow(shader_program, "viewMatrix", view_matrix);
    gpu::setUniformSlow(shader_program, "viewProjectionMatrix", projection_matrix * view_matrix);
    gpu::setUniformSlow(shader_program, "modelMatrix", this->model_matrix);
    gpu::setUniformSlow(shader_program, "modelViewProjectionMatrix",
                        projection_matrix * view_matrix * this->model_matrix);
    gpu::setUniformSlow(shader_program, "modelViewMatrix", view_matrix * this->model_matrix);
    gpu::setUniformSlow(shader_program, "normalMatrix",
                        inverse(transpose(view_matrix * this->model_matrix)));
    gpu::setUniformSlow(shader_program, "eyeWorldPos", camera_position);

    gpu::setUniformSlow(shader_program, "noise.num_octaves", (GLint)noise.num_octaves);
    gpu::setUniformSlow(shader_program, "noise.amplitude", noise.amplitude);
    gpu::setUniformSlow(shader_program, "noise.frequency", noise.frequency);
    gpu::setUniformSlow(shader_program, "noise.persistence", noise.persistence);
    gpu::setUniformSlow(shader_program, "noise.lacunarity", noise.lacunarity);

    gpu::setUniformSlow(shader_program, "sun.direction", sun.direction);
    gpu::setUniformSlow(shader_program, "sun.color", sun.color);
    gpu::setUniformSlow(shader_program, "sun.intensity", sun.intensity);

    gpu::setUniformSlow(shader_program, "noise.num_octaves", (GLint)noise.num_octaves);

    gpu::setUniformSlow(shader_program, "waterHeight", water_height);
    glUniform1fv(glGetUniformLocation(shader_program, "texture_start_heights"),
                 texture_start_heights.size(), texture_start_heights.data());
    glUniform1fv(glGetUniformLocation(shader_program, "texture_blends"), texture_blends.size(),
                 texture_blends.data());

    glUniform1fv(glGetUniformLocation(shader_program, "tessMultiplier"), 1, &this->tess_multiplier);

    // Draw the terrain
    glBindVertexArray(this->vao);
    glDrawElements(GL_PATCHES, this->indices_count, GL_UNSIGNED_SHORT, 0);
    glBindVertexArray(0);
  }

  if (this->wireframe) {
    glPolygonMode(GL_FRONT_AND_BACK, prev_polygon_mode);
  }
}

void Terrain::gui(Camera* camera) {
  if (ImGui::CollapsingHeader("Terrain")) {
    ImGui::Text("Debug");
    { ImGui::Checkbox("Wireframe", &this->wireframe); }

    ImGui::Text("Mesh");
    {
      ImGui::Text("Triangles: %d", this->indices_count / 3);

      bool mesh_changed = false;
      mesh_changed |= ImGui::SliderFloat("Size", &this->terrain_size, 512, 8192);
      mesh_changed |= ImGui::SliderInt("Subdivisions", &this->terrain_subdivision, 0, 256);
      ImGui::DragFloat("Tesselation Multiplier", &this->tess_multiplier, 1.0, 0.0);
      this->tess_multiplier = glm::max(this->tess_multiplier, 0.f);

      if (mesh_changed) {
        this->buildMesh(true);
      }
    }

    ImGui::Text("Shader");
    {
      ImGui::Text("Noise");
      this->noise.gui();

      ImGui::Text("Sun");
      this->sun.gui(camera);
    }

    ImGui::Text("Texture Start Heights");
    for (int i = 0; i < texture_start_heights.size(); i++) {
      auto& h = texture_start_heights[i];
      ImGui::SliderFloat(("h" + std::to_string(i)).c_str(), &h, 0.0, 1.0);
    }

    ImGui::Text("Texture blends");
    for (int i = 0; i < texture_start_heights.size(); i++) {
      auto& b = texture_blends[i];
      ImGui::SliderFloat(("b" + std::to_string(i)).c_str(), &b, 0.0, 0.5);
    }
  }
}
