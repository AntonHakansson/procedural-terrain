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

layout(binding = 0) uniform sampler2D grass;
layout(binding = 1) uniform sampler2D rock;
layout(binding = 2) uniform sampler2D sand;
layout(binding = 3) uniform sampler2D snow;
layout(binding = 4) uniform sampler2D grass_normal;
layout(binding = 5) uniform sampler2D rock_normal;
layout(binding = 6) uniform sampler2D sand_normal;
layout(binding = 7) uniform sampler2D snow_normal;
// layout(binding = 10) uniform sampler2DShadow shadowMapTex;

uniform mat4 normalMatrix;
uniform mat4 viewMatrix;
uniform mat4 modelMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 modelViewProjectionMatrix;
uniform mat4 viewProjectionMatrix;
uniform vec3 eyeWorldPos;
uniform vec3 viewSpaceLightPosition;

uniform float waterHeight;
uniform bool simple;

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

// Cascading shadow map
struct Sun {
  vec3 direction;
  vec3 color;
  float intensity;
};
uniform Sun sun;

const int NUM_CASCADES = 3;

in vec4 light_space_pos[NUM_CASCADES];
in float clip_space_depth;

layout(binding = 10) uniform sampler2DArrayShadow gShadowMap;
uniform float gCascadeEndClipSpace[NUM_CASCADES];

// Output color
layout(location = 0) out vec4 fragmentColor;

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

vec3[4] getAlbedos(vec3 world_pos, vec3 normal) {
  vec3 grass_color = triplanarSampling(grass, world_pos, normal);
  vec3 rock_color = triplanarSampling(rock, world_pos, normal);
  vec3 sand_color = triplanarSampling(sand, world_pos, normal);
  vec3 snow_color = triplanarSampling(snow, world_pos, normal);

  vec3 cc[4];
  cc[0] = sand_color;
  cc[1] = grass_color;
  cc[2] = rock_color;
  cc[3] = snow_color;

	return cc;
}

vec3[4] getNormals(vec3 world_pos, vec3 normal) {
  vec3 grass_normal = 2 * triplanarSampling(grass_normal, world_pos, normal) - vec3(1);
  vec3 rock_normal = 2 * triplanarSampling(rock_normal, world_pos, normal) - vec3(1);
  vec3 sand_normal = 2 * triplanarSampling(sand_normal, world_pos, normal) - vec3(1);
  vec3 snow_normal = 2 * triplanarSampling(snow_normal, world_pos, normal) - vec3(1);

  vec3 cc[4];
  cc[0] = sand_normal;
  cc[1] = grass_normal;
  cc[2] = rock_normal;
  cc[3] = snow_normal;

	return cc;
}

float[4] terrainBlending(vec3 world_pos, vec3 normal) {
  float height = world_pos.y;

  // A completely flat terrain has slope=0
  float slope = max(1 - dot(normal, vec3(0, 1, 0)), 0.0);
  float blending = smoothstep(0.7, 0.9, slope);

  // For each fragment we compute how much each texture contributes depending on height and slope
  float draw_strengths[4];

  float height_percentage = inverseLerpClamped(-(3 * noise.amplitude), noise.amplitude, height);
  for (int i = 0; i < 4; i++) {
    float height_diff = height_percentage - texture_start_heights[i];

    float lower_bound = -texture_blends[i] / 2 - 1E-4;
    float upper_bound = texture_blends[i] / 2;

    draw_strengths[i] = inverseLerpClamped(lower_bound, upper_bound, height_diff);
  }

  // fix slope blending stuff here
  draw_strengths[3] *= smoothstep(0.0, 0.2, pow(1 - slope, 2));

	return draw_strengths;
}

vec3 terrainNormal(vec3 world_pos, vec3 normal) {
	vec3[] normals = getNormals(world_pos, normal);
  float[] draw_strengths = terrainBlending(world_pos, normal);

  vec3 norm = vec3(0);

  for (int i = 0; i < 4; i++) {
    // float d = draw_strengths[i] / tot_draw_strength;
    float d = draw_strengths[i];
    // c = mix(c, cc[i], draw_strengths[i]);
    norm = norm * (1 - draw_strengths[i]) + (In.tangent_matrix * normals[i]) * d;
  }

  return norm;

  // return In.tangent_matrix * rock_normal;
  // return normalize(
  //     mix(In.tangent_matrix * rock_normal, In.tangent_matrix * grass_normal, blending));
}

