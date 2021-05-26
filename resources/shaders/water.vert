#version 420

layout(location = 0) in vec3 position;
// layout(location = 1) in vec3 normal;
// layout(location = 2) in vec2 tex_coord;

uniform mat4 model_matrix;
uniform mat4 view_matrix;
uniform mat4 inv_view_matrix;
uniform mat4 projection_matrix;
uniform mat4 inv_projection_matrix;
uniform mat4 pixel_projection; // `pixel_projection` projects from view space to pixel coordinate

out DATA {
    vec2 tex_coord;
    vec3 view_space_normal;
    vec3 view_space_position;
} Out;

void main() {
    vec4 world_space = model_matrix * vec4(position, 1.0);

    gl_Position = projection_matrix * view_matrix * world_space;
    // NOTE: 1024 is the size of the water plane
    Out.tex_coord = world_space.xz / 4096.0;
    Out.view_space_normal = (view_matrix * vec4(0.0, 1.0, 0.0, 0.0)).xyz;

    vec4 view_space_position_h = view_matrix * world_space;
    Out.view_space_position = view_space_position_h.xyz;
}
