cmake_minimum_required ( VERSION 3.0.2 )

project ( project )

# Find *all* shaders.
file(GLOB_RECURSE SHADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.frag"
)
# Separate filter for shaders.
source_group("Shaders" FILES ${SHADERS})

# Build and link executable.
add_executable ( ${PROJECT_NAME}
    main.cpp
    fbo.cpp
    hdr.cpp
    heightfield.cpp
    ParticleSystem.cpp
    ParticleSystem.h
    ${SHADERS}
    )

target_link_libraries ( ${PROJECT_NAME} labhelper SDL2)
config_build_output()
