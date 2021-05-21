#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

// in vec2 TexCoord_FS_in;
in vec3 Normal_FS_in;
in vec3 WorldPos_FS_in;

layout(binding = 0) uniform sampler2D grass;
layout(binding = 1) uniform sampler2D rock;

uniform mat4 normalMatrix;
uniform mat4 modelMatrix;
uniform mat4 modelViewMatrix;
uniform mat4 modelViewProjectionMatrix;
uniform mat4 viewProjectionMatrix;
uniform vec3 eyeWorldPos;

struct Sun {
	vec3 direction;
	vec3 color;
	float intensity;
};
uniform Sun sun;

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
	float diffuse_factor = max(0.0, dot(-sun.direction, normal));
	vec3 diffuse = diffuse_factor * sun.color * sun.intensity;
	return diffuse;
}

void main()
{
	vec3 normal = Normal_FS_in;
	vec3 terrain_color = terrainColor(WorldPos_FS_in, normal);
	vec3 ambient = ambient();
	vec3 diffuse = diffuse(WorldPos_FS_in, normal);
	fragmentColor = vec4(terrain_color * (ambient + diffuse), 1.0);

	// fog
	if (false) {
		float dist = distance(WorldPos_FS_in, eyeWorldPos);

		float density = 0.0005;
		float fog_factor = exp(-pow(density * dist, 2.0));
		fog_factor = 1.0 - clamp(fog_factor, 0.0, 1.0);

		vec3 fog_color = vec3(109.0 / 255.0, 116.0 / 255.0, 109.0 / 255.0);
		fragmentColor = vec4(mix(fragmentColor.xyz, fog_color, fog_factor), 1.0f);
	}
}
