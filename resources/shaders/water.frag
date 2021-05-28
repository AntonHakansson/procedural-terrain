#version 420

#define PI 3.14159265359

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

// Implementation based on this paper: http://jcgt.org/published/0003/04/04/paper.pdf
// blogpost about paper: https://casual-effects.blogspot.com/2014/08/screen-space-ray-tracing.html
//
// Also relevant: http://roar11.com/2015/07/screen-space-glossy-reflections/

// Inputs
layout(binding = 0) uniform sampler2D pixel_buffer;
layout(binding = 1) uniform sampler2D depth_buffer;
layout(binding = 2) uniform sampler2D dudv_map;

layout(binding = 6) uniform sampler2D environment_map;

// Output
layout(location = 0) out vec4 fragmentColor;

// Data
in DATA {
  vec2 tex_coord;
  vec3 view_space_normal;
  vec3 view_space_position;
  vec3 world_pos;
}
In;

// Uniforms
uniform float current_time;

struct Sun {
  vec3 direction;
  vec3 color;
};
uniform Sun sun;

struct Water {
  float height;
  float foam_distance;
  float wave_speed;
  float wave_strength;
  float wave_scale;
  float size;
};
uniform Water water;

struct ScreenSpaceReflection {
  ivec2 depth_buffer_size;
  float z_near;
  float z_far;

  // Thickness to assign each pixel in the depth buffer
  float z_thickness;
  // The camera-space distance to step in each iteration
  float stride;
  // Number between 0 and 1 for how far to bump the ray in stride units to conceal banding artifacts
  float jitter;
  // Max number of trace iterations
  float max_steps;
  // Maximum camera-space distance to trace before returning a miss
  float max_distance;
};
uniform ScreenSpaceReflection ssr_reflection;
uniform ScreenSpaceReflection ssr_refraction;

uniform mat4 inv_view_matrix;
uniform mat4 pixel_projection;  // `pixel_projection` projects from view space to pixel coordinate
uniform float environment_multiplier;

#define DEBUG_OFF 0
#define DEBUG_SSR_REFLECTION 1
#define DEBUG_SSR_REFRACTION 2
#define DEBUG_SSR_REFRACTION_MISSES 3
uniform int debug_flag; // 0 = off; 1 = screen space reflection; 2 = screen space refraction;

// Constants
const vec3 ocean_blue = vec3(0.1, 0.3, 0.6);
const vec3 ocean_blue_deep = vec3(0.05, 0.1, 0.2);

// Utilities
#define point2 vec2
#define point3 vec3

void swap(inout float a, inout float b) {
  float t = a;
  a = b;
  b = t;
}

float distanceSquared(vec2 a, vec2 b) {
  a -= b;
  return dot(a, a);
}
float distanceSquared(vec3 a, vec3 b) {
  a -= b;
  return dot(a, a);
}

float linearizeDepth(float depth, float near, float far) {
  return 2.0 * near * far / (far + near - (2.0 * depth - 1.0) * (far - near));
}

/**
    \param csOrigin Camera-space ray origin, which must be
    within the view volume and must have z < -0.01 and project within the valid screen rectangle

    \param csDirection Unit length camera-space ray direction

    \param pixel_projection A projection matrix that maps to pixel coordinates (not [-1, +1]
   normalized device coordinates)

    \param hitPixel Pixel coordinates of the first intersection with the scene

    \param csHitPoint Camera space location of the ray hit
 */
