#version 420

layout(location = 0) in vec3 position;
// layout(location = 1) in vec3 normal;
// layout(location = 2) in vec2 tex_coord;

uniform mat4 model_matrix;
uniform mat4 view_matrix;
uniform mat4 inv_view_matrix;
uniform mat4 projection_matrix;

struct Water {
  float height;
  float foam_distance;
  float wave_speed;
  float wave_strength;
  float wave_scale;
  float size;
};
uniform Water water;

out DATA {
  vec2 tex_coord;
  vec3 view_space_normal;
  vec3 view_space_position;
  vec3 world_pos;
}
Out;

void main() {
  vec4 world_pos = model_matrix * vec4(position, 1.0);
  Out.world_pos = world_pos.xyz;

  gl_Position = projection_matrix * view_matrix * world_pos;

  Out.tex_coord = world_pos.xz / water.size;
  Out.view_space_normal = (view_matrix * vec4(0.0, 1.0, 0.0, 0.0)).xyz;
  Out.view_space_position = (view_matrix * world_pos).xyz;
}