vec3 terrainColor(vec3 world_pos, vec3 normal) {
	vec3[] albedos = getAlbedos(world_pos, normal);
	float[] draw_strengths = terrainBlending(world_pos, normal);

  vec3 c = vec3(0);

  for (int i = 0; i < 4; i++) {
    // float d = draw_strengths[i] / tot_draw_strength;
    float d = draw_strengths[i];
    // c = mix(c, cc[i], draw_strengths[i]);
    c = c * (1 - draw_strengths[i]) + albedos[i] * d;
  }

  return c;
}

vec3 ambient() {
  vec3 ambient = sun.color * sun.intensity;
  return ambient;
}

vec3 diffuse(vec3 world_pos, vec3 normal) {
  vec3 norm = terrainNormal(world_pos, normal);

  float diffuse_factor = max(0.0, dot(-sun.direction, norm));
  vec3 diffuse = diffuse_factor * sun.color * sun.intensity;

  return diffuse;
}

float calcShadowFactor(int index, vec4 light_space_pos) {
  vec3 ProjCoords = light_space_pos.xyz / light_space_pos.w;

  vec2 UVCoords;
  UVCoords.x = 0.5 * ProjCoords.x + 0.5;
  UVCoords.y = 0.5 * ProjCoords.y + 0.5;

  float z = 0.5 * ProjCoords.z + 0.5;

  float bias = 0.0005;

  float percentLit = 0;
  float size = 5;
  int maxBound = int(floor(size / 2));
  int minBound = -int(floor((size - 0.01) / 2));

  for (int k = minBound; k <= maxBound; k++) {
    for (int l = minBound; l <= maxBound; l++) {
      vec2 texel = vec2(1 / 4096.0, 1 / 4096.0);
      vec2 offset = vec2(texel * vec2(l, k));

      // x, y, level, depth
      float visibility = texture(gShadowMap, vec4(UVCoords + offset, index, (z - bias)));

      percentLit += visibility;
    }
  }

  return mix(0.5, 1.0, percentLit / (size * size));

  // // Finish the average of all samples, and gamma correct the shadow factor.
  // return pow(percentLit /= 25.0f, 2.2);

  // return visibility;
}

void main() {
  float shadow_factor = 0.0;
  vec3 cascade_indicator = vec3(0.0, 0.0, 0);
  vec3 prev_color = cascade_indicator;

  float blend_distance = 50;
  float prev_shadow_factor = 0;

  for (int i = 0; i < NUM_CASCADES; i++) {
    float end = gCascadeEndClipSpace[i];
    float prev_end = i == 0 ? 0 : gCascadeEndClipSpace[i - 1];

    vec3 indicator_color = vec3(1, 0, 0);
    if (i == 1)
      indicator_color = vec3(0, 1, 0);
    else if (i == 2)
      indicator_color = vec3(0, 0, 1);

    if (i > 0 && clip_space_depth > prev_end && clip_space_depth < prev_end + blend_distance) {
      prev_shadow_factor = calcShadowFactor(i - 1, light_space_pos[i - 1]);

      prev_color = vec3(0, 0, 0);
      if (i == 1)
        prev_color = vec3(1, 0, 0);
      else if (i == 2)
        prev_color = vec3(0, 1, 0);
    }

    if (clip_space_depth <= end) {
      float sf = calcShadowFactor(i, light_space_pos[i]);

      float f0 = i == 0 ? 0 : 1 - clamp(inverseLerp(prev_end, prev_end + blend_distance, clip_space_depth), 0, 1);
      float f1 = clamp(inverseLerp(end - blend_distance, end, clip_space_depth), 0, 1);
      float f = max(f0, f1);

      shadow_factor = mix(sf, prev_shadow_factor, f0);

      // prev_shadow_factor = sf;

      cascade_indicator = mix(indicator_color, prev_color, f0);
    }

    if (clip_space_depth <= end) break;
  }

  // shadow_factor = clip_space_depth > 1000 ? 1 : 0;

  vec3 terrain_color = terrainColor(In.world_pos, In.normal);
  vec3 ambient = ambient();
  vec3 diffuse = diffuse(In.world_pos, In.normal);

  fragmentColor = vec4(terrain_color * shadow_factor * (ambient + diffuse), 1.0);
// fragmentColor = vec4(cascade_indicator, 1.0);
// fragmentColor = vec4(vec3(shadow_factor), 1.0);

// fog
#if 0
		float dist = distance(In.world_pos, eyeWorldPos);

		float density = 0.0005;
		float fog_factor = exp(-pow(density * dist, 2.0));
		fog_factor = 1.0 - clamp(fog_factor, 0.0, 1.0);

		vec3 fog_color = vec3(109.0 / 255.0, 116.0 / 255.0, 109.0 / 255.0);
		fragmentColor = vec4(mix(fragmentColor.xyz, fog_color, fog_factor), 1.0f);
#endif

  // fragmentColor = vec4(terrain_color, 1.0);
}
