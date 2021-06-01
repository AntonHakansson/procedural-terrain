#ifndef _UTILS_H_
#define _UTILS_H_

#ifndef PI
#  define PI 3.14159265359
#endif

float inverseLerp(float a, float b, float x) { return (x - a) / (b - a); }
float inverseLerpClamped(float a, float b, float x) { return clamp(inverseLerp(a, b, x), 0, 1); }

float linearizeDepth(float depth, float near, float far) {
  return 2.0 * near * far / (far + near - (2.0 * depth - 1.0) * (far - near));
}

// Calculate the spherical coordinates of the direction in world space
vec2 sphericalCoordinate(vec3 world_dir) {
  float theta = acos(max(-1.0f, min(1.0, world_dir.y)));
  float phi = atan(world_dir.z, world_dir.x);
  if (phi < 0.0) {
    phi = phi + 2.0 * PI;
  }

  return vec2(phi / (2.0 * PI), theta / PI);
}

#endif  // _UTILS_H_