bool traceScreenSpaceRay(vec3 ray_origin, vec3 ray_dir, mat4 pixel_projection, sampler2D depth_buffer, ScreenSpaceReflection ssr,
           out vec2 hit_pixel, out vec3 hit_point) {
  // Flip z near in camera direction
  float z_near = -ssr.z_near;

  // Clip to the near plane
  float ray_length = ((ray_origin.z + ray_dir.z * ssr.max_distance) > z_near)
                         ? (z_near - ray_origin.z) / ray_dir.z
                         : ssr.max_distance;
  vec3 end_point = ray_origin + ray_dir * ray_length;

  // Init to off screen
  hit_pixel = vec2(-1.0);

  // Project into homogeneous clip space
  vec4 H0 = pixel_projection * vec4(ray_origin, 1.0);
  vec4 H1 = pixel_projection * vec4(end_point, 1.0);

  float k0 = 1.0 / H0.w;
  float k1 = 1.0 / H1.w;

  // The interpolated homogeneous version of the camera-space points
  vec3 Q0 = ray_origin * k0;
  vec3 Q1 = end_point * k1;

  // Screen-space endpoints
  vec2 P0 = H0.xy * k0;
  vec2 P1 = H1.xy * k1;

  P1 += vec2(distanceSquared(P0, P1) < 0.0001 ? 0.01 : 0.0);
  vec2 delta = P1 - P0;

  // the primary iteration is in x direction so permute P if vertical line
  bool permute = false;
  if (abs(delta.x) < abs(delta.y)) {
    // more-vertical line
    permute = true;
    delta = delta.yx;
    P0 = P0.yx;
    P1 = P1.yx;
  }

  float step_dir = sign(delta.x);
  float invdx = step_dir / delta.x;

  // derivatives of Q and k
  vec3 dQ = (Q1 - Q0) * invdx;
  float dk = (k1 - k0) * invdx;
  vec2 dP = vec2(step_dir, delta.y * invdx);

  //
  dP *= ssr.stride;
  dQ *= ssr.stride;
  dk *= ssr.stride;

  // Offset starting point by the jitter
  P0 += dP * ssr.jitter;
  Q0 += dQ * ssr.jitter;
  k0 += dk * ssr.jitter;

  // Slide P from P0 to P1, Q from Q0 to Q1, k from k0 to k1
  vec2 P = P0;
  vec3 Q = Q0;
  float k = k0;

  float end = P1.x * step_dir;

  float step_count = 0.0;
  float prev_z_max_estimate = ray_origin.z;
  float ray_z_min = prev_z_max_estimate;
  float ray_z_max = prev_z_max_estimate;
  float scene_z_max = ray_z_max + 1e4;

  // trace until intersection reached max steps
  while (((P.x * step_dir) <= end) && (step_count < ssr.max_steps)
         && ((ray_z_max < (scene_z_max - ssr.z_thickness)) || (ray_z_min > scene_z_max))
         && (scene_z_max != 0.0)) {
    ray_z_min = prev_z_max_estimate;
    ray_z_max = (dQ.z * 0.5 + Q.z) / (dk * 0.5 + k);
    prev_z_max_estimate = ray_z_max;

    if (ray_z_min > ray_z_max) {
      swap(ray_z_min, ray_z_max);
    }

    hit_pixel = permute ? P.yx : P;

    scene_z_max = -linearizeDepth(texelFetch(depth_buffer, ivec2(hit_pixel), 0).r, ssr.z_near, ssr.z_far);

    // Step
    P += dP;
    Q.z += dQ.z;
    k += dk;
    step_count++;
  }

  // Advance Q based on the number of steps
  Q.xy += dQ.xy * step_count;
  hit_point = Q * (1.0 / k);
  return (ray_z_max >= scene_z_max - ssr.z_thickness) && (ray_z_min < scene_z_max);
}

vec3 getDuDv(vec2 tex_coord) {
  if (debug_flag == DEBUG_SSR_REFLECTION) {
      return vec3(0);
  }

  vec3 dudv_x
      = texture(dudv_map, (tex_coord / water.wave_scale) + vec2(current_time * water.wave_speed, 0))
            .rgb;
  vec3 dudv_y
      = texture(dudv_map, (tex_coord / water.wave_scale) + vec2(0, current_time * water.wave_speed))
            .rgb;
  dudv_x = (dudv_x * 2.0 - 1.0);
  dudv_y = (dudv_y * 2.0 - 1.0);
  vec3 dudv = normalize(dudv_x + dudv_y) * water.wave_strength;
  return dudv;
}

float inverseLerp(float a, float b, float x) { return (x - a) / (b - a); }
float inverseLerpClamped(float a, float b, float x) { return clamp(inverseLerp(a, b, x), 0, 1); }

