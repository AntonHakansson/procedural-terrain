#include "terrain.h"

///////////////////////////////////////////////////////////////////////////////
// Terrain Chunk
///////////////////////////////////////////////////////////////////////////////
void TerrainChunk::init(int chunk_x, int chunk_z, Terrain *_terrain) {
    this->chunkX = chunk_x;
    this->chunkZ = chunk_z;
    this->size = 256;
    this->indices_count = (this->size - 1) * (this->size - 1) * 6;

    this->positions.resize(this->size * this->size);
    this->normals.resize(this->indices_count);
    this->indices.resize(this->indices_count);

    // init heightmap
    {
        size_t index = 0;
        float offset_x = this->chunkX * (this->size - 1);
        float offset_z = this->chunkZ * (this->size - 1);

        for (int z = 0; z < this->size; z++) {
            for (int x = 0; x < this->size; x++) {
                auto &pos = this->positions[index];

                pos.x = x + offset_x;
                pos.z = z + offset_z;
                pos.y = _terrain->getHeight(pos.x, pos.z);

                index += 1;
            }
        }
    }

    // build triangles
    {
        size_t index = 0;
        for (size_t z = 0; z < this->size - 1; z++) {
            for (size_t x = 0; x < this->size - 1; x++) {

                auto start = z * this->size + x;

                auto top_left = start;
                auto top_right = start + 1;
                auto bot_left = start + this->size;
                auto bot_right = start + this->size + 1;

                this->indices[index++] = top_left;
                this->indices[index++] = bot_left;
                this->indices[index++] = bot_right;

                this->indices[index++] = top_right;
                this->indices[index++] = top_left;
                this->indices[index++] = bot_right;
            }
        }
    }

    // compute normals
    {
        for (size_t i = 0; i < this->indices_count; i += 3) {
            auto &v0 = this->positions[this->indices[i + 0]];
            auto &v1 = this->positions[this->indices[i + 1]];
            auto &v2 = this->positions[this->indices[i + 2]];

            auto normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));

            this->normals[this->indices[i + 0]] += normal;
            this->normals[this->indices[i + 1]] += normal;
            this->normals[this->indices[i + 2]] += normal;
        }
        for (size_t i = 0; i < this->indices_count; i++) {
            this->normals[i] = glm::normalize(this->normals[i]);
        }
    }
}

void TerrainChunk::deinit() {
    glDeleteBuffers(1, &this->positions_bo);
    glDeleteBuffers(1, &this->normals_bo);
    glDeleteBuffers(1, &this->indices_bo);
    glDeleteVertexArrays(1, &this->vaob);
}

void TerrainChunk::upload() {
    glGenVertexArrays(1, &this->vaob);
    glBindVertexArray(this->vaob);

    glGenBuffers(1, &this->positions_bo);
    glBindBuffer(GL_ARRAY_BUFFER, this->positions_bo);
    glBufferData(GL_ARRAY_BUFFER, this->size * this->size * sizeof(glm::vec3), &this->positions[0], GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &this->normals_bo);
    glBindBuffer(GL_ARRAY_BUFFER, this->normals_bo);
    glBufferData(GL_ARRAY_BUFFER, this->indices_count * sizeof(glm::vec3), &this->normals[0], GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), 0);
    glEnableVertexAttribArray(1);

    glGenBuffers(1, &this->indices_bo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->indices_bo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, this->indices_count * sizeof(uint16_t), &this->indices[0], GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    this->positions.clear();
    this->normals.clear();
    this->indices.clear();
}

void TerrainChunk::render() {
    glBindVertexArray(this->vaob);
    glDrawElements(GL_PATCHES, this->indices_count, GL_UNSIGNED_SHORT, 0);
    glBindVertexArray(0);
}

///////////////////////////////////////////////////////////////////////////////
// Terrain
///////////////////////////////////////////////////////////////////////////////
//
//

