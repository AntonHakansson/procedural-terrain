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
uniform ScreenSpaceReflection ssr;

uniform mat4 inv_view_matrix;
uniform mat4 pixel_projection;  // `pixel_projection` projects from view space to pixel coordinate
uniform float environment_multiplier;

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

float linearizeDepth(float depth) {
  return 2.0 * ssr.z_near * ssr.z_far
         / (ssr.z_far + ssr.z_near - (2.0 * depth - 1.0) * (ssr.z_far - ssr.z_near));
}

#define uint1 uint
#define uint2 uvec2
#define uint3 uvec3
#define uint4 uvec4

#define int1 int
#define int2 ivec2
#define int3 ivec3
#define int4 ivec4

#define float1 float
#define float2 vec2
#define float3 vec3
#define float4 vec4

#define bool1 bool
#define bool2 bvec2
#define bool3 bvec3
#define bool4 bvec4

#define half float
#define half1 float
#define half2 vec2
#define half3 vec3
#define half4 vec4

#define Vector2 vec2
#define Point2 vec2
#define Vector3 vec3
#define Point3 vec3
#define Vector4 vec4

/**
    \param csOrigin Camera-space ray origin, which must be
    within the view volume and must have z < -0.01 and project within the valid screen rectangle

    \param csDirection Unit length camera-space ray direction

    \param projectToPixelMatrix A projection matrix that maps to pixel coordinates (not [-1, +1]
   normalized device coordinates)

    \param csZBuffer The depth or camera-space Z buffer, depending on the value of \a
   csZBufferIsHyperbolic

    \param csZBufferSize Dimensions of csZBuffer

    \param csZThickness Camera space thickness to ascribe to each pixel in the depth buffer

    \param csZBufferIsHyperbolic True if csZBuffer is an OpenGL depth buffer, false (faster) if
     csZBuffer contains (negative) "linear" camera space z values. Const so that the compiler can
   evaluate the branch based on it at compile time

    \param clipInfo See G3D::Camera documentation

    \param nearPlaneZ Negative number

    \param stride Step in horizontal or vertical pixels between samples. This is a float
     because integer math is slow on GPUs, but should be set to an integer >= 1

    \param jitterFraction  Number between 0 and 1 for how far to bump the ray in stride units
      to conceal banding artifacts

    \param maxSteps Maximum number of iterations. Higher gives better images but may be slow

    \param maxRayTraceDistance Maximum camera-space distance to trace before returning a miss

    \param hitPixel Pixel coordinates of the first intersection with the scene

    \param csHitPoint Camera space location of the ray hit
 */
