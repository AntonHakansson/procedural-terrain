#include "terrain.h"

void Terrain::init() {
  this->buildMesh(false);

  // OpenGL Setup
  glPatchParameteri(GL_PATCH_VERTICES, 3);
  loadShader(false);

  this->rock_texture.load("resources/textures/", "rock.jpg", 3);
  this->grass_texture.load("resources/textures/", "grass.jpg", 3);
}

void Terrain::deinit() {
  glDeleteBuffers(1, &this->positions_bo);
  // glDeleteBuffers(1, &this->normals_bo);
  glDeleteBuffers(1, &this->indices_bo);
  glDeleteVertexArrays(1, &this->vao);
}

void Terrain::loadShader(bool is_reload) {
  if (is_reload) {
    glDeleteProgram(this->shader_program);
  }
  std::array<ShaderInput, 4> program_shaders({
      ShaderInput{"resources/shaders/terrain.vert", GL_VERTEX_SHADER},
      ShaderInput{"resources/shaders/terrain.frag", GL_FRAGMENT_SHADER},
      ShaderInput{"resources/shaders/terrain.tcs", GL_TESS_CONTROL_SHADER},
      ShaderInput{"resources/shaders/terrain.tes", GL_TESS_EVALUATION_SHADER},
  });
  this->shader_program = loadShaderProgram(program_shaders, is_reload);
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

void Terrain::render(glm::mat4 projection_matrix, glm::mat4 view_matrix,
                     glm::vec3 camera_position, glm::mat4 light_matrix) {
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
    gpu::setUniformSlow(this->shader_program, "lightMatrix", light_matrix);
    gpu::setUniformSlow(this->shader_program, "viewMatrix", view_matrix);
    gpu::setUniformSlow(this->shader_program, "viewProjectionMatrix",
                        projection_matrix * view_matrix);
    gpu::setUniformSlow(this->shader_program, "modelMatrix", this->model_matrix);
    gpu::setUniformSlow(this->shader_program, "modelViewProjectionMatrix",
                        projection_matrix * view_matrix * this->model_matrix);
    gpu::setUniformSlow(this->shader_program, "modelViewMatrix", view_matrix * this->model_matrix);
    gpu::setUniformSlow(this->shader_program, "normalMatrix",
                        inverse(transpose(view_matrix * this->model_matrix)));
    gpu::setUniformSlow(this->shader_program, "eyeWorldPos", camera_position);

    gpu::setUniformSlow(this->shader_program, "noise.num_octaves", (GLint)noise.num_octaves);
    gpu::setUniformSlow(this->shader_program, "noise.amplitude", noise.amplitude);
    gpu::setUniformSlow(this->shader_program, "noise.frequency", noise.frequency);
    gpu::setUniformSlow(this->shader_program, "noise.persistence", noise.persistence);
    gpu::setUniformSlow(this->shader_program, "noise.lacunarity", noise.lacunarity);

    gpu::setUniformSlow(this->shader_program, "sun.direction", sun.direction);
    gpu::setUniformSlow(this->shader_program, "sun.color", sun.color);
    gpu::setUniformSlow(this->shader_program, "sun.intensity", sun.intensity);

    glUniform1fv(glGetUniformLocation(this->shader_program, "tessMultiplier"), 1,
                 &this->tess_multiplier);

    // Light
    // gpu::setUniformSlow(this->shader_program, "point_light_color", light.color);
    // gpu::setUniformSlow(this->shader_program, "point_light_intensity_multiplier", light.intensity);
    // gpu::setUniformSlow(this->shader_program, "viewSpaceLightPosition", view_matrix * glm::vec4(light.position, 1.0f));
    // gpu::setUniformSlow(this->shader_program, "viewSpaceLightDir", glm::normalize(glm::vec3(viewMatrix * vec4(-light.position, 0.0f))));
    // gpu::setUniformSlow(this->shader_program, "spotOuterAngle", std::cos(glm::radians(22.5f)));
    // gpu::setUniformSlow(this->shader_program, "spotInnerAngle", std::cos(glm::radians(17.5f)));

    // glm::mat4 lightMatrix = glm::translate(vec3(0.5f)) * glm::scale(vec3(0.5f)) * lightProjectionMatrix * lightViewMatrix * inverse(viewMatrix);
    // gpu::setUniformSlow(this->shader_program, "lightMatrix", lightMatrix);

    // Draw the terrain
    glBindVertexArray(this->vao);
    glDrawElements(GL_PATCHES, this->indices_count, GL_UNSIGNED_SHORT, 0);
    glBindVertexArray(0);
  }

  glPolygonMode(GL_FRONT_AND_BACK, prev_polygon_mode);

  glUseProgram(prev_program);
}

void Terrain::gui(SDL_Window* window) {
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
      this->sun.gui();
    }
  }
}
