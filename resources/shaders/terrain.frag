#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

#include "pbr.glsl"

in DATA {
  vec3 world_pos;
  vec3 view_pos;
  vec4 shadow_coord;
  vec2 tex_coord;
  vec3 normal;
  vec3 view_normal;
  vec3 tangent;
  vec3 bitangent;
  mat3 tangent_matrix;
}
In;

layout(binding = 0) uniform sampler2DArray albedos;
layout(binding = 1) uniform sampler2DArray normals;
layout(binding = 2) uniform sampler2DArray displacement;
layout(binding = 3) uniform sampler2DArray roughness;
layout(binding = 4) uniform sampler2DArray ambient_occlusion;

layout(binding = 7) uniform sampler2D irradiance_map;
layout(binding = 8) uniform sampler2D reflection_map;
uniform float environment_multiplier;

uniform vec3 eyeWorldPos;
uniform mat4 viewMatrix;
uniform mat4 viewInverse;

/**
 * Noise
 */
struct Noise {
  int num_octaves;
  float amplitude;
  float frequency;
  float persistence;
  float lacunarity;
};
uniform Noise noise;

uniform float texture_start_heights[4];
uniform float texture_blends[4];
uniform float texture_sizes[4];
uniform float texture_displacement_weights[4];

/**
 * Cascading shadow map
 */
const int NUM_CASCADES = 3;

struct ShadowMap {
  float cascade_clip_splits[NUM_CASCADES];
  mat4 light_wvp_matrix[NUM_CASCADES];
  float blend_distance;
  bool debug_show_splits;
  bool debug_show_blend;
};
uniform ShadowMap shadow_map;

in vec4 shadow_light_pos[NUM_CASCADES];
in float shadow_clip_depth;

layout(binding = 10) uniform sampler2DArrayShadow shadow_tex;

/**
 * Sun
 */
struct Sun {
  vec3 direction;
  vec3 color;
  float intensity;
};
uniform Sun sun;

/**
 * Output
 */
layout(location = 0) out vec4 fragmentColor;

/**
 * Functions
 */

// Triplanar mapping of texture as naively projecting the texture on to the xz
// plane will result in texture-stretching at steep angles
// REVIEW: Can get the texture coordinates from the tessellation shader instead?
vec3 triplanarSampling(sampler2D tex, vec3 worldpos, vec3 normal) {
  vec3 scaled_worldpos = worldpos / (2048.0 / 4.0);

  vec3 blend_axis = abs(normal);
  blend_axis /= blend_axis.x + blend_axis.y + blend_axis.z;

  vec3 x_projection = texture(tex, scaled_worldpos.yz).xyz * blend_axis.x;
  vec3 y_projection = texture(tex, scaled_worldpos.xz).xyz * blend_axis.y;
  vec3 z_projection = texture(tex, scaled_worldpos.xy).xyz * blend_axis.z;

  // return x_projection + y_projection + z_projection;
  return texture(tex, scaled_worldpos.xz).xyz;
}

float inverseLerp(float a, float b, float x) { return (x - a) / (b - a); }
float inverseLerpClamped(float a, float b, float x) { return clamp(inverseLerp(a, b, x), 0, 1); }

vec3 getWorldNormal(vec3 tex_coord) {
  vec3 tangent_normal = normalize(2.0 * texture(normals, tex_coord).xyz - 1.0);
  return In.tangent_matrix * tangent_normal;
}

float[4] getTextureWeightsByDisplacement(vec3[4] t, float[4] a) {
  const float depth = 0.1;

  float disp[t.length];
  for (int i = 0; i < t.length; i++) {
    disp[i] = texture(displacement, t[i]).r * texture_displacement_weights[i];
  }

  float ma = 0;
  for (int i = 0; i < t.length; i++) {
    ma = max(ma, disp[i] + a[i]);
  }
  ma -= depth;

  float total_w = 0;
  float[t.length] w;
  for (int i = 0; i < t.length; i++) {
    w[i] = max(disp[i] + a[i] - ma, 0);
    total_w += w[i];
  }

  // normalize
  for (int i = 0; i < t.length; i++) {
    w[i] /= total_w;
  }

  return w;
}

