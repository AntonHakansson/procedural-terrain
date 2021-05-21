#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

in DATA {
    vec2 tex_coord;
} In;

layout(location = 0) out vec4 fragmentColor;

void main() {
    fragmentColor = vec4(1.0, 0.0, 0.0, 1.0);
}
