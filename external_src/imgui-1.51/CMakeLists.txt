cmake_minimum_required ( VERSION 3.0.2 )

project ( imgui )

# Build and link executable.
add_library ( imgui
	${CMAKE_CURRENT_SOURCE_DIR}/imconfig.h
	${CMAKE_CURRENT_SOURCE_DIR}/imgui.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/imgui.h
	${CMAKE_CURRENT_SOURCE_DIR}/imgui_draw.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/imgui_internal.h
	${CMAKE_CURRENT_SOURCE_DIR}/stb_rect_pack.h
	${CMAKE_CURRENT_SOURCE_DIR}/stb_textedit.h
	${CMAKE_CURRENT_SOURCE_DIR}/stb_truetype.h
    ) 
target_include_directories(imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
