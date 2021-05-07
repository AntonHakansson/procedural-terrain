#pragma once


#include <GL/glew.h>
#include <vector>
#include <algorithm>
#include <glm/detail/type_vec3.hpp>
#include <glm/mat4x4.hpp>
#include <stb_image.h>
#include <imgui.h>

#include <labhelper.h>

struct Particle
{
  float lifetime;
  float life_length;
  glm::vec3 velocity;
  glm::vec3 pos;
};

class ParticleSystem
{
  public:

    // Members
    std::vector<Particle> particles;
    std::vector<glm::vec4> data;
    int max_size;

    GLuint vao{0};
    GLuint vbo{0};
    GLuint shader;
    GLuint texture;

    // Ctor/Dtor
    ParticleSystem(int size);
    ~ParticleSystem();

    // Methods
    void kill(int id);
    void spawn(Particle particle);
    void process_particles(float dt);
    void draw_particles(int screen_width, int screen_height, glm::mat4 viewMatrix, glm::mat4 projmatrix);
    void draw_imgui(SDL_Window* window);
};
