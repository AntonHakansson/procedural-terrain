#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

in DATA {
    vec3 world_pos;
    vec3 view_pos;
    vec4 shadow_coord;
    vec2 tex_coord;
    vec3 normal;
} In;

layout(binding = 0) uniform sampler2D grass;
layout(binding = 1) uniform sampler2D rock;
// layout(binding = 10) uniform sampler2DShadow shadowMapTex;

uniform mat4 normalMatrix;
uniform mat4 viewMatrix;
uniform mat4 modelMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 modelViewProjectionMatrix;
uniform mat4 viewProjectionMatrix;
uniform vec3 eyeWorldPos;
uniform vec3 viewSpaceLightPosition;
uniform bool simple;


// Cascading shadow map
struct Sun {
	vec3 direction;
	vec3 color;
	float intensity;
};
uniform Sun sun;

const int NUM_CASCADES = 3;

in vec4 LightSpacePos[NUM_CASCADES];
in float ClipSpacePosZ;

layout(binding = 10) uniform sampler2DArrayShadow gShadowMap;
uniform float gCascadeEndClipSpace[NUM_CASCADES];


// Output color
layout(location = 0) out vec4 fragmentColor;






// Triplanar mapping of texture as naively projecting the texture on to the xz
// plane will result in texture-stretching at steep angles
// REVIEW: Can get the texture coordinates from the tessellation shader instead?
vec3 triplanarSampling(sampler2D tex, vec3 worldpos, vec3 normal) {
	vec3 scaled_worldpos = worldpos / (2048.0 / 32.0);

	vec3 blend_axis = abs(normal);
	blend_axis /= blend_axis.x + blend_axis.y + blend_axis.z;

	vec3 x_projection = texture(tex, scaled_worldpos.yz).xyz * blend_axis.x;
	vec3 y_projection = texture(tex, scaled_worldpos.xz).xyz * blend_axis.y;
	vec3 z_projection = texture(tex, scaled_worldpos.xy).xyz * blend_axis.z;

	return x_projection + y_projection + z_projection;
}

vec3 terrainColor(vec3 world_pos, vec3 normal) {
	vec3 grass_color = triplanarSampling(grass, world_pos, normal);
	vec3 rock_color = triplanarSampling(rock, world_pos, normal);

	float transition = 0.2;

	float slope = dot(normal, vec3(0, 1, 0));
	float blending = smoothstep(0.7, 0.9, slope);

	return mix(rock_color, grass_color, blending);
}

vec3 ambient() {
    vec3 ambient = sun.color * sun.intensity;
    return ambient;
}

vec3 diffuse(vec3 world_pos, vec3 normal) {
	// Shadow map
	float visibility = 1; //textureProj(shadowMapTex, In.shadow_coord);

	float diffuse_factor = max(0.0, dot(-sun.direction, normal));
	vec3 diffuse = visibility * diffuse_factor * sun.color * sun.intensity;

	return diffuse;
}


float calcShadowFactor(int index, vec4 LightSpacePos)
{
    vec3 ProjCoords = LightSpacePos.xyz / LightSpacePos.w;

    vec2 UVCoords;
    UVCoords.x = 0.5 * ProjCoords.x + 0.5;
    UVCoords.y = 0.5 * ProjCoords.y + 0.5;

    float z = 0.5 * ProjCoords.z + 0.5;



		// Nu
		// LightSpacePos[i] = gLightWVP[i] * vec4(Out.world_pos, 1.0);
    // float depth = texture(gShadowMap[index], UVCoords).x;

		// // Tutorial 
		// vec4 shadowMapCoord = lightMatrix * vec4(viewSpacePosition, 1.f);
		// float depth = texture(shadowMapTex, shadowMapCoord.xy / shadowMapCoord.w).x;
		// float visibility = (depth >= (shadowMapCoord.z / shadowMapCoord.w)) ? 1.0 : 0.0;


		// // Hardware
		// float visibility = textureProj( shadowMapTex, shadowMapCoord);

    // Nu
    // float visibility = textureProj(gShadowMap[index], LightSpacePos);


		float bias = 0.01;

    // x, y, level
		// float depth = texture(gShadowMap, vec3(UVCoords, index)).x;

    // x, y, level, depth
		float depth = texture(gShadowMap, vec4(UVCoords, index, (z - bias)));

// float computeOcclusion(vec4 shadowCoords)
// {
//     vec3 coord = vec3(shadowCoords.xyz/shadowCoords.w);
//     float depth = texture( shadowMap, vec3(coord.xy,coord.z+0.001));
//     return depth;
// }

		// return visibility;
		return (depth + bias) < z ? 0.5 : 1.0;

		// return depth;


		// float dot_light_normal = dot(sun.direction, In.normal);

		// float bias = 0.0001; //max(0.05 * (1.0 - dot_light_normal), 0.005);

		// return (depth + bias) < z ? 0 : 1.0;

    // if (Depth < z - 0.001)
    //     return 0.5;
    // else
    //     return 1.0;
}


void main() {
	if (simple) return;

	float shadow_factor = 0.0;
	vec4 cascade_indicator = vec4(0.0, 0.0, 1, 0.0);

	for (int i = 0; i < NUM_CASCADES; i++) {
		if (-ClipSpacePosZ >= gCascadeEndClipSpace[i]) {
			shadow_factor = calcShadowFactor(i, LightSpacePos[i]);

			if (i == 0) 
					cascade_indicator = vec4(1, 0.0, 0.0, 0.0);
			else if (i == 1)
					cascade_indicator = vec4(0.0, 1, 0.0, 0.0);
			else if (i == 2)
					cascade_indicator = vec4(0.0, 0.0, 1, 0.0);

			break;
		}
	}



	vec3 terrain_color = terrainColor(In.world_pos, In.normal);
	vec3 ambient = ambient();
	vec3 diffuse = diffuse(In.world_pos, In.normal);

	fragmentColor = vec4(terrain_color * shadow_factor * (ambient + diffuse), 1.0);

	// float visibility = textureProj(shadowMapTex, In.shadow_coord);
	// fragmentColor = vec4(normalize(vec3(visibility)), 1);

	// fog
	if (false) {
		float dist = distance(In.world_pos, eyeWorldPos);

		float density = 0.0005;
		float fog_factor = exp(-pow(density * dist, 2.0));
		fog_factor = 1.0 - clamp(fog_factor, 0.0, 1.0);

		vec3 fog_color = vec3(109.0 / 255.0, 116.0 / 255.0, 109.0 / 255.0);
		fragmentColor = vec4(mix(fragmentColor.xyz, fog_color, fog_factor), 1.0f);
	}
}
