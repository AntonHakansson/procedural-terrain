#version 420

layout(location = 0) in vec3 world_pos_in;
layout(location = 2) in vec2 tex_coord_in;

uniform mat4 modelMatrix;
uniform vec3 eyeWorldPos;

// Out data
out DATA {
  vec3 world_pos;
  vec2 tex_coord;
  vec3 normal;
}
Out;

void main() {
  // NOTE: We transform the point into local space and _not_ clip space 
  // for the Tesselation Control Shader
  Out.world_pos = (modelMatrix * vec4(world_pos_in, 1.0)).xyz;
  Out.tex_coord = tex_coord_in;
}