bool traceScreenSpaceRay1(Point3 csOrigin, Vector3 csDirection, mat4x4 projectToPixelMatrix,
                          sampler2D csZBuffer, float2 csZBufferSize, float csZThickness,
                          const in bool csZBufferIsHyperbolic, float3 clipInfo, float nearPlaneZ,
                          float stride, float jitterFraction, float maxSteps,
                          in float maxRayTraceDistance, out Point2 hitPixel, out int which,
                          out Point3 csHitPoint) {
  // Clip ray to a near plane in 3D (doesn't have to be *the* near plane, although that would be a
  // good idea)
  float rayLength = ((csOrigin.z + csDirection.z * maxRayTraceDistance) > nearPlaneZ)
                        ? (nearPlaneZ - csOrigin.z) / csDirection.z
                        : maxRayTraceDistance;
  Point3 csEndPoint = csDirection * rayLength + csOrigin;

  // Project into screen space
  Vector4 H0 = projectToPixelMatrix * Vector4(csOrigin, 1.0);
  Vector4 H1 = projectToPixelMatrix * Vector4(csEndPoint, 1.0);

  // There are a lot of divisions by w that can be turned into multiplications
  // at some minor precision loss...and we need to interpolate these 1/w values
  // anyway.
  //
  // Because the caller was required to clip to the near plane,
  // this homogeneous division (projecting from 4D to 2D) is guaranteed
  // to succeed.
  float k0 = 1.0 / H0.w;
  float k1 = 1.0 / H1.w;

  // Switch the original points to values that interpolate linearly in 2D
  Point3 Q0 = csOrigin * k0;
  Point3 Q1 = csEndPoint * k1;

  // Screen-space endpoints
  Point2 P0 = H0.xy * k0;
  Point2 P1 = H1.xy * k1;

  // [Optional clipping to frustum sides here]

  // Initialize to off screen
  hitPixel = Point2(-1.0, -1.0);
  which = 0;  // Only one layer

  // If the line is degenerate, make it cover at least one pixel
  // to avoid handling zero-pixel extent as a special case later
  P1 += vec2((distanceSquared(P0, P1) < 0.0001) ? 0.01 : 0.0);

  Vector2 delta = P1 - P0;

  // Permute so that the primary iteration is in x to reduce
  // large branches later
  bool permute = false;
  if (abs(delta.x) < abs(delta.y)) {
    // More-vertical line. Create a permutation that swaps x and y in the output
    permute = true;

    // Directly swizzle the inputs
    delta = delta.yx;
    P1 = P1.yx;
    P0 = P0.yx;
  }

  // From now on, "x" is the primary iteration direction and "y" is the secondary one

  float stepDirection = sign(delta.x);
  float invdx = stepDirection / delta.x;
  Vector2 dP = Vector2(stepDirection, invdx * delta.y);

  // Track the derivatives of Q and k
  Vector3 dQ = (Q1 - Q0) * invdx;
  float dk = (k1 - k0) * invdx;

  // Scale derivatives by the desired pixel stride
  dP *= stride;
  dQ *= stride;
  dk *= stride;

  // Offset the starting values by the jitter fraction
  P0 += dP * jitterFraction;
  Q0 += dQ * jitterFraction;
  k0 += dk * jitterFraction;

  // Slide P from P0 to P1, (now-homogeneous) Q from Q0 to Q1, and k from k0 to k1
  Point3 Q = Q0;
  float k = k0;

  // We track the ray depth at +/- 1/2 pixel to treat pixels as clip-space solid
  // voxels. Because the depth at -1/2 for a given pixel will be the same as at
  // +1/2 for the previous iteration, we actually only have to compute one value
  // per iteration.
  float prevZMaxEstimate = csOrigin.z;
  float stepCount = 0.0;
  float rayZMax = prevZMaxEstimate, rayZMin = prevZMaxEstimate;
  float sceneZMax = rayZMax + 1e4;

  // P1.x is never modified after this point, so pre-scale it by
  // the step direction for a signed comparison
  float end = P1.x * stepDirection;

  // We only advance the z field of Q in the inner loop, since
  // Q.xy is never used until after the loop terminates.

  for (Point2 P = P0;
       ((P.x * stepDirection) <= end) && (stepCount < maxSteps)
       && ((rayZMax < sceneZMax - csZThickness) || (rayZMin > sceneZMax)) && (sceneZMax != 0.0);
       P += dP, Q.z += dQ.z, k += dk, stepCount += 1.0) {
    hitPixel = permute ? P.yx : P;

    // The depth range that the ray covers within this loop
    // iteration.  Assume that the ray is moving in increasing z
    // and swap if backwards.  Because one end of the interval is
    // shared between adjacent iterations, we track the previous
    // value and then swap as needed to ensure correct ordering
    rayZMin = prevZMaxEstimate;

    // Compute the value at 1/2 pixel into the future
    rayZMax = (dQ.z * 0.5 + Q.z) / (dk * 0.5 + k);
    prevZMaxEstimate = rayZMax;
    if (rayZMin > rayZMax) {
      swap(rayZMin, rayZMax);
    }

    // Camera-space z of the background
    sceneZMax = texelFetch(csZBuffer, int2(hitPixel), 0).r;

    // This compiles away when csZBufferIsHyperbolic = false
    if (csZBufferIsHyperbolic) {
      // sceneZMax = reconstructCSZ(sceneZMax, clipInfo);
      sceneZMax = linearizeDepth(sceneZMax);
    }
    sceneZMax *= -1;
  }  // pixel on ray

  Q.xy += dQ.xy * stepCount;
  csHitPoint = Q * (1.0 / k);

  // Matches the new loop condition:
  return (rayZMax >= sceneZMax - csZThickness) && (rayZMin <= sceneZMax);
}

