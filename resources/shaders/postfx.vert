#version 420

layout(location = 0) in vec2 position;

uniform mat4 viewMatrix;
uniform mat4 projMatrix;

out DATA {
  vec2 tex_coord;
  vec3 view_pos;
  vec3 world_pos;
  vec3 world_dir;
}
Out;

void main() {
  gl_Position = vec4(position, 0.0, 1.0);

  Out.tex_coord = 0.5 * (position + vec2(1, 1));

  mat4 view_inverse = inverse(viewMatrix);
  mat4 proj_inverse = inverse(projMatrix);

  vec3 screen_space_position = vec3(Out.tex_coord, 0) * 2.0 - vec3(1);
  Out.view_pos = (proj_inverse * vec4(screen_space_position, 1.0)).xyz;

  vec3 view_dir = normalize(Out.view_pos);

  Out.world_pos = (view_inverse * vec4(Out.view_pos, 1)).xyz;
  Out.world_dir = (view_inverse * vec4(view_dir, 0)).xyz;
}