#pragma once

#include <GL/glew.h>
#include <vector>
#include <glm/detail/type_vec3.hpp>
#include <glm/mat4x4.hpp>
#include <imgui.h>

#include <labhelper.h>
#include <Model.h>

#include "FastNoiseLite.h"

struct Terrain;

struct TerrainChunk {
	int chunkX, chunkZ;
	uint32_t size;
	uint32_t indices_count;

	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<uint16_t> indices;

	// Buffers on GPU
	uint32_t positions_bo;
	uint32_t normals_bo;
	uint32_t indices_bo; // NOTE(hak): this can be shared between chunks
	// uint32_t texture_coordinates_bo;

	// Vertex Array Object
	uint32_t vaob;

	void init(int chunk_x, int chunk_z, Terrain *_terrain);
	void deinit();
	void upload();
	void render();
};

struct Terrain {
	std::vector<TerrainChunk> chunks;

	fnl_state noise;

	glm::mat4 model_matrix;

    labhelper::Material material;

	void init();
	void deinit();

	float getHeight(float x, float z);

	void render();

	void draw_imgui(SDL_Window* window);
};

