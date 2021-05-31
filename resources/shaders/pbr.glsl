#ifndef _PBR_H_
#define _PBR_H_

#include "utils.glsl"

const vec3 PBR_DIELECTRIC_F0 = vec3(0.04);

struct Light {
  vec3 pos;
  vec3 color;
  float intensity;
  // 1 / (x + yd + zd^2) where d is the distance and x,y,z the components of attenuation
  vec3 attenuation;
};

struct Material {
  vec3 albedo;
  vec3 fresnel;
  float metallic;
  float roughness;
  float ao;
  float reflective;
};

// f0 is the surface reflection at zero incidence or how much the surface
// reflects if looking directly at the surface.
vec3 fresnelSchlick(float cos_theta, vec3 f0) {
  return f0 + (1 - f0) * pow(max(1.0 - cos_theta, 0.0), 5.0);
}

vec3 fresnelSchlickRoughness(float cos_theta, vec3 f0, float roughness) {
  return f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(max(1.0 - cos_theta, 0.0), 5.0);
}

float distributionGGX(vec3 n, vec3 wh, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float n_dot_h = max(dot(n, wh), 0.0);
  float n_dot_h2 = n_dot_h * n_dot_h;

  float num = a2;
  float denom = (n_dot_h2 * (a2 - 1.0) + 1.0);
  denom = PI * denom * denom;

  return num / denom;
}

float geometrySchlickGGX(float n_dot_v, float roughness) {
  float r = (roughness + 1.0);
  float k = (r * r) / 8.0;

  float num = n_dot_v;
  float denom = n_dot_v * (1.0 - k) + k;

  return num / denom;
}

float geometrySmith(vec3 n, vec3 wo, vec3 wi, float roughness) {
  float n_dot_wo = max(dot(n, wo), 0.0);
  float n_dot_wi = max(dot(n, wi), 0.0);
  float ggx2 = geometrySchlickGGX(n_dot_wo, roughness);
  float ggx1 = geometrySchlickGGX(n_dot_wi, roughness);

  return ggx1 * ggx2;
}

// All parameters are in view space
// p is the position of the fragment
// n is the surface normal
// wo is the direction from the fragment to the camera
// wi is the direction form the fragment to the light
vec3 pbrDirectLightning(vec3 p, vec3 n, vec3 wo, vec3 wi, mat4 view_inverse, Material m,
                        Light light, float environment_multiplier, sampler2D irradiance_map,
                        sampler2D reflection_map) {
  float cos_theta = max(dot(n, wi), 0.0);
  vec3 f0 = mix(m.fresnel, m.albedo, m.metallic);

  vec3 out_radiance = vec3(0);

  // Per light
  {
    float ld = length(light.pos - p);
    float attenuation
        = 1.0 / (light.attenuation.x + light.attenuation.y * ld + light.attenuation.z * ld * ld);
    vec3 radiance = light.intensity * light.color * attenuation;
    vec3 wh = normalize(wi + wo);

    vec3 fresnel = fresnelSchlick(max(dot(wh, wo), 0.0), f0);

    // [0, 1l, larger the more micro-facets aligned to wh (normal distribution function)
    float NDF = distributionGGX(n, wh, m.roughness);

    // [0, 1], smaller the more micro-facets shadowed by other micro-facets
    float G = geometrySmith(n, wo, wi, m.roughness);

    vec3 numerator = NDF * G * fresnel;
    float denominator = 4.0 * max(dot(n, wo), 0.0) * max(dot(n, wi), 0.0);
    vec3 specular = numerator / max(denominator, 0.0001);

    // for energy conservation the diffuse and specular light can't be above
    // 1.0 (unless the surface emits light); to preserve this relationship
    // the diffuse component (kD) should equal 1.0 - kS.
    vec3 kS = fresnel;       // energy of light that gets reflected
    vec3 kD = vec3(1) - kS;  // energy of light that gets refracted

    // only non-metals have diffuse lightning, or a linear blend if partly metal (pure metal have
    // _no_ diffuse lightning)
    kD *= 1.0 - m.metallic;

    float n_dot_l = max(dot(n, wi), 0.0);
    out_radiance += ((kD * m.albedo / PI) + specular) * radiance * n_dot_l;
  }

  vec3 F = fresnelSchlickRoughness(max(dot(n, wo), 0.0), f0, m.roughness);

  vec3 wr = normalize(-reflect(wo, n));

  // ambient lightning
  vec3 kS = F;
  vec3 kD = 1.0 - kS;
  kD *= 1.0 - m.metallic;

  vec3 irradiance;
  {
    vec3 world_n = normalize((view_inverse * vec4(n, 0.0)).xyz);
    vec2 lookup = sphericalCoordinate(world_n);
    irradiance = environment_multiplier * texture(irradiance_map, lookup).rgb;
  }
  vec3 diffuse = irradiance * (1 / PI) * m.albedo;

  const float MAX_REFLECTION_LOD = 7.0;
  vec3 prefiltered_color;
  {
    vec3 world_wr = normalize((view_inverse * vec4(wr, 0.0)).xyz);
    vec2 lookup = sphericalCoordinate(world_wr);

    prefiltered_color = environment_multiplier
                        * textureLod(reflection_map, lookup, m.roughness * MAX_REFLECTION_LOD).rgb;
  }
  // TODO: brdfLUT
  vec3 specular = prefiltered_color * (F * m.reflective);

  vec3 ambient = (kD * diffuse + specular) * m.ao;
  vec3 color = ambient + out_radiance;

  // HDR tonemapping
  // color = color / (color + vec3(1.0));

  // gamma correct
  // color = pow(color, vec3(1.0/2.2));

  return color;
}

#endif // _PBR_H_
