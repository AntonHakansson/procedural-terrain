#ifdef WIN32
#  define WIN32_LEAN_AND_MEAN
#  define VC_EXTRALEAN
#  define NOMINMAX  //          - Macros min(a,b) and max(a,b)
#  include <windows.h>
#  undef near
#  undef far
#else
#  include <signal.h>
#endif  // WIN32

#include <glad/glad.h>

// STB_IMAGE for loading images of many filetypes
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <imgui.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <iostream>
#include <sstream>
#include <streambuf>
#include <string>
#include <vector>

#include "gpu.h"

//#define HDR_FRAMEBUFFER

namespace gpu {
  SDL_Window* init_window_SDL(const std::string& caption, int width, int height) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
      fprintf(stderr, "%s: %s\n", "Couldn't initialize SDL", SDL_GetError());
      return nullptr;
    }
    atexit(SDL_Quit);
    SDL_GL_LoadLibrary(nullptr);  // Default OpenGL is fine.

    // Request an OpenGL 4.3 context (should be core)
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

#ifdef HDR_FRAMEBUFFER
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 16);
#endif

    // Also request a depth buffer
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    // Create the window
    SDL_Window* window
        = SDL_CreateWindow(caption.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width,
                           height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (window == nullptr) {
      fprintf(stderr, "%s: %s\n", "Couldn't set video mode", SDL_GetError());
      return nullptr;
    }

    static SDL_GLContext maincontext = SDL_GL_CreateContext(window);
    if (maincontext == nullptr) {
      fprintf(stderr, "%s: %s\n", "Failed to create OpenGL context", SDL_GetError());
      return nullptr;
    }

    // Initialize GLAD; this gives us access to OpenGL Extensions.
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
      fprintf(stderr, "Failed to initialize OpenGL context");
      return nullptr;
    }

    // Initialize ImGui; will allow us to edit variables in the application.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForOpenGL(window, maincontext);
    ImGui_ImplOpenGL3_Init(nullptr);

    // Check OpenGL properties
    gpu::startupGLDiagnostics();
    gpu::setupGLDebugMessages();

    // Flip textures vertically so they don't end up upside-down.
    stbi_set_flip_vertically_on_load(true);

    // 1 for v-sync
    SDL_GL_SetSwapInterval(1);

    /* Workaround for AMD. It might no longer be necessary, but I dunno if we
     * are ever going to remove it. (Consider it a piece of living history.)
     */
    // if(!glBindFragDataLocation)
    // {
    // 	glBindFragDataLocation = glBindFragDataLocationEXT;
    // }
    return window;
  }

  void shutDown(SDL_Window* window) {
    // If newframe is not ever run before shut down we crash
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window);

    // Destroy imgui
    ImGui_ImplSDL2_Shutdown();

    // Destroy window
    SDL_DestroyWindow(window);

    // Quit SDL subsystems
    SDL_Quit();
  }

  GLuint loadCubeMap(const char* facePosX, const char* faceNegX, const char* facePosY,
                     const char* faceNegY, const char* facePosZ, const char* faceNegZ) {
    //********************************************
    //	Creating a local helper function in C++
    //********************************************
    class tempTexHelper {
    public:
      static void loadCubeMapFace(std::string filename, GLenum face) {
        int w, h, comp;
        unsigned char* image = stbi_load(filename.c_str(), &w, &h, &comp, STBI_rgb_alpha);

        if (image == nullptr) {
          std::cout << "Failed to load texture: " << filename << std::endl;
        }

        glTexImage2D(face, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
      }
    };

    //************************************************
    //	Creating a texture ID for the OpenGL texture
    //************************************************
    GLuint textureID = 0;
    glGenTextures(1, &textureID);

    //************************************************
    //	 Load the faces into the cube map texture
    //************************************************
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    tempTexHelper::loadCubeMapFace(facePosX, GL_TEXTURE_CUBE_MAP_POSITIVE_X);
    tempTexHelper::loadCubeMapFace(faceNegX, GL_TEXTURE_CUBE_MAP_NEGATIVE_X);
    tempTexHelper::loadCubeMapFace(facePosY, GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
    tempTexHelper::loadCubeMapFace(faceNegY, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
    tempTexHelper::loadCubeMapFace(facePosZ, GL_TEXTURE_CUBE_MAP_POSITIVE_Z);
    tempTexHelper::loadCubeMapFace(faceNegZ, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);

    //************************************************
    //			Set filtering parameters
    //************************************************

    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // Sets the type of mipmap interpolation to be used on magnifying and
    // minifying the active texture.
    // For cube maps, filtering across faces causes artifacts - so disable filtering
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // In case you want filtering anyway, try this below instead
    //	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // Use
    // tri-linear mip map filtering 	glTexParameterf(GL_TEXTURE_CUBE_MAP,
    // GL_TEXTURE_MAX_ANISOTROPY_EXT, 16);			  // or replace trilinear mipmap
    // filtering with nicest anisotropic filtering

    CHECK_GL_ERROR();
    return textureID;
  }

  bool checkGLError(const char* file, int line) {
    bool wasError = false;

    for (GLenum glErr = glGetError(); glErr != GL_NO_ERROR; glErr = glGetError()) {
      wasError = true;
      std::string error;

      switch (glErr) {
        case GL_INVALID_OPERATION:
          error = "INVALID_OPERATION";
          break;
        case GL_INVALID_ENUM:
          error = "INVALID_ENUM";
          break;
        case GL_INVALID_VALUE:
          error = "INVALID_VALUE";
          break;
        case GL_OUT_OF_MEMORY:
          error = "OUT_OF_MEMORY";
          break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
          error = "INVALID_FRAMEBUFFER_OPERATION";
          break;
      }

      std::cerr << "GL Error #" << glErr << "(" << error << ") "
                << " in File " << file << " at line: " << line << std::endl;

#if defined(_WIN32)
      std::stringstream ss;
      ss << file << "(" << line << "): GL Error #" << glErr << "(" << error << ") " << std::endl;

      // outputs error message to debug console, if a debugger is attached.
      OutputDebugStringA(ss.str().c_str());
#endif
    }
    return wasError;
  }

  // startupGLDiagnostics
  void startupGLDiagnostics() {
    // print diagnostic information
    printf("GL  VENDOR: %s\n", glGetString(GL_VENDOR));
    printf("   VERSION: %s\n", glGetString(GL_VERSION));
    printf("  RENDERER: %s\n", glGetString(GL_RENDERER));

    // test if we've got GL 3.0
    // if(!GLEW_VERSION_3_0)
    // {
    // 	fatal_error("OpenGL 3.0 not supported.\n"
    // 	            "Please update your drivers and/or buy a better graphics card.");
    // }
  }

  // setupGLDebugMessages
  namespace {
#if defined(_WIN32)
#  define CALLBACK_ CALLBACK
#else  // !win32
#  define CALLBACK_
#endif  // ~ platform

    /* Callback function. This function is called by GL whenever it generates
     * a debug message.
     */
    GLvoid CALLBACK_ handle_debug_message_(GLenum aSource, GLenum aType, GLuint aId,
                                           GLenum aSeverity, GLsizei /*aLength*/,
                                           GLchar const* aMessage, GLvoid* /*aUser*/) {
      // source string
      const char* srcStr = nullptr;
      switch (aSource) {
        case GL_DEBUG_SOURCE_API:
          srcStr = "API";
          break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
          srcStr = "WINDOW_SYSTEM";
          break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
          srcStr = "SHADER_COMPILER";
          break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:
          srcStr = "THIRD_PARTY";
          break;
        case GL_DEBUG_SOURCE_APPLICATION:
          srcStr = "APPLICATION";
          break;
        case GL_DEBUG_SOURCE_OTHER:
          srcStr = "OTHER";
          break;
        default:
          srcStr = "UNKNOWN";
          break;
      }

      // type
      const char* typeStr = nullptr;
      switch (aType) {
        case GL_DEBUG_TYPE_ERROR:
          typeStr = "ERROR";
          break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
          typeStr = "DEPRECATED_BEHAVIOR";
          break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
          typeStr = "UNDEFINED_BEHAVIOR";
          break;
        case GL_DEBUG_TYPE_PORTABILITY:
          typeStr = "PORTABILITY";
          break;
        case GL_DEBUG_TYPE_PERFORMANCE:
          typeStr = "PERFORMANCE";
          break;
        case GL_DEBUG_TYPE_OTHER:
          typeStr = "OTHER";
          break;
        default:
          typeStr = "UNKNOWN";
          break;
      }

      // severity
      const char* sevStr = nullptr;
      switch (aSeverity) {
        case GL_DEBUG_SEVERITY_HIGH:
          sevStr = "HIGH";
          break;
        case GL_DEBUG_SEVERITY_MEDIUM:
          sevStr = "MEDIUM";
          break;
        case GL_DEBUG_SEVERITY_LOW:
          sevStr = "LOW";
          break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
          sevStr = "NOTIFICATION";
          break;
        default:
          sevStr = "UNKNOWN";
      }

      // output message, if not just notification
      if (aSeverity != GL_DEBUG_SEVERITY_NOTIFICATION) {
        std::stringstream szs;
        szs << "\n"
            << "--\n"
            << "-- GL DEBUG MESSAGE:\n"
            << "--   severity = '" << sevStr << "'\n"
            << "--   type     = '" << typeStr << "'\n"
            << "--   source   = '" << srcStr << "'\n"
            << "--   id       = " << std::hex << aId << "\n"
            << "-- message:\n"
            << aMessage << "\n"
            << "--\n"
            << "\n";

        fprintf(stderr, "%s", szs.str().c_str());
        fflush(stderr);
#if defined(_WIN32)
        OutputDebugStringA(szs.str().c_str());
#endif
      }

      // Additionally: if it's (really) bad -> break!
      if (aSeverity == GL_DEBUG_SEVERITY_HIGH) {
#if defined(_WIN32)
        if (IsDebuggerPresent()) __debugbreak();
#else   // !win32
        raise(SIGTRAP);
#endif  // ~ platform
      }
    }

    // cleanup macros
#undef CALLBACK_
  }  // namespace

  void setupGLDebugMessages() {
    // Check for GL errors prior to us.
    CHECK_GL_ERROR();

    /* Make sure that we support this extension before attempting to do any-
     * thing with it...
     */
    if (glad_glDebugMessageCallback == nullptr) {
      printf(" --- \n");
      printf(" --- \n");
      printf(" --- BIG WARNING: GL_debug_output not supported!\n");
      printf(" --- This is rather bad news.\n"); /* TODO: give link to doc explaining this */
      printf(" --- \n");
      printf(" --- \n");
      return;
    }

    /* Enable debug messages. Set a callback handle, and then enable all
     * messages to begin with.
     */
    glDebugMessageCallback((GLDEBUGPROC)handle_debug_message_, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_PERFORMANCE, GL_DONT_CARE, 0, nullptr,
                          GL_FALSE);

    /* Enable synchronous debug output; this causes the callback to be called
     * immediately on error, usually in the actual gl-function where the error
     * occurred. (So, your stack trace is actually useful).
     *
     * This comes at a performance cost, so for performance sensitive stuff,
     * it might not be a good idea to unconditionally enable this. For the
     * labs, we just enable it, however.
     */
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

    /* Debug output can be somewhat spammy, especially if all messages are
     * enabled. For now, disable the lowest level of messages, which mostly
     * contain performance-related information and other random notes.
     */
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW, 0, 0, false);

    /* Now, check if enabling debug messages caused a GL error. If so, that
     * error might not be reported by the debug message mechanism (after all,
     * the error occurred while setting it up). Later errors should be reported
     * via the callbacks, though.
     */
    CHECK_GL_ERROR();
  }

  // Error reporting
  void fatal_error(std::string errorString, std::string title) {
    if (title.empty()) {
      title = "GL-Tutorial - Error";
    }
    if (errorString.empty()) {
      errorString = "(unknown error)";
    }
    // On Win32 we'll use a message box. On !Win32, just print to stderr and abort()
#if defined(_WIN32)
    MessageBox(0, errorString.c_str(), title.c_str(), MB_OK | MB_ICONEXCLAMATION);
#else
    fprintf(stderr, "%s : %s\n", title.c_str(), errorString.c_str());
#endif
    abort();
  }

  void non_fatal_error(std::string errorString, std::string title) {
    if (title.empty()) {
      title = "GL-Tutorial - Error";
    }
    if (errorString.empty()) {
      errorString = "(unknown error)";
    }
    // On Win32 we'll use a message box. On !Win32, just print to stderr and abort()
#if defined(_WIN32)
    MessageBox(0, errorString.c_str(), title.c_str(), MB_OK | MB_ICONEXCLAMATION);
#else
    fprintf(stderr, "%s : %s\n", title.c_str(), errorString.c_str());
#endif
  }

  std::string GetShaderInfoLog(GLuint obj) {
    int logLength = 0;
    int charsWritten = 0;
    char* tmpLog;
    std::string log;

    glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &logLength);

    if (logLength > 0) {
      tmpLog = new char[logLength];
      glGetShaderInfoLog(obj, logLength, &charsWritten, tmpLog);
      log = tmpLog;
      delete[] tmpLog;
    }

    return log;
  }

  GLuint loadShaderProgram(const std::string& vertexShader, const std::string& fragmentShader,
                           bool allow_errors) {
    GLuint vShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fShader = glCreateShader(GL_FRAGMENT_SHADER);

    std::ifstream vs_file(vertexShader);
    std::string vs_src((std::istreambuf_iterator<char>(vs_file)), std::istreambuf_iterator<char>());

    std::ifstream fs_file(fragmentShader);
    std::string fs_src((std::istreambuf_iterator<char>(fs_file)), std::istreambuf_iterator<char>());

    const char* vs = vs_src.c_str();
    const char* fs = fs_src.c_str();

    glShaderSource(vShader, 1, &vs, nullptr);
    glShaderSource(fShader, 1, &fs, nullptr);
    // text data is not needed beyond this point

    glCompileShader(vShader);
    int compileOk = 0;
    glGetShaderiv(vShader, GL_COMPILE_STATUS, &compileOk);
    if (!compileOk) {
      std::string err = GetShaderInfoLog(vShader);
      if (allow_errors) {
        non_fatal_error(err, "Vertex Shader");
      } else {
        fatal_error(err, "Vertex Shader");
      }
      return 0;
    }

    glCompileShader(fShader);
    glGetShaderiv(fShader, GL_COMPILE_STATUS, &compileOk);
    if (!compileOk) {
      std::string err = GetShaderInfoLog(fShader);
      if (allow_errors) {
        non_fatal_error(err, "Fragment Shader");
      } else {
        fatal_error(err, "Fragment Shader");
      }
      return 0;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, fShader);
    glDeleteShader(fShader);
    glAttachShader(shaderProgram, vShader);
    glDeleteShader(vShader);
    if (!allow_errors) CHECK_GL_ERROR();

    if (!linkShaderProgram(shaderProgram, allow_errors)) return 0;

    return shaderProgram;
  }

  bool linkShaderProgram(GLuint shaderProgram, bool allow_errors) {
    glLinkProgram(shaderProgram);
    GLint linkOk = 0;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkOk);
    if (!linkOk) {
      std::string err = GetShaderInfoLog(shaderProgram);
      if (allow_errors) {
        non_fatal_error(err, "Linking");
      } else {
        fatal_error(err, "Linking");
      }
      return false;
    }
    return true;
  }

  GLuint createAddAttribBuffer(GLuint vertexArrayObject, const void* data, const size_t dataSize,
                               GLuint attributeIndex, GLsizei attributeSize, GLenum type,
                               GLenum bufferUsage) {
    GLuint buffer = 0;
    glGenBuffers(1, &buffer);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, dataSize, data, bufferUsage);
    CHECK_GL_ERROR();

    // Now attach buffer to vertex array object.
    glBindVertexArray(vertexArrayObject);
    glVertexAttribPointer(attributeIndex, attributeSize, type, false, 0, 0);
    glEnableVertexAttribArray(attributeIndex);
    CHECK_GL_ERROR();

    return buffer;
  }

  void setUniformSlow(GLuint shaderProgram, const char* name, const glm::mat4& matrix) {
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, name), 1, false, &matrix[0].x);
  }
  void setUniformSlow(GLuint shaderProgram, const char* name, const float value) {
    glUniform1f(glGetUniformLocation(shaderProgram, name), value);
  }
  void setUniformSlow(GLuint shaderProgram, const char* name, const GLint value) {
    int loc = glGetUniformLocation(shaderProgram, name);
    glUniform1i(loc, value);
  }
  void setUniformSlow(GLuint shaderProgram, const char* name, const glm::vec3& value) {
    glUniform3fv(glGetUniformLocation(shaderProgram, name), 1, &value.x);
  }
  void setUniformSlow(GLuint shaderProgram, const char* name, const uint32_t nof_values,
                      const glm::vec3* values) {
    glUniform3fv(glGetUniformLocation(shaderProgram, name), nof_values, (float*)values);
  }
  void setUniformSlow(GLuint shaderProgram, const char* name, const glm::ivec2& value) {
    glUniform2i(glGetUniformLocation(shaderProgram, name), value.x, value.y);
  }

  size_t createSubdividedPlane(float _size, unsigned int subdivisions, GLuint* out_vao,
                               GLuint* out_vbo, GLuint* out_texcoord_bo, GLuint* out_ebo) {
    assert(out_vao != nullptr);
    assert(out_vbo != nullptr);
    assert(out_ebo != nullptr);

    auto vertices_x_count = (subdivisions + 2);
    auto vertices_count = vertices_x_count * vertices_x_count;
    auto indices_count = (vertices_x_count - 1) * (vertices_x_count - 1) * 6;
    auto step_size = _size / (subdivisions + 1);

    auto* positions = new glm::vec3[vertices_count];
    auto* texcoords = new glm::vec2[vertices_count];
    auto* indices = new uint16_t[indices_count];
    defer(delete[] positions);
    defer(delete[] texcoords);
    defer(delete[] indices);

    // init heightmap
    {
      size_t index = 0;
      for (int z = 0; z < vertices_x_count; z++) {
        for (int x = 0; x < vertices_x_count; x++) {
          auto& pos = positions[index];

          pos.x = x * step_size;
          pos.z = z * step_size;
          pos.y = 0;

          index += 1;
        }
      }
    }

    if (out_texcoord_bo != nullptr) {
      size_t index = 0;
      for(int v = 0; v < vertices_x_count; v++) {
        for(int u = 0; u < vertices_x_count; u++) {
          auto& texcoord = texcoords[index];
          texcoord.x = u / (float)(vertices_x_count - 1);
          texcoord.y = v / (float)(vertices_x_count - 1);

          index += 1;
        }
      }
    }

    // build triangles
    {
      size_t index = 0;
      for (size_t z = 0; z < vertices_x_count - 1; z++) {
        for (size_t x = 0; x < vertices_x_count - 1; x++) {
          auto start = z * vertices_x_count + x;

          auto top_left = start;
          auto top_right = start + 1;
          auto bot_left = start + vertices_x_count;
          auto bot_right = start + vertices_x_count + 1;

          indices[index++] = top_left;
          indices[index++] = bot_left;
          indices[index++] = bot_right;

          indices[index++] = top_right;
          indices[index++] = top_left;
          indices[index++] = bot_right;
        }
      }
    }

    glCreateVertexArrays(1, out_vao);

    // positions
    {
      glCreateBuffers(1, out_vbo);
      glNamedBufferData(*out_vbo, vertices_count * sizeof(positions[0]), positions, GL_STATIC_DRAW);
      glVertexArrayVertexBuffer(*out_vao, 0, *out_vbo, 0, sizeof(glm::vec3));
      glEnableVertexArrayAttrib(*out_vao, 0);
      glVertexArrayAttribFormat(*out_vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
      glVertexArrayAttribBinding(*out_vao, 0, 0);
    }

    // texcoords
    if (out_texcoord_bo != nullptr) {
      glCreateBuffers(1, out_texcoord_bo);
      glNamedBufferData(*out_texcoord_bo, vertices_count * sizeof(texcoords[0]), texcoords, GL_STATIC_DRAW);
      glVertexArrayVertexBuffer(*out_vao, 2, *out_texcoord_bo, 0, sizeof(glm::vec2));
      glEnableVertexArrayAttrib(*out_vao, 2);
      glVertexArrayAttribFormat(*out_vao, 2, 2, GL_FLOAT, GL_FALSE, 0);
      glVertexArrayAttribBinding(*out_vao, 2, 2);
    }

    // indices
    {
      glCreateBuffers(1, out_ebo);
      glNamedBufferData(*out_ebo, indices_count * sizeof(uint16_t), indices, GL_STATIC_DRAW);
      glVertexArrayElementBuffer(*out_vao, *out_ebo);
    }

    CHECK_GL_ERROR();

    return indices_count;
  }

  void debugDrawLine(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix,
                     const glm::vec3& worldSpaceLightPos) {
    static GLuint vertexArrayObject = 0;
    static int nofVertices = 2;
    // do this initialization first time the function is called...
    if (vertexArrayObject == 0) {
      glGenVertexArrays(1, &vertexArrayObject);
      static const glm::vec3 positions[] = {
          {worldSpaceLightPos.x, worldSpaceLightPos.y, worldSpaceLightPos.z}, {0.0f, 0.0f, 0.0f}};
      gpu::createAddAttribBuffer(vertexArrayObject, positions, sizeof(positions), 0, 3, GL_FLOAT);
    }
    glBindVertexArray(vertexArrayObject);
    glDrawArrays(GL_LINES, 0, nofVertices);
  }

  void drawFullScreenQuad() {
    GLboolean previous_depth_state;
    glGetBooleanv(GL_DEPTH_TEST, &previous_depth_state);
    glDisable(GL_DEPTH_TEST);
    static GLuint vertexArrayObject = 0;
    static int nofVertices = 6;
    // do this initialization first time the function is called...
    if (vertexArrayObject == 0) {
      glGenVertexArrays(1, &vertexArrayObject);
      static const glm::vec2 positions[] = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f},
                                            {-1.0f, -1.0f}, {1.0f, 1.0f},  {-1.0f, 1.0f}};
      gpu::createAddAttribBuffer(vertexArrayObject, positions, sizeof(positions), 0, 2, GL_FLOAT);
    }
    glBindVertexArray(vertexArrayObject);
    glDrawArrays(GL_TRIANGLES, 0, nofVertices);
    if (previous_depth_state) glEnable(GL_DEPTH_TEST);
  }

  float uniform_randf(const float from, const float to) {
    return from + (to - from) * float(rand()) / float(RAND_MAX);
  }

  float randf() { return float(rand()) / float(RAND_MAX); }

  ///////////////////////////////////////////////////////////////////////////
  // Generate uniform points on a disc
  ///////////////////////////////////////////////////////////////////////////
  void concentricSampleDisk(float* dx, float* dy) {
    float r, theta;
    float u1 = randf();
    float u2 = randf();
    // Map uniform random numbers to $[-1,1]^2$
    float sx = 2 * u1 - 1;
    float sy = 2 * u2 - 1;
    // Map square to $(r,\theta)$
    // Handle degeneracy at the origin
    if (sx == 0.0 && sy == 0.0) {
      *dx = 0.0;
      *dy = 0.0;
      return;
    }
    if (sx >= -sy) {
      if (sx > sy) {  // Handle first region of disk
        r = sx;
        if (sy > 0.0)
          theta = sy / r;
        else
          theta = 8.0f + sy / r;
      } else {  // Handle second region of disk
        r = sy;
        theta = 2.0f - sx / r;
      }
    } else {
      if (sx <= sy) {  // Handle third region of disk
        r = -sx;
        theta = 4.0f - sy / r;
      } else {  // Handle fourth region of disk
        r = -sy;
        theta = 6.0f + sx / r;
      }
    }
    theta *= float(M_PI) / 4.0f;
    *dx = r * cosf(theta);
    *dy = r * sinf(theta);
  }
  ///////////////////////////////////////////////////////////////////////////
  // Generate points with a cosine distribution on the hemisphere
  ///////////////////////////////////////////////////////////////////////////
  glm::vec3 cosineSampleHemisphere() {
    glm::vec3 ret;
    concentricSampleDisk(&ret.x, &ret.y);
    ret.z = sqrt(glm::max(0.f, 1.f - ret.x * ret.x - ret.y * ret.y));
    return ret;
  }
}  // namespace gpu
