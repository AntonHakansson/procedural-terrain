#version 420
///////////////////////////////////////////////////////////////////////////////
// Input vertex attributes
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) in vec3 Position_VS_in;
// layout(location = 1) in vec3 Normal_VS_in;
layout(location = 2) in vec2 TexCoord_VS_in;

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 normalMatrix;
uniform mat4 modelMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 modelViewProjectionMatrix;
uniform vec3 eyeWorldPos;

///////////////////////////////////////////////////////////////////////////////
// Output to Tesselation Control Shader
///////////////////////////////////////////////////////////////////////////////
out vec3 WorldPos_CS_in;
out vec2 TexCoord_CS_in;
// out vec3 Normal_CS_in;

void main()
{
	// NOTE: We transform the point into local space and _not_ clip space for the Tesselation Control Shader
	WorldPos_CS_in = (modelMatrix * vec4(Position_VS_in, 1.0)).xyz;
	TexCoord_CS_in = TexCoord_VS_in;
	// Normal_CS_in = (modelMatrix * vec4(Normal_VS_in, 0.0)).xyz;
}
