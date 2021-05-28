#include "terrain.h"

void Terrain::init() {
  this->buildMesh(false);

  // OpenGL Setup
  glPatchParameteri(GL_PATCH_VERTICES, 3);
  loadShader(false);

  std::array<std::string, 4> albedo_paths = {
      "resources/textures/terrain/beach/albedo.jpg",
      "resources/textures/terrain/grass/albedo.jpg",
      "resources/textures/terrain/rock_beach/albedo.jpg",
      "resources/textures/terrain/snow/albedo.jpg",
  };
  std::array<std::string, 4> normal_paths = {
      "resources/textures/terrain/beach/normal.jpg",
      "resources/textures/terrain/grass/normal.jpg",
      "resources/textures/terrain/rock_beach/normal.jpg",
      "resources/textures/terrain/snow/normal.jpg",
  };
  std::array<std::string, 4> displacement_paths = {
      "resources/textures/terrain/beach/displacement.jpg",
      "resources/textures/terrain/grass/displacement.jpg",
      "resources/textures/terrain/rock_beach/displacement.jpg",
      "resources/textures/terrain/snow/displacement.jpg",
  };
  std::array<std::string, 4> roughness_paths = {
      "resources/textures/terrain/beach/roughness.jpg",
      "resources/textures/terrain/grass/roughness.jpg",
      "resources/textures/terrain/rock_beach/roughness.jpg",
      "resources/textures/terrain/snow/roughness.jpg",
  };

  albedos.load2DArray<4>(albedo_paths, 5);
  normals.load2DArray<4>(normal_paths, 5);
  displacements.load2DArray<4>(displacement_paths, 5);
}

void Terrain::deinit() {
  glDeleteTextures(1, &albedos.gl_id);
  glDeleteTextures(1, &normals.gl_id);
  glDeleteTextures(1, &displacements.gl_id);
  glDeleteBuffers(1, &this->positions_bo);
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

void Terrain::update(float delta_time, float current_time) {
  mat4 sun_matrix = inverse(lookAt(vec3(0), -sun.direction, vec3(0, 1, 0)));
  sun_matrix = rotate(radians(delta_time * sun.orbit_speed), sun.orbit_axis) * sun_matrix;
  sun.direction = vec3(sun_matrix[2][0], sun_matrix[2][1], sun_matrix[2][2]);
}

void Terrain::begin(bool simple) {
  glUseProgram(simple ? this->shader_program_simple : this->shader_program);
  this->simple = simple;
}

void Terrain::render(glm::mat4 projection_matrix, glm::mat4 view_matrix, glm::vec3 center, 
                     glm::vec3 camera_position, glm::mat4 light_matrix, float water_height) {
  GLint prev_polygon_mode;

  GLuint shader_program = this->simple ? this->shader_program_simple : this->shader_program;

  {
    glBindTextureUnit(0, albedos.gl_id);
    glBindTextureUnit(1, normals.gl_id);
    glBindTextureUnit(2, displacements.gl_id);

    if (this->wireframe) {
      glGetIntegerv(GL_POLYGON_MODE, &prev_polygon_mode);

      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    float s = (this->terrain_size / (this->terrain_subdivision + 1));

    this->model_matrix = glm::translate(
        glm::vec3(glm::floor(center.x / s) * s, 0, glm::floor(center.z / s) * s)
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
    glUniform1fv(glGetUniformLocation(shader_program, "texture_sizes"), texture_sizes.size(),
                 texture_sizes.data());
    glUniform1fv(glGetUniformLocation(shader_program, "texture_displacement_weights"),
                 texture_displacement_weights.size(), texture_displacement_weights.data());

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
      ImGui::DragFloat(("h" + std::to_string(i)).c_str(), &h);
    }

    ImGui::Text("Texture Blends");
    for (int i = 0; i < texture_start_heights.size(); i++) {
      auto& b = texture_blends[i];
      ImGui::DragFloat(("b" + std::to_string(i)).c_str(), &b);
    }

    ImGui::Text("Texture Scaling");
    for (int i = 0; i < texture_sizes.size(); i++) {
      auto& b = texture_sizes[i];
      ImGui::SliderFloat(("s" + std::to_string(i)).c_str(), &b, 0.0, 80);
    }

    ImGui::Text("Texture Displacement weight");
    for (int i = 0; i < texture_displacement_weights.size(); i++) {
      auto& b = texture_displacement_weights[i];
      ImGui::SliderFloat(("d" + std::to_string(i)).c_str(), &b, 0.5, 2.0);
    }
  }
}
