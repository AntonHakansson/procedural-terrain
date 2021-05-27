#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <array>

namespace gpu {
  struct Texture {
    bool valid = false;
    uint32_t gl_id = 0;
    std::string filename;
    std::string directory;
    int width, height;
    uint8_t* data = nullptr;
    bool load(const std::string& directory, const std::string& filename, int nof_components);

    template<int N>
    void load2DArray(const std::array<std::string, N>& filepaths, int miplevels) {
      std::array<int, N> widths;
      std::array<int, N> heights;
      std::array<uint8_t*, N> tex_data;

      for (int i = 0; i < filepaths.size(); i++) {
        auto filepath = filepaths[i];
        int width;
        int height;
        int components;
        uint8_t* data = stbi_load(filepath.c_str(), &width, &height, &components, 3);

        if (data == nullptr) {
          std::cout << "ERROR: Failed to load texture:" << filepaths[i] << "\n";
          exit(1);
        }

        widths[i] = width;
        heights[i] = height;
        tex_data[i] = data;
      }

      glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &gl_id);
      glTextureParameteri(gl_id, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTextureParameteri(gl_id, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
      glTextureParameteri(gl_id, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTextureParameteri(gl_id, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTextureParameteri(gl_id, GL_TEXTURE_MAX_ANISOTROPY, 16);

      glTextureStorage3D(gl_id, miplevels, GL_RGB8, widths[0], heights[0], filepaths.size());

      for (int i = 0; i < filepaths.size(); i++) {
        auto w = widths[i];
        auto h = heights[i];
        glTextureSubImage3D(gl_id, 0, 0, 0, i, w, h, 1, GL_RGB, GL_UNSIGNED_BYTE, tex_data[i]);
        stbi_image_free(tex_data[i]);
      }

      glGenerateTextureMipmap(gl_id);
    }
  };
  //////////////////////////////////////////////////////////////////////////////
  // This material class implements a subset of the suggested PBR extension
  // to the OBJ/MTL format, explained at:
  // http://exocortex.com/blog/extending_wavefront_mtl_to_support_pbr
  // NOTE: A material can have _either_ a (textured) roughness, or a good old
  //       shininess value. We still use shininess for the Blinn mfd in the
  //       GL labs, but roughness for pathtracing.
  //////////////////////////////////////////////////////////////////////////////
  struct Material {
    std::string m_name;
    glm::vec3 m_color;
    float m_reflectivity;
    float m_shininess;
    float m_metalness;
    float m_fresnel;
    float m_emission;
    float m_transparency;
    Texture m_color_texture;
    Texture m_reflectivity_texture;
    Texture m_shininess_texture;
    Texture m_metalness_texture;
    Texture m_fresnel_texture;
    Texture m_emission_texture;
  };

  struct Mesh {
    std::string m_name;
    uint32_t m_material_idx;
    // Where this Mesh's vertices start
    uint32_t m_start_index;
    uint32_t m_number_of_vertices;
  };

  class Model {
  public:
    ~Model();
    // The name of the whole model
    std::string m_name;
    // The filename of this model
    std::string m_filename;
    // The materials
    std::vector<Material> m_materials;
    // A model will contain one or more "Meshes"
    std::vector<Mesh> m_meshes;
    // Buffers on CPU
    std::vector<glm::vec3> m_positions;
    std::vector<glm::vec3> m_normals;
    std::vector<glm::vec2> m_texture_coordinates;
    // Buffers on GPU
    uint32_t m_positions_bo;
    uint32_t m_normals_bo;
    uint32_t m_texture_coordinates_bo;
    // Vertex Array Object
    uint32_t m_vaob;
  };

  Model* loadModelFromOBJ(std::string filename);
  void saveModelToOBJ(Model* model, std::string filename);
  void freeModel(Model* model);
  void render(const Model* model, const bool submitMaterials = true);
}  // namespace gpu