GLuint load_program(const std::string& vertexShader, const std::string& fragmentShader, const std::string& tcsShaderName, const std::string& tesShaderName, bool allow_errors) {
    GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);
    GLuint tcsShader = glCreateShader(GL_TESS_CONTROL_SHADER);
    GLuint tesShader = glCreateShader(GL_TESS_EVALUATION_SHADER);

    std::ifstream vs_file(vertexShader);
    std::string vs_src((std::istreambuf_iterator<char>(vs_file)), std::istreambuf_iterator<char>());

    std::ifstream fs_file(fragmentShader);
    std::string fs_src((std::istreambuf_iterator<char>(fs_file)), std::istreambuf_iterator<char>());


    std::ifstream tcs_file(tcsShaderName);
    std::string tcs_src((std::istreambuf_iterator<char>(tcs_file)), std::istreambuf_iterator<char>());

    std::ifstream tes_file(tesShaderName);
    std::string tes_src((std::istreambuf_iterator<char>(tes_file)), std::istreambuf_iterator<char>());

    const char* vs = vs_src.c_str();
    const char* fs = fs_src.c_str();
    const char* tcs = tcs_src.c_str();
    const char* tes = tes_src.c_str();

    glShaderSource(vShader, 1, &vs, nullptr);
    glShaderSource(fShader, 1, &fs, nullptr);
    glShaderSource(tcsShader, 1, &tcs, nullptr);
    glShaderSource(tesShader, 1, &tes, nullptr);
    // text data is not needed beyond this point

    glCompileShader(vShader);
    int compileOk = 0;
    glGetShaderiv(vShader, GL_COMPILE_STATUS, &compileOk);
    if(!compileOk)
    {
        std::string err = labhelper::GetShaderInfoLog(vShader);
        if(allow_errors)
        {
            labhelper::non_fatal_error(err, "Vertex Shader");
        }
        else
        {
            labhelper::fatal_error(err, "Vertex Shader");
        }
        return 0;
    }

    glCompileShader(fShader);
    glGetShaderiv(fShader, GL_COMPILE_STATUS, &compileOk);
    if(!compileOk)
    {
        std::string err = labhelper::GetShaderInfoLog(fShader);
        if(allow_errors)
        {
            labhelper::non_fatal_error(err, "Fragment Shader");
        }
        else
        {
            labhelper::fatal_error(err, "Fragment Shader");
        }
        return 0;
    }

    glCompileShader(tcsShader);
    glGetShaderiv(tcsShader, GL_COMPILE_STATUS, &compileOk);
    if(!compileOk)
    {
        std::string err = labhelper::GetShaderInfoLog(tcsShader);
        if(allow_errors)
        {
            labhelper::non_fatal_error(err, "TCS Shader");
        }
        else
        {
            labhelper::fatal_error(err, "TCS Shader");
        }
        return 0;
    }
    glCompileShader(tesShader);
    glGetShaderiv(tesShader, GL_COMPILE_STATUS, &compileOk);
    if(!compileOk)
    {
        std::string err = labhelper::GetShaderInfoLog(tesShader);
        if(allow_errors)
        {
            labhelper::non_fatal_error(err, "TES Shader");
        }
        else
        {
            labhelper::fatal_error(err, "TES Shader");
        }
        return 0;
    }


    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, fShader);
    glDeleteShader(fShader);
    glAttachShader(shaderProgram, vShader);
    glDeleteShader(vShader);
    glAttachShader(shaderProgram, tcsShader);
    glDeleteShader(tcsShader);
    glAttachShader(shaderProgram, tesShader);
    glDeleteShader(tesShader);
    if(!allow_errors)
        CHECK_GL_ERROR();

    if(!labhelper::linkShaderProgram(shaderProgram, allow_errors))
        return 0;

    return shaderProgram;
}

