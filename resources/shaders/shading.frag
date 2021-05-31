#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

#include "pbr.glsl"

///////////////////////////////////////////////////////////////////////////////
// Material
///////////////////////////////////////////////////////////////////////////////
uniform vec3 material_color;
uniform float material_reflectivity;
uniform float material_metalness;
uniform float material_fresnel;
uniform float material_shininess;
uniform float material_emission;

uniform int has_emission_texture;
uniform int has_color_texture;
layout(binding = 0) uniform sampler2D colorMap;
layout(binding = 5) uniform sampler2D emissiveMap;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
layout(binding = 6) uniform sampler2D environmentMap;
layout(binding = 7) uniform sampler2D irradianceMap;
layout(binding = 8) uniform sampler2D reflectionMap;
uniform float environment_multiplier;

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
uniform vec3 point_light_color = vec3(1.0, 1.0, 1.0);
uniform float point_light_intensity_multiplier = 30.0;

///////////////////////////////////////////////////////////////////////////////
// Constants
///////////////////////////////////////////////////////////////////////////////
#define PI 3.14159265359
#define Epsilon 0.00001

///////////////////////////////////////////////////////////////////////////////
// Input varyings from vertex shader
///////////////////////////////////////////////////////////////////////////////
in vec2 texCoord;
in vec3 viewSpaceNormal;
in vec3 viewSpacePosition;

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 viewInverse;
uniform vec3 viewSpaceLightPosition;

///////////////////////////////////////////////////////////////////////////////
// Output color
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 fragmentColor;

vec3 calculateDirectIllumiunation(vec3 wo, vec3 n, vec3 wi) {
  vec3 direct_illum = material_color;

  // float dist_to_light = length(viewSpaceLightPosition - viewSpacePosition);
  // float attenuation = 1.0 / (dist_to_light * dist_to_light);
  float attenuation = 1;

  vec3 L_i = point_light_intensity_multiplier * point_light_color * attenuation;

  float cos_theta = max(dot(n, wi), 0.0);
  if (cos_theta <= Epsilon) {
    return vec3(0.0);
  }
  vec3 diffuse_term = material_color * (1.0 / PI) * cos_theta * L_i;

  vec3 wh = normalize(wi + wo);
  float n_dot_wh = max(dot(n, wh), 0.0);
  float n_dot_wo = max(dot(n, wo), 0.0);
  float n_dot_wi = max(dot(n, wi), 0.0);

  // Calculate Fresnel term for direct lighting.
  // - amount of incoming light that is going to be reflected instead of refracted
  //   i.e. amount of reclection when looking straight at a surface
  float fresnel_factor = 0.0;
  {
    float R0 = material_fresnel;
    float cos_theta_h = max(dot(wh, wi), 0.0);

    fresnel_factor = R0 + ((1.0 - R0) * max(pow(1.0 - cos_theta_h, 5), 0));
  }

  // Calculate normal distribution for specular BRDF.
  // - distribution of microfacets with a particular normal
  float microfacet_distribution_factor = 0.0;
  {
    microfacet_distribution_factor = ((material_shininess + 2.0) / (2.0 * PI))
                                     * pow(n_dot_wh, max(material_shininess, Epsilon));
  }

  // Calculate geometric attenuation for specular BRDF.
  // - modeling of microfacets blocking the view or light direction close to grazing angles
  float shadowing_factor = 0.0;
  {
    float denom = 1.0 / max(dot(wo, wh), Epsilon);
    shadowing_factor
        = min(1.0, 2.0 * min(n_dot_wh * n_dot_wo * denom, n_dot_wh * n_dot_wi * denom));
  }

  // Cook-Torrance specular microfacet BRDF.
  float brdf = 0.0;
  {
    float denom = 1.0 / (4.0 * max(n_dot_wo * n_dot_wi, Epsilon));
    brdf = (fresnel_factor * microfacet_distribution_factor * shadowing_factor) * denom;
  }

  vec3 l = dot(n, wi) * L_i;
  vec3 dielectric_term = brdf * l + (1 - fresnel_factor) * diffuse_term;
  vec3 metal_term = brdf * material_color * l;
  vec3 microfacet_term
      = material_metalness * metal_term + (1 - material_metalness) * dielectric_term;

  return material_reflectivity * microfacet_term + (1 - material_reflectivity) * diffuse_term;
}

