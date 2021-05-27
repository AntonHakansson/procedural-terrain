#version 420

layout(location = 0) out vec4 fragmentColor;

in vec3 fColor;

void main() { fragmentColor = vec4(vec3(1, 0, 0), 1.0); }