float[4] terrainBlending(vec3 world_pos, vec3 normal) {
  float height = world_pos.y;

  // A completely flat terrain has slope=0
  float slope = max(1 - dot(normal, vec3(0, 1, 0)), 0.0);

  // For each fragment we compute how much each texture contributes depending on height and slope
  float draw_strengths[4];

  float sand_grass_height = texture_start_heights[0];
  float grass_rock_height = texture_start_heights[1];
  float rock_snow_height = texture_start_heights[2];

  float grass_falloff = texture_blends[1];
  float rock_falloff = texture_blends[2];
  float snow_falloff = texture_blends[3];

  float a, b, c, d;
  {
    b = inverseLerpClamped(sand_grass_height - grass_falloff / 2,
                           sand_grass_height + grass_falloff / 2, height);
  }
  {
    c = inverseLerpClamped(grass_rock_height - rock_falloff / 2,
                           grass_rock_height + rock_falloff / 2, height);
  }

  float b_in, c_in, d_in;
  b_in = inverseLerpClamped(sand_grass_height - grass_falloff / 2,
                            sand_grass_height + grass_falloff / 2, height);
  c_in = inverseLerpClamped(grass_rock_height - rock_falloff / 2,
                            grass_rock_height + rock_falloff / 2, height);
  d_in = inverseLerpClamped(rock_snow_height - snow_falloff / 2,
                            rock_snow_height + snow_falloff / 2, height);

  a = 1 - b_in;
  b *= 1 - c_in;
  c *= 1 - d_in;
  d = d_in;

  // a *= 1 - inverseLerpClamped(0.1, 1, slope);
  b *= 1 - inverseLerpClamped(0.1, 1.0, slope);
  d *= 1 - inverseLerpClamped(0.1, 1, slope);

  float tot = a + b + c + d;
  draw_strengths[0] = max(a / tot, 0);
  draw_strengths[1] = max(b / tot, 0);
  draw_strengths[2] = max(c / tot, 0);
  draw_strengths[3] = max(d / tot, 0);

  return draw_strengths;
}

vec3 getTextureCoordinate(vec3 world_pos, int texture_index) {
  return vec3(In.tex_coord * texture_sizes[texture_index], texture_index);
}

vec3 terrainNormal(vec3 world_pos, vec3 normal) {
  float[] draw_strengths = terrainBlending(world_pos, normal);

  vec3 norm = vec3(0);

  for (int i = 0; i < 4; i++) {
    float d = draw_strengths[i];
    vec3 normal = normalize(2.0 * texture(normals, getTextureCoordinate(world_pos, i)).xyz - 1.0);
    norm = norm * (1 - draw_strengths[i]) + (In.tangent_matrix * normal) * d;
  }

  return norm;
}

vec3 ambient() {
  vec3 ambient = vec3(1) * sun.intensity;
  return ambient;
}

vec3 diffuse(vec3 world_pos, vec3 normal) {
  float diffuse_factor = max(0.0, dot(-sun.direction, normal));
  vec3 diffuse = diffuse_factor * vec3(1) * sun.intensity;

  return diffuse;
}

float calcShadowFactor(int index, vec4 shadow_light_pos, vec3 normal) {
  vec3 ProjCoords = shadow_light_pos.xyz / shadow_light_pos.w;

  float ndotl = dot(normal, -sun.direction);

  float l = clamp(smoothstep(0.0, 0.2, ndotl), 0, 1);

  vec2 UVCoords;
  UVCoords.x = 0.5 * ProjCoords.x + 0.5;
  UVCoords.y = 0.5 * ProjCoords.y + 0.5;

  float z = 0.5 * ProjCoords.z + 0.5;

  float bias = 0.005;

  float percentLit = 0;
  float size = 5;
  int maxBound = int(floor(size / 2));
  int minBound = -int(floor((size - 0.01) / 2));

  // PCF sampling
  for (int k = minBound; k <= maxBound; k++) {
    for (int l = minBound; l <= maxBound; l++) {
      vec2 texel = vec2(1 / 2048.0, 1 / 2048.0);
      vec2 offset = vec2(texel * vec2(l, k));

      // x, y, level, depth
      float visibility = texture(shadow_tex, vec4(UVCoords + offset, index, (z - bias)));

      percentLit += visibility;
    }
  }

  return mix(0.5, 1.0, percentLit / (size * size) * l);
}