void Terrain::init() {

    this->model_matrix = glm::mat4(1.0);

    this->material = labhelper::Material{
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

    this->amplitude = 5.f;
    this->noise = fnlCreateState();
    this->noise.noise_type = FNL_NOISE_OPENSIMPLEX2;
    this->noise.fractal_type = FNL_FRACTAL_FBM;
    this->noise.frequency = 0.1f;
    this->noise.octaves = 6;
    this->noise.lacunarity = 2.5f;
    this->noise.gain = 1.0f;

    // for (auto z = -1; z < 2; z++) {
    //     for (auto x = -1; x < 2; x++) {
    //         this->chunks.emplace_back();
    //         this->chunks.back().init(x, z, this);
    //         this->chunks.back().upload();
    //     }
    // }

    this->chunks.emplace_back();
    this->chunks.back().init(0, 0, this);
    this->chunks.back().upload();

    // OPENGL Setup
    glPatchParameteri(GL_PATCH_VERTICES, 3);
    this->shader_program = load_program("../project/terrain.vert", "../project/terrain.frag", "../project/terrain.tcs", "../project/terrain.tes", false);
}



void Terrain::deinit() {
    for (auto chunk : this->chunks) {
        chunk.deinit();
    }
    this->chunks.clear();
}

float Terrain::getHeight(float x, float z) {
    return fnlGetNoise2D(&this->noise, x, z) * this->amplitude;
}

void Terrain::render(glm::mat4 projection_matrix, glm::mat4 view_matrix, glm::vec3 camera_position) {
    GLint prev_program = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);

    {
        glUseProgram(this->shader_program);
        printf("Using terrain\n");

        camera_position.y = 0;
        this->model_matrix = glm::translate(camera_position - 256 / 2.0f);
        labhelper::setUniformSlow(this->shader_program, "viewProjectionMatrix",
                                    projection_matrix * view_matrix);
        labhelper::setUniformSlow(this->shader_program, "modelViewProjectionMatrix",
                                    projection_matrix * view_matrix * this->model_matrix);
        labhelper::setUniformSlow(this->shader_program, "modelViewMatrix", view_matrix * this->model_matrix);
        labhelper::setUniformSlow(this->shader_program, "normalMatrix",
                                    inverse(transpose(view_matrix * this->model_matrix)));

        labhelper::setUniformSlow(this->shader_program, "eyeWorldPos", camera_position);

        glUniform1i(glGetUniformLocation(this->shader_program, "amplitude"), this->amplitude);

        glUniform1i(glGetUniformLocation(this->shader_program, "has_color_texture"), 0);
        glUniform1i(glGetUniformLocation(this->shader_program, "has_diffuse_texture"), 0);
        glUniform1i(glGetUniformLocation(this->shader_program, "has_reflectivity_texture"), 0);
        glUniform1i(glGetUniformLocation(this->shader_program, "has_metalness_texture"), 0);
        glUniform1i(glGetUniformLocation(this->shader_program, "has_fresnel_texture"), 0);
        glUniform1i(glGetUniformLocation(this->shader_program, "has_shininess_texture"), 0);
        glUniform1i(glGetUniformLocation(this->shader_program, "has_emission_texture"), 0);

        glUniform3fv(glGetUniformLocation(this->shader_program, "material_color"), 1, &material.m_color.x);
        glUniform3fv(glGetUniformLocation(this->shader_program, "material_diffuse_color"), 1, &material.m_color.x);
        glUniform3fv(glGetUniformLocation(this->shader_program, "material_emissive_color"), 1, &material.m_color.x);
        glUniform1fv(glGetUniformLocation(this->shader_program, "material_reflectivity"), 1, &material.m_reflectivity);
        glUniform1fv(glGetUniformLocation(this->shader_program, "material_metalness"), 1, &material.m_metalness);
        glUniform1fv(glGetUniformLocation(this->shader_program, "material_fresnel"), 1, &material.m_fresnel);
        glUniform1fv(glGetUniformLocation(this->shader_program, "material_shininess"), 1, &material.m_shininess);
        glUniform1fv(glGetUniformLocation(this->shader_program, "material_emission"), 1, &material.m_emission);

        for (auto chunk : this->chunks) {
            chunk.render();
        }
    }

    glUseProgram(prev_program);
    printf("prev\n");
}

void Terrain::draw_imgui(SDL_Window* window) {
    if (ImGui::CollapsingHeader("Terrain"))
    {
        ImGui::Text("Chunks %ld", this->chunks.size());
        ImGui::Separator();

        ImGui::Text("Material");
        ImGui::ColorEdit3("Color", &this->material.m_color.x);
        ImGui::SliderFloat("Reflectivity", &this->material.m_reflectivity, 0.0f, 1.0f);
        ImGui::SliderFloat("Metalness", &this->material.m_metalness, 0.0f, 1.0f);
        ImGui::SliderFloat("Fresnel", &this->material.m_fresnel, 0.0f, 1.0f);
        ImGui::SliderFloat("Shininess", &this->material.m_shininess, 0.0f, 25000.0f, "%.3f", 5);
        ImGui::SliderFloat("Emission", &this->material.m_emission, 0.0f, 10.0f);
        ImGui::Separator();

        ImGui::Text("Noise");

        bool noise_changed = false;

        if(ImGui::SliderFloat("Amplitude", &this->amplitude, 1.f, 100.f)) {
            // noise_changed = true;
        };
        if(ImGui::SliderFloat("Tesselation Level", &this->tess_level, 1.f, 50.f)) {
            // noise_changed = true;
        };
        if(ImGui::SliderFloat("Tesselation Multiplier", &this->tess_multiplier, 1.f, 50.f)) {
            // noise_changed = true;
        };
        if(ImGui::SliderFloat("Frequency", &this->noise.frequency, 0.01f, 1.f)) {
            noise_changed = true;
        };
        if(ImGui::SliderInt("Octaves", &this->noise.octaves, 1, 10)) {
            noise_changed = true;
        };
        if(ImGui::SliderFloat("Lacunarity", &this->noise.lacunarity, 0.0f, 5.f)) {
            noise_changed = true;
        };
        if(ImGui::SliderFloat("Gain", &this->noise.gain, 0.1f, 5.f)) {
            noise_changed = true;
        };

        if (noise_changed) {
            for (auto &chunk : this->chunks) {
                chunk.deinit();
                chunk.init(chunk.chunkX, chunk.chunkZ, this);
                chunk.upload();
            }
        }

        ImGui::Separator();
    }
}
