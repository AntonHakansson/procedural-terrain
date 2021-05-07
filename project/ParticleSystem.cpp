#include "ParticleSystem.h"

ParticleSystem::ParticleSystem(int size) : max_size(size)
{
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // Allocate gpu memory for max_size particles
    glVertexAttribPointer(0, 4, GL_FLOAT, false /*normalized*/, 0 /*stride*/, 0 /*offset*/);
    glEnableVertexAttribArray(0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec4) * max_size, nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Allocate cpu memory to upload to gpu
    data.resize(max_size);

    // Load our shader program
    shader = labhelper::loadShaderProgram("../project/particle.vert", "../project/particle.frag", true);

    // load texture
    {
        int w, h, comp;
        unsigned char* image = stbi_load("../scenes/explosion.png", &w, &h, &comp, STBI_rgb_alpha);

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
        free(image);

        // Indicates that the active texture should be repeated for texture coordinates >1 or <-1
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        // unbind texture
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

ParticleSystem::~ParticleSystem()
{
    glDeleteTextures(1, &texture);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

void ParticleSystem::kill(int id) {
    auto position = particles.begin() + id;
    particles.erase(position);
}

void ParticleSystem::spawn(Particle particle) {
    if (particles.size() >= max_size) return;
    particles.push_back(particle);
}

void ParticleSystem::process_particles(float dt) {
    // Kill dead particles
    for (unsigned i = 0; i < particles.size(); ++i) {
        auto &p = particles[i];
        if (p.lifetime > p.life_length) {
            kill(i);
        }
    }

    // Update alive particles!
    for (unsigned i = 0; i < particles.size(); ++i) {
        auto &p = particles[i];
        p.lifetime += dt;
        p.pos      += p.velocity * dt;
        // TODO: add gravity
    }
}

void ParticleSystem::draw_particles(int screen_width, int screen_height, glm::mat4 viewMatrix, glm::mat4 projMatrix) {
    // transform particels to gpu-friendly format
    for (int i = 0; i < particles.size(); i++) {
        const auto &p = particles[i];
        const float a = (p.lifetime / p.life_length);
        const glm::vec3 view_pos = glm::vec3(viewMatrix * glm::vec4(p.pos, 1));
        data[i] = glm::vec4(view_pos, a);
    }

    // Sort particles back to front for correct alpha blending
    std::sort(data.begin(), std::next(data.begin(), particles.size()),
        [](const glm::vec4 &lhs, const glm::vec4 &rhs) { return lhs.z < rhs.z; });

    // Bind the particle texture to texture slot 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);

    // Enable shader program point size modulation.
    glEnable(GL_PROGRAM_POINT_SIZE);

    // Enable blending.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Use particle shader
    glUseProgram(shader);
    labhelper::setUniformSlow(shader, "P", projMatrix);
    labhelper::setUniformSlow(shader, "screen_x", (float)screen_width);
    labhelper::setUniformSlow(shader, "screen_y", (float)screen_height);

    // Draw active particles
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(glm::vec4) * particles.size(), &data[0]);
    glDrawArrays(GL_POINTS, 0, particles.size());

    // Reset GL state
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glDisable(GL_BLEND);
    glDisable(GL_PROGRAM_POINT_SIZE);
}

void ParticleSystem::draw_imgui(SDL_Window* window) {
    if (ImGui::CollapsingHeader("Particle System"))
    {
        ImGui::Text("Max Particles %d", max_size);
        ImGui::Text("Active Particles %d", particles.size());
    }
}