void main() {
  vec3 out_color = vec3(0);

  float shadow_factor = 0.0;
  vec3 cascade_indicator = vec3(0.0, 0.0, 0);

  {
    float prev_shadow_factor = 0;
    vec3 prev_cascade_color = cascade_indicator;

    for (int i = 0; i < NUM_CASCADES; i++) {
      float end = shadow_map.cascade_clip_splits[i];
      float prev_end = i == 0 ? 0 : shadow_map.cascade_clip_splits[i - 1];

      vec3 indicator_color = vec3(1, 0, 0);
      if (i == 1)
        indicator_color = vec3(0, 1, 0);
      else if (i == 2)
        indicator_color = vec3(0, 0, 1);

      if (i > 0 && shadow_clip_depth > prev_end
          && shadow_clip_depth < prev_end + shadow_map.blend_distance) {
        prev_shadow_factor = calcShadowFactor(i - 1, shadow_light_pos[i - 1], In.normal);

        prev_cascade_color = vec3(0, 0, 0);
        if (i == 1)
          prev_cascade_color = vec3(1, 0, 0);
        else if (i == 2)
          prev_cascade_color = vec3(0, 1, 0);
      }

      if (shadow_clip_depth <= end) {
        float sf = calcShadowFactor(i, shadow_light_pos[i], In.normal);

        float f = i == 0 ? 0
                         : 1
                               - clamp(inverseLerp(prev_end, prev_end + shadow_map.blend_distance,
                                                   shadow_clip_depth),
                                       0, 1);

        shadow_factor = mix(sf, prev_shadow_factor, f);

        if (shadow_map.debug_show_blend) {
          cascade_indicator = mix(indicator_color, prev_cascade_color, f);
        } else {
          cascade_indicator = indicator_color;
        }
      }

      if (shadow_clip_depth <= end) break;
    }
  }

  float[4] draw_strengths = terrainBlending(In.world_pos, In.normal);

  vec3[4] tex_coords;
  tex_coords[0] = getTextureCoordinate(In.world_pos, 0);
  tex_coords[1] = getTextureCoordinate(In.world_pos, 1);
  tex_coords[2] = getTextureCoordinate(In.world_pos, 2);
  tex_coords[3] = getTextureCoordinate(In.world_pos, 3);

  vec3 terrain_color = vec3(0);
  vec3 terrain_normal = vec3(0);
  float terrain_roughness = 0;
  float ao = 0;

#if 1
  float w[4] = getTextureWeightsByDisplacement(tex_coords, draw_strengths);
#else
  float w[4] = draw_strengths;
#endif

  for (int i = 0; i < w.length; i++) {
    terrain_color += texture(albedos, tex_coords[i]).rgb * w[i];
    terrain_normal += getWorldNormal(tex_coords[i]) * w[i];
    terrain_roughness += texture(roughness, tex_coords[i]).r * w[i];
    ao += texture(ambient_occlusion, tex_coords[i]).r * w[i];
  }
  terrain_normal = normalize(terrain_normal);

  Material m;
  m.albedo = terrain_color;
  m.metallic = 0.2;
  m.roughness = terrain_roughness;
  m.ao = ao;
  m.reflective = 0.0;
  m.fresnel = PBR_DIELECTRIC_F0;

  Light light;
  light.color = light.color
      = mix(sun.color, vec3(1), pow(max(dot(-sun.direction, vec3(0, 1, 0)), 0.0), 1.5));
  light.intensity = sun.intensity;
  light.attenuation = vec3(0.35, 0, 0);

  vec3 wo = -normalize(In.view_pos);
  vec3 n = (viewMatrix * vec4(terrain_normal, 0.0)).xyz;
  vec3 wi = (viewMatrix * vec4(-sun.direction, 0.0)).xyz;
  out_color = pbrDirectLightning(In.view_pos, n, wo, wi, viewInverse, m, light,
                                 environment_multiplier, irradiance_map, reflection_map);
  out_color *= shadow_factor;

  // Debug normals
#if 0
  fragmentColor = vec4(terrain_normal, 1.0);
  return;
#endif

  // Debug draw_strengths
#if 0
  float tot = draw_strengths[0] + draw_strengths[1] + draw_strengths[2] + draw_strengths[3];
  if (tot > 1.0001) {
      out_color = vec3(1.0, 0.0, 0.0);
  }
  else if (tot < 0.9999) {
      out_color = vec3(0.0, 0.0, 1.0);
  }
  fragmentColor = vec4(out_color, 1.0);
  return;
#endif

  // Parallax mapping playground
#if 0
  vec3 world_pos = In.world_pos;
  int texture_index = 2;

  vec3 world_view_dir = normalize(world_pos - eyeWorldPos);

  vec2 tex_coord = In.tex_coord * texture_sizes[texture_index];

  mat3 TBN = transpose(mat3(vec3(1, 0, 0), vec3(0, 0, -1), vec3(0, 1, 0)));
  vec3 tangent_view_pos = TBN * eyeWorldPos;
  vec3 tangent_frag_pos = TBN * world_pos;

  vec3 tangent_view_dir = normalize(tangent_view_pos - tangent_frag_pos);

  float height = 1.0 - texture(displacement, vec3(tex_coord, texture_index)).r;
  vec3 tc = vec3(tex_coord - world_view_dir.xy * height * 0.09, texture_index);
  vec3 parallaxed_albedo = texture(albedos, tc).rgb;

  fragmentColor = vec4(tangent_view_dir.xy, 0.0, 1.0);
  fragmentColor = vec4(parallaxed_albedo, 1.0);

  return;
#endif

  fragmentColor = vec4(out_color, 1.0);
}
