cmake_minimum_required ( VERSION 3.7 )

project ( labhelper )

# External libraries which need to be built.
add_subdirectory ( ${CMAKE_SOURCE_DIR}/external_src/imgui-1.51 ${CMAKE_CURRENT_BINARY_DIR}/imgui )
set_target_properties(imgui PROPERTIES FOLDER external)

find_package ( SDL2 REQUIRED )
find_package ( glm REQUIRED )
find_package ( GLEW REQUIRED )
find_package ( OpenGL REQUIRED )

# Build and link library.
add_library ( ${PROJECT_NAME} 
    labhelper.h 
    labhelper.cpp 
    Model.h
    Model.cpp
    imgui_impl_sdl_gl3.h
    imgui_impl_sdl_gl3.cpp
    )

if (MSVC)
	set(CMAKE_CXX_FLAGS_DEBUG_MODEL "/O2")
	string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
else()
	set(CMAKE_CXX_FLAGS_DEBUG_MODEL "-O3")
endif()
set_property(SOURCE Model.cpp labhelper.cpp PROPERTY COMPILE_OPTIONS "$<$<CONFIG:Debug>:${CMAKE_CXX_FLAGS_DEBUG_MODEL}>")

target_include_directories( ${PROJECT_NAME}
    PUBLIC
    ${CMAKE_SOURCE_DIR}/labhelper
    ${CMAKE_SOURCE_DIR}/external_src/stb-master
    ${CMAKE_SOURCE_DIR}/external_src/tinyobjloader-1.0.6
    ${SDL2_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    )

target_link_libraries ( ${PROJECT_NAME}
    PUBLIC
    imgui
    ${SDL2_LIBRARIES}
    ${GLEW_LIBRARIES}
    ${OPENGL_LIBRARY}
    )
