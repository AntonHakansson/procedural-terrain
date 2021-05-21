#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

layout(location = 0) out vec4 fragmentColor;
in vec2 texCoord;

layout(binding = 10) uniform sampler2DShadow shadowMapTex;
layout(binding = 11) uniform sampler2D colorBufferTex;

void main()
{
	fragmentColor = vec4(texCoord, 0, 1); // vec4(1, 0, 0, 1); 
	fragmentColor = texture(colorBufferTex, texCoord);
}

