#version 420
// This vertex shader simply outputs the input coordinates to the rasterizer. It only uses 2D coordinates.
layout(location = 0) in vec2 position;

out vec2 texCoord;

void main()
{
	gl_Position = vec4(position, 0.0, 1.0);
	texCoord = 0.5 * (position + vec2(1, 1));
}