void main() {
  vec3 color = texelFetch(pixel_buffer, ivec2(gl_FragCoord.xy), 0).xyz;

  vec3 point_on_water = In.view_space_position;

  // NOTE: The depth buffer is hyperbolic i.e. not linear
  float depth = texelFetch(depth_buffer, ivec2(gl_FragCoord.xy), 0).x;
  float terrain_depth = linearizeDepth(depth, ssr_reflection.z_near, ssr_reflection.z_far);
  float plane_depth = -point_on_water.z;

  float diff_depth = abs(terrain_depth - plane_depth);

  vec3 view_dir = normalize(In.view_space_position);

  vec3 dudv = getDuDv(In.world_pos.xz);
  vec3 reflection_dir = normalize(reflect(view_dir, In.view_space_normal));
  vec3 refraction_dir = normalize(refract(view_dir, In.view_space_normal, 0.8));

  reflection_dir = normalize(reflection_dir + dudv);
  refraction_dir = normalize(refraction_dir + dudv);

  vec3 out_color = color;

  // foam
  float ocean_mask = clamp((diff_depth - 100) / 1200.0, 0.0, 0.12) / 0.18;

  float foam_mask;
  foam_mask += max(1.0 - diff_depth / water.foam_distance, 0);
  foam_mask *= max(sin((diff_depth / 1.5 + current_time * 8 + dudv.y * water.wave_scale / 8) / 2) * 1.5, 0);
  foam_mask += max(1.0 - (diff_depth - water.foam_distance / 4.0) / (water.foam_distance * 0.6), 0);
  
  if (diff_depth < water.foam_distance / 3.0) {
    foam_mask += inverseLerp(water.foam_distance / 3.0, 0.4, diff_depth) * 1.5;
  }


// reflection based on paper
// -----------
#if 0
  {
    vec2 hit_pixel;
    vec3 view_hit_point;
    bool reflection_hit = traceScreenSpaceRay(point_on_water, reflection_dir, ssr_reflection, pixel_projection, hit_pixel, view_hit_point);
    vec4 reflection_color = texelFetch(pixel_buffer, ivec2(hit_pixel), 0);
  }
#endif

// reflection with paper impl
// -----------
#if 1
  vec3 reflection_color = vec3(0);
  {
    vec2 hit_pixel;
    int which;
    vec3 view_hit_point;
    bool reflection_hit = traceScreenSpaceRay(point_on_water, reflection_dir, pixel_projection, depth_buffer, ssr_reflection, hit_pixel, view_hit_point);

    if (reflection_hit) {
      reflection_color = texelFetch(pixel_buffer, ivec2(hit_pixel), 0).rgb;
    } else {
      vec3 world_reflection_dir = normalize((inv_view_matrix * vec4(reflection_dir, 0.0)).xyz);

      // Calculate the spherical coordinates of the direction
      float theta = acos(max(-1.0f, min(1.0f, world_reflection_dir.y)));
      float phi = atan(world_reflection_dir.z, world_reflection_dir.x);

      if (phi < 0.0f) phi = phi + 2.0f * PI;

      // Use these to lookup the color in the environment map
      vec2 lookup = vec2(phi / (2.0 * PI), theta / PI);
      reflection_color = texture(environment_map, lookup).xyz * environment_multiplier;


      float sun_threshold = 0.998;
      float wdots = max(dot(world_reflection_dir, -sun.direction), 0.0);

      // reflection_color = vec3(wdots);
      float f1 = smoothstep(0.0, 1.0,
                            inverseLerpClamped(sun_threshold, (1 + sun_threshold) / 2.0, wdots));
      float f2 = smoothstep(0.2, 1.0, inverseLerpClamped(sun_threshold, 1, wdots));

      reflection_color += vec3(sun.color * f1 * 2 + vec3(1) * f2 * 4.5) * 2;
    }
    if (debug_flag == DEBUG_SSR_REFLECTION) {
        fragmentColor = vec4(reflection_color, 1.0);
        return;
    }
  }

  vec3 refraction_color = vec3(0);
  {
    vec2 hit_pixel;
    int which;
    vec3 view_hit_point;
    bool refraction_hit = traceScreenSpaceRay(point_on_water, refraction_dir, pixel_projection, depth_buffer, ssr_refraction, hit_pixel, view_hit_point);

    if (refraction_hit) {
      refraction_color = texelFetch(pixel_buffer, ivec2(hit_pixel), 0).rgb;
    } else {
      // REVIEW: is there some other hack here?
      ivec2 offset_tex_coord = ivec2(gl_FragCoord.xy) + ivec2(vec2(dudv) * textureSize(pixel_buffer, 0));
      // vec3 ss_refraction_dir = normalize(pixel_projection * vec4(refraction_dir, 0.0)).xyz;
      // offset_tex_coord = ivec2(gl_FragCoord.xy) + ivec2(ss_refraction_dir.xy)*5;
      // offset_tex_coord = ivec2(gl_FragCoord.xy);
      vec3 offset_color = texelFetch(pixel_buffer, offset_tex_coord, 0).xyz;
      refraction_color = debug_flag == DEBUG_SSR_REFRACTION_MISSES ? vec3(0) : offset_color;
    }
    if (debug_flag == DEBUG_SSR_REFRACTION || debug_flag == DEBUG_SSR_REFRACTION_MISSES) {
        fragmentColor = vec4(refraction_color, 1.0);
        return;
    }
  }

  vec3 fresnel_color
      = mix(reflection_color, refraction_color, max(-dot(view_dir, In.view_space_normal), 0.0));

  out_color = mix(fresnel_color, ocean_blue, 0.5);
  out_color = out_color + (foam_mask * 0.3);

  out_color = mix(out_color.xyz, ocean_blue_deep, ocean_mask);
#endif

  fragmentColor = vec4(out_color, 1.0);
}
