#version 420

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coord;

uniform mat4 normalMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 modelViewProjectionMatrix;

out DATA {
    vec2 tex_coord;
} Out;

void main() {
    Out.tex_coord = tex_coord;
	gl_Position = modelViewProjectionMatrix * vec4(position, 1.0);
}
