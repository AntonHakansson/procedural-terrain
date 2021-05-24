#version 420

layout(location = 0) in vec2 position;
// layout(location = 1) in vec3 normal;
// layout(location = 2) in vec2 tex_coord;

// uniform mat4 normalMatrix;
// uniform mat4 modelViewMatrix;
// uniform mat4 modelViewProjectionMatrix;

out DATA {
    vec2 tex_coord;
} Out;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    Out.tex_coord = 0.5 * (position + vec2(1.0, 1.0));
}
