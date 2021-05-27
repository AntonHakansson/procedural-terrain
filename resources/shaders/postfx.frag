#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

layout(location = 0) out vec4 fragmentColor;
layout(binding = 0) uniform sampler2D tex;

in vec2 texCoord;

uniform mat4 viewMatrix;
uniform mat4 projMatrix;
uniform float currentTime;

struct Water {
  float height;
};
uniform Water water;

const float Epsilon = 1e-10;

vec3 RGBtoHSV(in vec3 RGB) {
  vec4 P = (RGB.g < RGB.b) ? vec4(RGB.bg, -1.0, 2.0 / 3.0) : vec4(RGB.gb, 0.0, -1.0 / 3.0);
  vec4 Q = (RGB.r < P.x) ? vec4(P.xyw, RGB.r) : vec4(RGB.r, P.yzx);
  float C = Q.x - min(Q.w, Q.y);
  float H = abs((Q.w - Q.y) / (6.0 * C + Epsilon) + Q.z);
  vec3 HCV = vec3(H, C, Q.x);
  float S = HCV.y / (HCV.z + Epsilon);
  return vec3(HCV.x, S, HCV.z);
}

vec3 HSVtoRGB(in vec3 HSV) {
  float H = HSV.x;
  float R = abs(H * 6.0 - 3.0) - 1.0;
  float G = 2.0 - abs(H * 6.0 - 2.0);
  float B = 2.0 - abs(H * 6.0 - 4.0);
  vec3 RGB = clamp(vec3(R, G, B), 0.0, 1.0);
  return ((RGB - 1.0) * HSV.y + 1.0) * HSV.z;
}

void main() {
  vec3 screen_space_position = vec3(texCoord, 0) * 2.0 - vec3(1);

  vec4 view_pos_h = inverse(projMatrix) * vec4(screen_space_position, 1.0);
  vec3 view_pos = view_pos_h.xyz / view_pos_h.w;

  vec3 world_pos = (inverse(viewMatrix) * vec4(view_pos, 1)).xyz;

  float height = world_pos.y;

  vec3 ocean_blue_deep = vec3(0.05, 0.1, 0.25);

  if (height < water.height) {
    vec3 col = texture(tex, texCoord + vec2(0, sin(currentTime) * 0.01)).xyz;
    vec3 out_color = mix(col, ocean_blue_deep, 0.9);

    float diff_depth = water.height - height;
    float foam_distance = 8;
    float foam_mask = max(1.0 - diff_depth / foam_distance, 0);
    foam_mask *= max(sin(diff_depth / 1.5 + sin(currentTime * 1.0) * 4), 0);

    foam_mask += max(1.0 - diff_depth / (foam_distance / 2.0), 0);

    fragmentColor = vec4(out_color, 1);
    // fragmentColor = vec4(vec3(foam_mask), 1);

    return;
  }

  vec3 out_color = texture(tex, texCoord).xyz;

  float u_hue = 0.5;
  float u_saturate = 0.5;
  float u_value = 0.5;

  vec3 col_hsv = RGBtoHSV(out_color);
  col_hsv.x *= (u_hue * 2.0);
  col_hsv.y *= (u_saturate * 2.0);
  col_hsv.z *= (u_value * 2.0);
  vec3 col_rgb = HSVtoRGB(col_hsv.xyz);

  fragmentColor = vec4(col_rgb, 1.0);
}