vec3 calculateIndirectIllumination(vec3 wo, vec3 n) {
  vec3 indirect_illum = vec3(0.f);

  vec3 diffuse_term = vec3(0.0);
  {
    // calc world-space normal
    vec3 n_ws = normalize((viewInverse * vec4(n, 0.0)).xyz);

    // Calculate the spherical coordinates of the direction
    float theta = acos(max(-1.0f, min(1.0, n_ws.y)));
    float phi = atan(n_ws.z, n_ws.x);
    if (phi < 0.0) {
      phi = phi + 2.0 * PI;
    }

    // Use these to lookup the color in the environment map
    vec2 lookup = vec2(phi / (2.0 * PI), theta / PI);
    vec3 irradiance = environment_multiplier * texture(irradianceMap, lookup).xyz;
    diffuse_term = material_color * (1.0 / PI) * irradiance;
  }

  ///////////////////////////////////////////////////////////////////////////
  vec3 dielectric_term = vec3(0.0);
  vec3 metal_term = vec3(0.0);
  {
    vec3 wi = normalize(-reflect(wo, n));
    vec3 wh = normalize(wi + wo);

    // Calculate the spherical coordinates of the direction
    vec2 lookup = vec2(0.0);
    {
      // calc world-space wi
      vec3 wi_ws = normalize((viewInverse * vec4(wi, 0.0)).xyz);

      float theta = acos(max(-1.0f, min(1.0, wi_ws.y)));
      float phi = atan(wi_ws.z, wi_ws.x);
      if (phi < 0.0) {
        phi = phi + 2.0 * PI;
      }
      lookup = vec2(phi / (2.0 * PI), theta / PI);
    }

    // Calculate Fresnel term for indirect lighting.
    float fresnel_factor = 0.0;
    {
      float R0 = material_fresnel;
      float cos_theta_h = max(dot(wh, wi), 0.0);

      fresnel_factor = R0 + ((1.0 - R0) * pow(1.0 - cos_theta_h, 5));
    }

    float roughness
        = sqrt(sqrt(2.0 / (material_shininess + 2.0)));  // sample from the preconvolved env map
    vec3 Li = environment_multiplier * textureLod(reflectionMap, lookup, roughness * 7.0).xyz;

    dielectric_term = fresnel_factor * Li + (1 - fresnel_factor) * diffuse_term;
    metal_term = fresnel_factor * material_color * Li;
  }

  vec3 microfacet_term
      = material_metalness * metal_term + (1.0 - material_metalness) * dielectric_term;
  indirect_illum
      = material_reflectivity * microfacet_term + (1.0 - material_reflectivity) * diffuse_term;

  return indirect_illum;
}

void main() {
  float visibility = 1.0;
  float attenuation = 1.0;

  vec3 wo = -normalize(viewSpacePosition);
  vec3 wi = normalize(viewSpaceLightPosition - viewSpacePosition);
  vec3 n = normalize(viewSpaceNormal);

#if 1
  Material m;
  m.albedo = material_color;
  m.metallic = material_metalness;
  m.roughness = sqrt(sqrt(2.0 / (material_shininess + 2.0)));
  m.reflective = material_reflectivity;
  m.fresnel = vec3(material_fresnel);
  m.ao = 1.0;

  Light light;
  light.pos = viewSpaceLightPosition;
  light.color = point_light_color;
  light.intensity = point_light_intensity_multiplier * (1);
  light.attenuation = vec3(0, 0, 1);

  vec3 direct_illumination_term
      = pbrDirectLightning(viewSpacePosition, n, wo, wi, viewInverse, m, light, false,
                           environment_multiplier, irradianceMap, reflectionMap);
  // vec3 indirect_illumination_term = pbrIndirectLightning(n, wo, m, viewInverse,
  // environment_multiplier, irradianceMap, reflectionMap);
  vec3 indirect_illumination_term = vec3(0);
#else
  vec3 direct_illumination_term = visibility * calculateDirectIllumiunation(wo, n, wi);
  vec3 indirect_illumination_term = calculateIndirectIllumination(wo, n);
#endif

  ///////////////////////////////////////////////////////////////////////////
  // Add emissive term. If emissive texture exists, sample this term.
  ///////////////////////////////////////////////////////////////////////////
  vec3 emission_term = material_emission * material_color;
  if (has_emission_texture == 1) {
    emission_term = texture(emissiveMap, texCoord).xyz;
  }

  vec3 shading = direct_illumination_term + indirect_illumination_term + emission_term;
  fragmentColor = vec4(shading * visibility, 1.0);
}
