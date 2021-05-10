#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

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

layout(binding = 0) uniform sampler2D grass;
layout(binding = 1) uniform sampler2D rock;
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
uniform float point_light_intensity_multiplier = 50.0;

///////////////////////////////////////////////////////////////////////////////
// Constants
///////////////////////////////////////////////////////////////////////////////
#define PI 3.14159265359
#define Epsilon 0.00001

///////////////////////////////////////////////////////////////////////////////
// Input varyings from vertex shader
///////////////////////////////////////////////////////////////////////////////
in vec2 TexCoord_FS_in;
in vec3 Normal_FS_in;
in vec3 WorldPos_FS_in;
// in vec3 viewSpaceNormal;
// in vec3 viewSpacePosition;
vec3 viewSpacePosition;
vec3 viewSpaceNormal;
uniform vec3 eyeWorldPos;
uniform mat4 viewProjectionMatrix;

uniform float amplitude;
uniform float frequency;

///////////////////////////////////////////////////////////////////////////////
// Input uniform variables
///////////////////////////////////////////////////////////////////////////////
uniform mat4 viewInverse;
uniform vec3 viewSpaceLightPosition;

///////////////////////////////////////////////////////////////////////////////
// Output color
///////////////////////////////////////////////////////////////////////////////
layout(location = 0) out vec4 fragmentColor;

vec3 calculateDirectIllumiunation(vec3 wo, vec3 n)
{
	vec3 direct_illum = material_color;
	vec3 wi = normalize(viewSpaceLightPosition - viewSpacePosition);
	float dist_to_light = length(viewSpaceLightPosition - viewSpacePosition);
	float attenuation = 1.0 / (dist_to_light * dist_to_light);
	vec3 L_i = point_light_intensity_multiplier * point_light_color * attenuation;

	float cos_theta = max(dot(n, wi), 0.0);
	if (cos_theta <= Epsilon) { return vec3(0.0); }
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

		fresnel_factor = R0 + ((1.0 - R0) * pow(1.0 - cos_theta_h, 5));
	}

	// Calculate normal distribution for specular BRDF.
	// - distribution of microfacets with a particular normal
	float microfacet_distribution_factor = 0.0;
	{
		microfacet_distribution_factor = ((material_shininess + 2.0) / (2.0 * PI)) * pow(n_dot_wh, max(material_shininess, Epsilon));
	}

	// Calculate geometric attenuation for specular BRDF.
	// - modeling of microfacets blocking the view or light direction close to grazing angles
	float shadowing_factor = 0.0;
	{
		float denom = 1.0 / max(dot(wo, wh), Epsilon);
		shadowing_factor = min(1.0, 2.0 * min(n_dot_wh * n_dot_wo * denom, n_dot_wh * n_dot_wi * denom));
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
	vec3 microfacet_term = material_metalness * metal_term + (1 - material_metalness) * dielectric_term;

	return material_reflectivity * microfacet_term + (1 - material_reflectivity) * diffuse_term;
}