// paper approach
bool trace(vec3 ray_origin, vec3 ray_dir, ScreenSpaceReflection ssr, mat4 pixel_projection,
           out vec2 hit_pixel, out vec3 hit_point) {
  // Clip to the near plane
  float ray_length = ((ray_origin.z + ray_dir.z * ssr.max_distance) > ssr.z_near)
                         ? (ssr.z_near - ray_origin.z) / ray_dir.z
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

    {
      float z_b = texelFetch(depth_buffer, ivec2(hit_pixel), 0).r;
      float z_n = 2.0 * z_b - 1.0;
      float z_e = 2.0 * ssr.z_near * ssr.z_far
                  / (ssr.z_far + ssr.z_near - z_n * (ssr.z_far - ssr.z_near));
      scene_z_max = z_e;
    }

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
  if (debug_flag == 1 || debug_flag == 2) {
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

void main() {
  // REVIEW: Does `texture` filter the result? maybe opt for `texelFetch` to get unfiltered value
  vec3 color = texelFetch(pixel_buffer, ivec2(gl_FragCoord.xy), 0).xyz;

  vec3 point_on_water = In.view_space_position;

  // NOTE: The depth buffer is hyperbolic i.e. not linear
  float depth = texelFetch(depth_buffer, ivec2(gl_FragCoord.xy), 0).x;
  float terrain_depth = linearizeDepth(depth);
  float plane_depth = -point_on_water.z;

  float diff_depth = abs(terrain_depth - plane_depth);

  vec3 view_dir = normalize(In.view_space_position);

  vec3 dudv = getDuDv(In.world_pos.xz);
  vec3 reflection_dir = normalize(reflect(view_dir, In.view_space_normal));
  vec3 refraction_dir = normalize(reflection_dir - 2 * normalize(In.view_space_normal));

  reflection_dir = normalize(reflection_dir + dudv);
  refraction_dir = normalize(refraction_dir + dudv);

  vec3 out_color = color;

  // foam
  float foam_mask;
  float ocean_mask;

  foam_mask += max(1.0 - (diff_depth) / water.foam_distance, 0);
  foam_mask *= max(sin((diff_depth / 1.5 + current_time * 4 + dudv.y * 50) / 2), 0);
  foam_mask += max(1.0 - (diff_depth - water.foam_distance / 4.0) / (water.foam_distance * 0.5), 0);
  if (diff_depth < water.foam_distance / 5.0)
    foam_mask += inverseLerp(water.foam_distance / 5.0, 0, diff_depth);

  ocean_mask = clamp((diff_depth - 100) / 1200.0, 0.0, 0.12) / 0.18;

// reflection based on paper
// -----------
#if 0
  {
    vec2 hit_pixel;
    vec3 view_hit_point;
    bool reflection_hit = trace(point_on_water, reflection_dir, ssr, pixel_projection, hit_pixel, view_hit_point);
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
    bool reflection_hit = traceScreenSpaceRay1(
        point_on_water, reflection_dir, pixel_projection, depth_buffer, vec2(ssr.depth_buffer_size),
        ssr.z_thickness, true, vec3(0), -ssr.z_near, ssr.stride, ssr.jitter, ssr.max_steps,
        ssr.max_distance, hit_pixel, which, view_hit_point);

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
    }
    if (debug_flag == 1) {
        fragmentColor = vec4(reflection_color, 1.0);
        return;
    }
  }

  vec3 refraction_color = vec3(0);
  {
    vec2 hit_pixel;
    int which;
    vec3 view_hit_point;
    bool reflection_hit = traceScreenSpaceRay1(
        point_on_water, refraction_dir, pixel_projection, depth_buffer, vec2(ssr.depth_buffer_size),
        ssr.z_thickness, true, vec3(0), -ssr.z_near, ssr.stride, ssr.jitter, ssr.max_steps,
        ssr.max_distance, hit_pixel, which, view_hit_point);

    if (reflection_hit) {
      refraction_color = texelFetch(pixel_buffer, ivec2(hit_pixel), 0).rgb;
    } else {
      // REVIEW: is there some hack here?
      refraction_color = color;
    }
    if (debug_flag == 2) {
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
