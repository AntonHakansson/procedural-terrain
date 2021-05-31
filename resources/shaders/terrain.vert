#version 420

layout(location = 0) in vec3 pos_in;

uniform mat4 modelMatrix;

// Out data
out DATA {
  vec3 world_pos;
  vec2 tex_coord;
  vec3 normal;
}
Out;

void main() {
  // NOTE: We transform the point into world space and _not_ clip space
  // for the Tesselation Control Shader
  Out.world_pos = (modelMatrix * vec4(pos_in, 1.0)).xyz;
  Out.tex_coord = Out.world_pos.xz / (2048.0);
}
