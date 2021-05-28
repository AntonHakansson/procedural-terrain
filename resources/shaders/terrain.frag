#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

in DATA {
  vec3 world_pos;
  vec3 view_pos;
  vec4 shadow_coord;
  vec2 tex_coord;
  vec3 normal;
  vec3 tangent;
  vec3 bitangent;
  mat3 tangent_matrix;
}
In;

layout(binding = 0) uniform sampler2DArray albedos;
layout(binding = 1) uniform sampler2DArray normals;
layout(binding = 2) uniform sampler2DArray displacement;
layout(binding = 3) uniform sampler2DArray roughness;

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

vec3 blendTextures(vec3 t1, vec3 t2, float a, float b) {
  vec3 col1 = texture(albedos, t1).rgb;
  vec3 col2 = texture(albedos, t2).rgb;

  float disp1 = texture(displacement, t1).r;
  float disp2 = texture(displacement, t2).r;

  float depth = 0.1;
  float ma = max(disp1 + a, disp2 + b) - depth;

  float w1 = max(disp1 + a - ma, 0);
  float w2 = max(disp2 + b - ma, 0);

  return (col1 * w1 + col2 * w2) / (w1 + w2);
}

vec3 getWorldNormal(vec3 tex_coord) {
  vec3 tangent_normal = normalize(2.0 * texture(normals, tex_coord).xyz - 1.0);
  return In.tangent_matrix * tangent_normal;
}

void blendTextures(vec3 t1, vec3 t2, vec3 t3, vec3 t4, float[4] w, out vec3 color,
                   out vec3 normal) {
  vec3 col1 = texture(albedos, t1).rgb;
  vec3 col2 = texture(albedos, t2).rgb;
  vec3 col3 = texture(albedos, t3).rgb;
  vec3 col4 = texture(albedos, t4).rgb;

  vec3 normal1 = getWorldNormal(t1);
  vec3 normal2 = getWorldNormal(t2);
  vec3 normal3 = getWorldNormal(t3);
  vec3 normal4 = getWorldNormal(t4);

  float disp1 = texture(displacement, t1).r * texture_displacement_weights[0];
  float disp2 = texture(displacement, t2).r * texture_displacement_weights[1];
  float disp3 = texture(displacement, t3).r * texture_displacement_weights[2];
  float disp4 = texture(displacement, t4).r * texture_displacement_weights[3];

  float depth = 0.1;
  float ma = max(max(max(disp1 + w[0], disp2 + w[1]), disp3 + w[2]), disp4 + w[3]) - depth;

  float w1 = max(disp1 + w[0] - ma, 0);
  float w2 = max(disp2 + w[1] - ma, 0);
  float w3 = max(disp3 + w[2] - ma, 0);
  float w4 = max(disp4 + w[3] - ma, 0);

  float total_w = (w1 + w2 + w3 + w4);

  color = (col1 * w1 + col2 * w2 + col3 * w3 + col4 * w4) / total_w;
  normal = normalize((normal1 * w1 + normal2 * w2 + normal3 * w3 + normal4 * w4) / total_w);
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
  vec2 scaled_worldpos = (world_pos / (2048.0)).xz;
  vec2 tex_coord = scaled_worldpos * texture_sizes[texture_index];

  mat4 inv_view_matrix = viewInverse;
  vec3 view_direction
      = normalize(-vec3(inv_view_matrix[2][0], inv_view_matrix[2][1], inv_view_matrix[2][2]));

  return vec3(tex_coord, texture_index);
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
  if (shadow_map.debug_show_splits) {
    fragmentColor = vec4(1, 0, 0, 1);
    return;
  }

  float shadow_factor = 0.0;
  vec3 cascade_indicator = vec3(0.0, 0.0, 0);
  vec3 prev_color = cascade_indicator;

  float blend_distance = 50;
  float prev_shadow_factor = 0;

  for (int i = 0; i < NUM_CASCADES; i++) {
    float end = shadow_map.cascade_clip_splits[i];
    float prev_end = i == 0 ? 0 : shadow_map.cascade_clip_splits[i - 1];

    vec3 indicator_color = vec3(1, 0, 0);
    if (i == 1)
      indicator_color = vec3(0, 1, 0);
    else if (i == 2)
      indicator_color = vec3(0, 0, 1);

    if (i > 0 && shadow_clip_depth > prev_end && shadow_clip_depth < prev_end + blend_distance) {
      prev_shadow_factor = calcShadowFactor(i - 1, shadow_light_pos[i - 1], In.normal);

      prev_color = vec3(0, 0, 0);
      if (i == 1)
        prev_color = vec3(1, 0, 0);
      else if (i == 2)
        prev_color = vec3(0, 1, 0);
    }

    if (shadow_clip_depth <= end) {
      float sf = calcShadowFactor(i, shadow_light_pos[i], In.normal);

      float f0
          = i == 0 ? 0
                   : 1
                         - clamp(inverseLerp(prev_end, prev_end + blend_distance, shadow_clip_depth),
                                 0, 1);
      float f1 = clamp(inverseLerp(end - blend_distance, end, shadow_clip_depth), 0, 1);
      float f = max(f0, f1);

      shadow_factor = mix(sf, prev_shadow_factor, f0);

      cascade_indicator = mix(indicator_color, prev_color, f0);
    }

    if (shadow_clip_depth <= end) break;
  }

  float[4] draw_strengths = terrainBlending(In.world_pos, In.normal);

  vec3 terrain_color;
  vec3 terrain_normal;
  blendTextures(getTextureCoordinate(In.world_pos, 0), getTextureCoordinate(In.world_pos, 1),
                getTextureCoordinate(In.world_pos, 2), getTextureCoordinate(In.world_pos, 3),
                draw_strengths, terrain_color, terrain_normal);

  vec3 ambient = ambient();
  vec3 diffuse = diffuse(In.world_pos, terrain_normal);

  fragmentColor = vec4(terrain_color * shadow_factor * (ambient + diffuse), 1.0);

  // Debug normals
#if 0
  fragmentColor = vec4(terrain_normal, 1);
  return;
#endif

  // Debug for draw_weights
#if 0
  float[4] draw_strengths = terrainBlending(In.world_pos, In.normal);
  float tot = draw_strengths[0] + draw_strengths[1] + draw_strengths[2] + draw_strengths[3];
  if (tot > 1.0001) {
      fragmentColor = vec4(1.0, 0.0, 0.0, 1.0);
      return;
  }
  else if (tot < 0.9999) {
      fragmentColor = vec4(0.0, 0.0, 1.0, 1.0);
      return;
  }
  return;
#endif
}
