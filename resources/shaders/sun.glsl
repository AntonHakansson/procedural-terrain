#ifndef _SUN_H_
#define _SUN_H_

#include "utils.glsl"

struct Sun {
  vec3 direction;
  vec3 view_space_direction;
  vec3 color;
  float intensity;
};

#endif // _SUN_H_