vec3 calculateIndirectIllumination(vec3 wo, vec3 n)
{
	vec3 indirect_illum = vec3(0.f);

	vec3 diffuse_term = vec3(0.0);
	{
		// calc world-space normal
		vec3 n_ws = normalize((viewInverse * vec4(n, 0.0)).xyz);

		// Calculate the spherical coordinates of the direction
		float theta = acos(max(-1.0f, min(1.0, n_ws.y)));
		float phi = atan(n_ws.z, n_ws.x);
		if(phi < 0.0)
		{
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
			if(phi < 0.0)
			{
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

		float roughness = sqrt(sqrt(2.0 / (material_shininess + 2.0))); // sample from the preconvolved env map
		vec3 Li = environment_multiplier * textureLod(reflectionMap, lookup, roughness * 7.0).xyz;

		dielectric_term = fresnel_factor * Li + (1 - fresnel_factor) * diffuse_term;
		metal_term = fresnel_factor * material_color * Li;
	}

	vec3 microfacet_term = material_metalness * metal_term + (1.0 - material_metalness) * dielectric_term;
	indirect_illum =  material_reflectivity * microfacet_term + (1.0 - material_reflectivity) * diffuse_term;

	return indirect_illum;
}


vec3 mod289(vec3 x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec2 mod289(vec2 x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec3 permute(vec3 x) {
  return mod289(((x*34.0)+1.0)*x);
}

float snoise(vec2 v)
  {
  const vec4 C = vec4(0.211324865405187,  // (3.0-sqrt(3.0))/6.0
                      0.366025403784439,  // 0.5*(sqrt(3.0)-1.0)
                     -0.577350269189626,  // -1.0 + 2.0 * C.x
                      0.024390243902439); // 1.0 / 41.0
// First corner
  vec2 i  = floor(v + dot(v, C.yy) );
  vec2 x0 = v -   i + dot(i, C.xx);

// Other corners
  vec2 i1;
  //i1.x = step( x0.y, x0.x ); // x0.x > x0.y ? 1.0 : 0.0
  //i1.y = 1.0 - i1.x;
  i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
  // x0 = x0 - 0.0 + 0.0 * C.xx ;
  // x1 = x0 - i1 + 1.0 * C.xx ;
  // x2 = x0 - 1.0 + 2.0 * C.xx ;
  vec4 x12 = x0.xyxy + C.xxzz;
  x12.xy -= i1;

// Permutations
  i = mod289(i); // Avoid truncation effects in permutation
  vec3 p = permute( permute( i.y + vec3(0.0, i1.y, 1.0 ))
    + i.x + vec3(0.0, i1.x, 1.0 ));

  vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
  m = m*m ;
  m = m*m ;

// Gradients: 41 points uniformly over a line, mapped onto a diamond.
// The ring size 17*17 = 289 is close to a multiple of 41 (41*7 = 287)

  vec3 x = 2.0 * fract(p * C.www) - 1.0;
  vec3 h = abs(x) - 0.5;
  vec3 ox = floor(x + 0.5);
  vec3 a0 = x - ox;

// Normalise gradients implicitly by scaling m
// Approximation of: m *= inversesqrt( a0*a0 + h*h );
  m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );

// Compute final noise value at P
  vec3 g;
  g.x  = a0.x  * x0.x  + h.x  * x0.y;
  g.yz = a0.yz * x12.xz + h.yz * x12.yw;
  return 130.0 * dot(m, g);
}

vec3 computeNormals(vec3 WorldPos, out mat3 TBN){
	float st = 1.0;
	float dhdu = (snoise(vec2((WorldPos.x + st), WorldPos.z)) - snoise(vec2((WorldPos.x - st), WorldPos.z)))/(2.0*st) * amplitude;
	float dhdv = (snoise(vec2(WorldPos.x, (WorldPos.z + st))) - snoise(vec2(WorldPos.x, (WorldPos.z - st))))/(2.0*st) * amplitude;

	vec3 X = vec3(1.0, dhdu, 1.0);
	vec3 Z = vec3(0.0, dhdv, 1.0);

	vec3 n = normalize(cross(Z,X));
	TBN = mat3(normalize(X), normalize(Z), n);

	return n;
}


void main()
{
	viewSpacePosition = WorldPos_FS_in;

	mat3 TBN;
	viewSpaceNormal = computeNormals(viewSpacePosition * frequency, TBN);

	vec3 wo = -normalize(viewSpacePosition);
	vec3 n = normalize(viewSpaceNormal);

	// Direct illumination
	vec3 direct_illumination_term = calculateDirectIllumiunation(wo, n);

	// Indirect illumination
	vec3 indirect_illumination_term = calculateIndirectIllumination(wo, n);

	vec3 shading = direct_illumination_term + indirect_illumination_term;

	float height = WorldPos_FS_in.y;
	fragmentColor = vec4(vec3(height / amplitude), 1.0);
	// fragmentColor = vec4(vec3(height / amplitude), 1.0);
	// fragmentColor = vec4(Normal_FS_in, 1.0);
	// fragmentColor = vec4(viewSpaceNormal, 1.0);

	float dist = distance(WorldPos_FS_in, eyeWorldPos);

	// fragmentColor *= pow((300 - dist) / 300.0, 10);

	// if (dist > 300.0) {
	// 	fragmentColor *= vec4(vec3(0.1), 1);
	// }
	// else if (dist > 150.0) {
	// 	fragmentColor *= vec4(vec3(0.4), 1);
	// }
}
