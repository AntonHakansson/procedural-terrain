#version 420

layout(location = 0) in vec3 Position_VS_in;
// layout(location = 1) in vec3 Normal_VS_in;
layout(location = 2) in vec2 TexCoord_VS_in;

uniform mat4 normalMatrix;
uniform mat4 modelMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 modelViewProjectionMatrix;
uniform vec3 eyeWorldPos;

// Out data
out DATA {
	vec3 world_pos;
	vec2 tex_coord;
	vec3 normal;
} Out;

void main() {
	// NOTE: We transform the point into local space and _not_ clip space for the Tesselation Control Shader
	Out.world_pos = (modelMatrix * vec4(Position_VS_in, 1.0)).xyz;
	Out.tex_coord = TexCoord_VS_in;
	// Out.normal = (modelMatrix * vec4(Normal_VS_in, 0.0)).xyz;
}
