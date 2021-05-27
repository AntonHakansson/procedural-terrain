#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

layout(location = 0) out vec4 fragmentColor;
layout(binding = 0) uniform sampler2D tex;
layout(binding = 1) uniform sampler2D depth_tex;

in vec2 texCoord;

uniform mat4 viewMatrix;
uniform mat4 projMatrix;
uniform float currentTime;

struct Water {
  float height;
};
uniform Water water;

struct Sun {
  vec3 direction;
  vec3 color;
  float intensity;
};
uniform Sun sun;

struct PostFX {
  float z_near;
  float z_far;
};
uniform PostFX postfx;

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

vec3 shiftHSV(in vec3 in_col, float hue, float saturate, float value) {
  vec3 col_hsv = RGBtoHSV(in_col);
  col_hsv.x *= (hue * 2.0);
  col_hsv.y *= (saturate * 2.0);
  col_hsv.z *= (value * 2.0);

  return HSVtoRGB(col_hsv.xyz);
}

float linearizeDepth(float depth) {
  return 2.0 * postfx.z_near * postfx.z_far
         / (postfx.z_far + postfx.z_near - (2.0 * depth - 1.0) * (postfx.z_far - postfx.z_near));
}

float inverseLerp(float a, float b, float x) { return (x - a) / (b - a); }
float inverseLerpClamped(float a, float b, float x) { return clamp(inverseLerp(a, b, x), 0, 1); }

void main() {
  vec3 screen_space_position = vec3(texCoord, 0) * 2.0 - vec3(1);

  mat4 view_inverse = inverse(viewMatrix);

  vec4 view_pos_h = inverse(projMatrix) * vec4(screen_space_position, 1.0);
  vec3 view_pos = view_pos_h.xyz / view_pos_h.w;
  vec3 view_dir = normalize(view_pos);

  vec3 world_pos = (view_inverse * vec4(view_pos, 1)).xyz;
  vec3 world_dir = (view_inverse * vec4(view_dir, 0)).xyz;

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
  float depth = texture(depth_tex, texCoord).x;
  float linear_depth = linearizeDepth(depth);

  out_color = shiftHSV(out_color, 0.5, 0.65, 0.5);

  fragmentColor = vec4(out_color, 1.0);

  /*
   * Procedural sun and horizon
   */

  float f = max(dot(world_dir, -sun.direction), 0.0);
  float hf = -sun.direction.y;
  float horizon_trans = inverseLerpClamped(0.2, 0.6, hf);
  float night_trans = inverseLerpClamped(-0.4, 0.45, hf);

  vec3 sun_color = mix(vec3(1, 0.6, 0.4), sun.color, horizon_trans);

  float sun_threshold = 0.997;
  float sun_fade = 0.0025;

  if (f > sun_threshold) {
    if (linear_depth == postfx.z_far) {
      float f1 = smoothstep(0, 1, inverseLerpClamped(sun_threshold, min(sun_threshold + sun_fade, 1), f));

      // vec3 sun_col = shiftHSV(sun.col, 0.5, 0.5, 0.5);
      fragmentColor += vec4(mix(vec3(1, 0.6, 0.2), sun_color, horizon_trans) * f1, 1.0);
    }
    else {
      // Sample some nearby pixels

      float size = 7;
      int maxBound = int(floor(size / 2));
      int minBound = -int(floor((size - 0.01) / 2));

      float percentage_lit = 0;
      for (int k = minBound; k <= maxBound; k++) {
        for (int l = minBound; l <= maxBound; l++) {
          vec2 texel = vec2(1 / 128.0, 1 / 128.0);
          vec2 offset = vec2(texel * vec2(l, k));

          float depth = texture(depth_tex, texCoord + offset).x;
          float linear_depth = linearizeDepth(depth);

          if (f > sun_threshold && linear_depth == postfx.z_far) {
            percentage_lit++;
          }
        }
      }

      float average = percentage_lit / (size * size);

      float f1 = smoothstep(0, 1, inverseLerpClamped(sun_threshold, 1, f));
      fragmentColor += vec4(sun_color * average * f1, 1.0);
    }
  }

// (1 - horizon_trans) * 
  float hfs = linear_depth == postfx.z_far ? clamp(inverseLerpClamped(0, 0.4, world_dir.y) + (inverseLerpClamped(sun_threshold + sun_fade / 2, 1, f)), 0, 1) : 1;
  float hfs2 = mix(hfs, 1, horizon_trans);

  if (hf < 0) {
  }
  else {
  }
  fragmentColor = vec4(mix(fragmentColor.xyz * vec3(1, 0.6, 0.4), fragmentColor.xyz, hfs2), 1);
  fragmentColor = vec4(mix(fragmentColor.xyz * vec3(0.1, 0.1, 0.4), fragmentColor.xyz, smoothstep(0, 1, night_trans)), 1);
  // fragmentColor = vec4(vec3(min(hfs2, night_trans)), 1);
}
