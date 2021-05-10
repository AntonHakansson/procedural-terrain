#pragma once

#include <vector>
#include <sstream>
#include <fstream>

#include <GL/glew.h>
#include <glm/detail/type_vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtx/transform.hpp>
#include <imgui.h>

#include <labhelper.h>
#include <Model.h>

#include "FastNoiseLite.h"

struct Terrain {
	float terrain_size;
	int indices_count;
	int terrain_resolution;

	bool wireframe;

	float amplitude;
	float tess_multiplier;
	fnl_state noise;

	GLuint shader_program;

	glm::mat4 model_matrix;

	labhelper::Material material;
	labhelper::Texture grass_texture;
	labhelper::Texture rock_texture;

	// Buffers on GPU
	uint32_t positions_bo;
	uint32_t normals_bo;
	uint32_t indices_bo;

	// Vertex Array Object
	uint32_t vaob;

	void init();
	void init_mesh(bool is_reload);
	void deinit();

	float getHeight(float x, float z);

	void render(glm::mat4 projection_matrix, glm::mat4 view_matrix, glm::vec3 camera_position);

	void loadShader(bool is_reload);

	void draw_imgui(SDL_Window* window);
};

