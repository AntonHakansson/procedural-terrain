#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

// Inputs
layout(location = 0) out vec4 fragmentColor;
layout(binding = 0) uniform sampler2D tex;
layout(binding = 1) uniform sampler2D depth_tex;

// Data
in DATA {
  vec2 tex_coord;
  vec3 view_pos;
  vec3 world_pos;
  vec3 world_dir;
}
In;

// Uniforms
uniform float currentTime;
uniform mat4 viewMatrix;
uniform mat4 projMatrix;

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
  int debug_mask;
};
uniform PostFX postfx;

#define DEBUG_MASK_OFF 0
#define DEBUG_MASK_HORIZON 1
#define DEBUG_MASK_GOD_RAY 2

// Constants
const float Epsilon = 1e-10;
const vec3 ocean_blue_deep = vec3(24.0 / 255.0, 43.0 / 255.0, 78.0 / 255.0);

// HSV RGB conversion
// http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl.
// Licensed under the WTFPL.
// All components are in the range [0…1], including hue.
vec3 rgb2hsv(vec3 c) {
  vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
  vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
  vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

  float d = q.x - min(q.w, q.y);
  float e = 1.0e-10;
  return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

// All components are in the range [0…1], including hue.
vec3 hsv2rgb(vec3 c) {
  vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
  return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 shiftHSV(in vec3 in_col, float hue, float saturate, float value) {
  vec3 col_hsv = rgb2hsv(in_col);
  col_hsv.x += hue;
  col_hsv.y += saturate;
  col_hsv.z += value;

  return hsv2rgb(col_hsv.xyz);
}

// Utilities
float linearizeDepth(float depth) {
  return 2.0 * postfx.z_near * postfx.z_far
         / (postfx.z_far + postfx.z_near - (2.0 * depth - 1.0) * (postfx.z_far - postfx.z_near));
}

float inverseLerp(float a, float b, float x) { return (x - a) / (b - a); }
float inverseLerpClamped(float a, float b, float x) { return clamp(inverseLerp(a, b, x), 0, 1); }

float calculateGodMask(float depth, float wdots, float sun_threshold) {
  float linear_depth = linearizeDepth(depth);
  if (linear_depth == postfx.z_far) {
    if (wdots > sun_threshold) return 1.0;

    return smoothstep(0.5, 1.0, wdots);
  }

  return 0;
}

// Render
void main() {
  vec3 view_dir = normalize(In.view_pos);
  vec3 world_dir = normalize(In.world_dir);

  float height = In.world_pos.y;

  // Underwater filter
  if (height < water.height) {
    vec3 col = texture(tex, In.tex_coord + vec2(0, sin(currentTime) * 0.01)).xyz;
    vec3 out_color = mix(col, ocean_blue_deep, 0.9);
    out_color = shiftHSV(out_color, 0.0, 0.15, 0.0);

    fragmentColor = vec4(out_color, 1);

    return;
  }

  vec3 out_color = texture(tex, In.tex_coord).xyz;
  float depth = texture(depth_tex, In.tex_coord).x;
  float linear_depth = linearizeDepth(depth);

  // Increase saturation for more vididness
  out_color = shiftHSV(out_color, 0.0, 0.1, 0.0);

  /*
   * Procedural sun and horizon
   */
  float wdots = max(dot(world_dir, -sun.direction), 0.0);
  float hf = -sun.direction.y;
  float sunset_trans = inverseLerpClamped(0.2, 0.6, hf);
  float night_trans = inverseLerpClamped(-0.4, 0.45, hf);

  vec3 sun_color = mix(shiftHSV(sun.color, 0.0, 0.3, 0.0), sun.color, sunset_trans);

  float sun_threshold = 0.998;

  if (wdots > sun_threshold) {
    // Render sun
    if (linear_depth == postfx.z_far) {
      float f1 = smoothstep(0.0, 1.0,
                            inverseLerpClamped(sun_threshold, (1 + sun_threshold) / 2.0, wdots));
      float f2 = smoothstep(0.2, 1.0, inverseLerpClamped(sun_threshold, 1, wdots));

      out_color += vec3(sun_color * f1 + vec3(1) * f2 * 1.5);
    } else {
      // Sample some nearby pixels to give some "bloom" onto the mountaintops
      float size = 3;
      int maxBound = int(floor(size / 2));
      int minBound = -int(floor((size - 0.01) / 2));

      // PCF?
      float percentage_lit = 0;
      for (int k = minBound; k <= maxBound; k++) {
        for (int l = minBound; l <= maxBound; l++) {
          vec2 texel = vec2(1 / 128.0, 1 / 128.0);
          vec2 offset = vec2(texel * vec2(l, k));

          float depth = texture(depth_tex, In.tex_coord + offset).x;
          float linear_depth = linearizeDepth(depth);

          if (wdots > sun_threshold && linear_depth == postfx.z_far) {
            percentage_lit++;
          }
        }
      }

      // PCF!
      float average = percentage_lit / (size * size);

      float f1 = smoothstep(0, 1, inverseLerpClamped(sun_threshold, 1, wdots));
      out_color += vec3(sun_color * average * f1);
    }
  }

  // Calculate horizon mask
  float horizon_mask = 1;
  if (linear_depth == postfx.z_far) {
    horizon_mask = clamp(
        inverseLerpClamped(0, 0.8, world_dir.y) + (inverseLerpClamped(sun_threshold, 1, wdots)), 0,
        1);
  }

  float horizon_sunset_mask = mix(horizon_mask, 1, sunset_trans);

  if (postfx.debug_mask == DEBUG_MASK_HORIZON) {
    fragmentColor = vec4(vec3(horizon_sunset_mask), 1);
    return;
  }

  // Apply sunset and night colors
  vec3 sunset_color = shiftHSV(sun_color, 0, -0.6, 0.0);
  vec3 night_color = shiftHSV(sun_color, -0.42, -0.3, -0.5);
  out_color = mix(out_color * sunset_color, out_color, horizon_sunset_mask);
  out_color = mix(out_color * night_color, out_color, smoothstep(0, 1, night_trans));

  /**
   * God rays
   */
  vec2 screen_pos = vec2(In.tex_coord) * 2.0 - 1;

  // Calculate sun position in screen space coordinates
  vec3 sun_dir_view = normalize((viewMatrix * vec4(-sun.direction, 0.0)).xyz);
  vec4 sun_dir_proj = projMatrix * vec4(sun_dir_view, 0);
  vec2 sun_pos_screen = (sun_dir_proj.xy / sun_dir_proj.z);

  vec2 ray_dir = sun_pos_screen - screen_pos;
  float ray_dist = length(ray_dir);
  ray_dir = normalize(ray_dir);

  const uint NUM_RAY_STEPS = 40;
  float step_size = ray_dist / float(NUM_RAY_STEPS);

  // March towards the sun in screen space
  float visibility_factor = 0;
  for (uint i = 0; i < NUM_RAY_STEPS; ++i) {
    vec2 coords = 0.5 * (screen_pos + ray_dir * step_size * i + 1);

    // if (coords.x < 0 || coords.x > 1 || coords.y < 0 || coords.y > 1) continue;

    float depth = texture(depth_tex, coords).x;
    float god_mask = calculateGodMask(depth, wdots, sun_threshold);

    visibility_factor += god_mask;
  }
  visibility_factor /= float(NUM_RAY_STEPS);

  vec3 god_ray_color = mix(sunset_color, sunset_color, sunset_trans);
  out_color += god_ray_color * visibility_factor * (1 - sunset_trans * 0.4)
               * clamp((1 - ray_dist / 1), 0.0, 1.0) * 0.6;

  if (postfx.debug_mask == DEBUG_MASK_GOD_RAY) {
    fragmentColor = vec4(vec3(visibility_factor), 1);
    return;
  }

  // Finally, update
  fragmentColor = vec4(out_color, 1.0);
}
