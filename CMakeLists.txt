cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

# ---- Project ----

project(
  AdvGfxProject
  VERSION 1.0
  LANGUAGES CXX
)

# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

# ---- Add dependencies via CPM ----
# see https://github.com/TheLartians/CPM.cmake for more info

include(cmake/CPM.cmake)

CPMAddPackage("gh:TheLartians/Format.cmake@1.7.0")

# Threads
find_package(Threads REQUIRED)

# OpenGL
find_package(OpenGL)

# Glad: https://github.com/Dav1dde/glad
CPMAddPackage(
  NAME glad
  GITHUB_REPOSITORY Dav1dde/glad
  VERSION 0.1.34
)

# GLM
CPMAddPackage(
  NAME glm
  GIT_TAG 0.9.9.8
  GITHUB_REPOSITORY g-truc/glm
)

# stb
CPMAddPackage(
  NAME stb
  GITHUB_REPOSITORY nothings/stb
  GIT_TAG c9064e317699d2e495f36ba4f9ac037e88ee371a
  DOWNLOAD_ONLY YES
)

if(stb_ADDED)
  add_library(stb INTERFACE)
  target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
endif()

# SDL2
CPMAddPackage(
  NAME SDL2
  GITHUB_REPOSITORY libsdl-org/SDL
  VERSION 2.0.15
  # 2.0.15 hasn't released yet, so let's use a specific commit
  GIT_TAG 62a562dea20728726a497b4af49d591a2fa385de
)

# ImGui: https://github.com/ocornut/imgui
CPMAddPackage(
  NAME imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui
  VERSION 1.82
)

if(imgui_ADDED)
  set(IMGUI_INCLUDE_DIR ${imgui_SOURCE_DIR}/)
  file(GLOB IMGUI_SOURCES ${imgui_SOURCE_DIR}/*.cpp)
  file(GLOB IMGUI_HEADERS ${imgui_SOURCE_DIR}/*.h)
  add_library(imgui STATIC ${IMGUI_SOURCES})
  target_compile_definitions(imgui PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD=1)
  target_include_directories(imgui PUBLIC ${IMGUI_INCLUDE_DIR})
endif()

# Tiny Obj Loader
CPMAddPackage(
  NAME tinyobjloader
  GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader
  VERSION 1.0.6
)

# --- Import tools ----

include(cmake/tools.cmake)

# ---- Add source files ----

# Note: globbing sources is considered bad practice as CMake's generators may not detect new files
# automatically. Keep that in mind when changing files, or explicitly mention them here.
file(GLOB_RECURSE headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE shaders CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/*")

# ---- Create executable ----
# list all files that will either be used for compilation or that should show up in the ide of your
# choice
add_executable(
  AdvGfxProject ${headers} ${sources} ${shaders} ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl.cpp
                ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

set_target_properties(AdvGfxProject PROPERTIES CXX_STANDARD 17)

# being a cross-platform target, we enforce standards conformance on MSVC
target_compile_options(AdvGfxProject PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")

# Link dependencies
target_link_libraries(
  AdvGfxProject
  PUBLIC ${CMAKE_DL_LIBS}
         glad
         glm
         stb
         SDL2-static
         imgui
         tinyobjloader
)

if(FALSE)
  target_include_directories(
    AdvGfxProject PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                         $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
  )
endif()
