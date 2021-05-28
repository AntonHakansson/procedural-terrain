#version 420

layout(location = 0) out vec4 fragmentColor;

in vec3 fColor;

void main() { fragmentColor = vec4(fColor, 1.0); }