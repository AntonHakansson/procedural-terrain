#version 420

// required by GLSL spec Sect 4.5.3 (though nvidia does not, amd does)
precision highp float;

// Implementation based on this paper: http://jcgt.org/published/0003/04/04/paper.pdf
// blogpost about paper: https://casual-effects.blogspot.com/2014/08/screen-space-ray-tracing.html
//
// Also relevant: http://roar11.com/2015/07/screen-space-glossy-reflections/

in DATA {
    vec2 tex_coord;
} In;

uniform float current_time;

struct Water {
    float height;
    float foam_distance;
};
uniform Water water;

struct ScreenSpaceReflection {
    ivec2 depth_buffer_size;
    float z_near;
    float z_far;

    // Thickness to assign each pixel in the depth buffer
    float z_thickness;
    // The camera-space distance to step in each iteration
    float stride;
    // Number between 0 and 1 for how far to bump the ray in stride units to conceal banding artifacts
    float jitter;
    // Max number of trace iterations
    float max_steps;
    // Maximum camera-space distance to trace before returning a miss
    float max_distance;
};
uniform ScreenSpaceReflection ssr;

// TODO: pass inverse for performance
uniform mat4 view_matrix;
uniform mat4 projection_matrix;

uniform vec3 camera_position;

layout(binding = 0) uniform sampler2D pixel_buffer;
layout(binding = 1) uniform sampler2D depth_buffer;

layout(location = 0) out vec4 fragmentColor;

#define point2 vec2
#define point3 vec3

void swap(inout float a, inout float b)
{
    float t = a;
    a = b;
    b = t;
}

float distanceSquared(vec2 a, vec2 b)
{
    a -= b;
    return dot(a, a);
}
float distanceSquared(vec3 a, vec3 b)
{
    a -= b;
    return dot(a, a);
}

float linearizeDepth(float hyperbolic_depth)
{
    float z_n = 2.0 * hyperbolic_depth - 1.0;
    float z_e = 2.0 * ssr.z_near * ssr.z_far / (ssr.z_far + ssr.z_near - z_n * (ssr.z_far - ssr.z_near));
    return z_e;
}

// bool traceScreenSpaceRay(
//     point3 csOrig, vec3 csDir, mat4x4 proj,
//     sampler2D csZBuffer, vec2 csZBufferSize, float zThickness,
//     const bool csZBufferIsHyperbolic, vec3 clipInfo, float nearPlaneZ,
//     float stride, float jitter, const float maxSteps, float maxDistance,
//     out point2 hitPixel, out int hitLayer, out point3 csHitPoint) {

//  // Clip to the near plane
//  float rayLength = ((csOrig.z + csDir.z * maxDistance) > nearPlaneZ) ? (nearPlaneZ - csOrig.z) / csDir.z : maxDistance;
//  point3 csEndPoint = csOrig + csDir * rayLength;
//  hitPixel = point2(-1, -1);

//  // Project into screen space
//  vec4 H0 = proj * vec4(csOrig, 1.0);
//  vec4 H1 = proj * vec4(csEndPoint, 1.0);

//  float k0 = 1.0 / H0.w, k1 = 1.0 / H1.w;
//  point3 Q0 = csOrig * k0, Q1 = csEndPoint * k1;

//  // Screen-space endpoints
//  point2 P0 = H0.xy * k0, P1 = H1.xy * k1;

//  // [ Optionally clip here using listing 4 ]

//  P1 += vec2((distanceSquared(P0, P1) < 0.0001) ? 0.01 : 0.0);
//  vec2 delta = P1 - P0;

//  bool permute = false;
//  if (abs(delta.x) < abs(delta.y)) {
//     permute = true;
//     delta = delta.yx; P0 = P0.yx; P1 = P1.yx;
//  }

//  float stepDir = sign(delta.x), invdx = stepDir / delta.x;

//  // Track the derivatives of Q and k.
//  vec3 dQ = (Q1 - Q0) * invdx;
//  float dk = (k1 - k0) * invdx;
//  vec2 dP = vec2(stepDir, delta.y * invdx);

//  dP *= stride; dQ *= stride; dk *= stride;
//  P0 += dP * jitter; Q0 += dQ * jitter; k0 += dk * jitter;
//  float prevZMaxEstimate = csOrig.z;
//     // Slide P from P0 to P1, (now-homogeneous) Q from Q0 to Q1, k from k0 to k1
//     point3 Q = Q0; float k = k0, stepCount = 0.0, end = P1.x * stepDir;
//     for (point2 P = P0;
//         ((P.x * stepDir) <= end) && (stepCount < maxSteps);
//         P += dP, Q.z += dQ.z, k += dk, stepCount += 1.0) {

//         // Project back from homogeneous to camera space
//         hitPixel = permute ? P.yx : P;

//         // The depth range that the ray covers within this loop iteration.
//         // Assume that the ray is moving in increasing z and swap if backwards.
//         float rayZMin = prevZMaxEstimate;
//         // Compute the value at 1/2 pixel into the future
//         float rayZMax = (dQ.z * 0.5 + Q.z) / (dk * 0.5 + k);
//         prevZMaxEstimate = rayZMax;
//         if (rayZMin > rayZMax) {
//             swap(rayZMin, rayZMax);
//         }

//         // Camera-space z of the background at each layer (there can be up to 4)
//         // vec4 sceneZMax = texelFetch(csZBuffer, int2(hitPixel), 0); NOTE: Typo??
//         vec4 sceneZMax = texelFetch(csZBuffer, ivec2(hitPixel), 0);

//         if (csZBufferIsHyperbolic) {
//             int layer = 0;
//             // for (int layer = 0; layer < numLayers; ++layer)
//             // sceneZMax[layer] = reconstructCSZ(sceneZMax[layer], clipInfo);
//             // endfor
//         }
//         vec4 sceneZMin = sceneZMax - zThickness;

//         int L = 0;
//         // # for (int L = 0; L < numLayers; ++L)
//         if (((rayZMax >= sceneZMin[L]) && (rayZMin <= sceneZMax[L])) ||
//             (sceneZMax[L] == 0)) {
//             hitLayer = L;
//             break; // Breaks out of both loops, since the inner loop is a macro
//         }
//         // # endfor // layer
//     } // for each pixel on ray

//     // Advance Q based on the number of steps
//     Q.xy += dQ.xy * stepCount;
//     // hitPoint = Q * (1.0 / k); NOTE: Typo? hitPoint is undeclared
//     csHitPoint = Q * (1.0 / k);
//     return all(lessThanEqual(abs(hitPixel - (csZBufferSize * 0.5)), csZBufferSize * 0.5));
// }

// Returns distance to plane along ray_dir
bool rayPlaneIntersection(vec3 ray_origin, vec3 ray_dir, vec3 point_on_plane, vec3 plane_normal, out float hit_depth) {
    float denom = dot(plane_normal, ray_dir);
    if (denom < -1e-6) {
        float t = dot(point_on_plane - ray_origin, plane_normal) / denom;
        hit_depth = t;
        return true;
    }
    return false;
}

// paper approach
bool trace(vec3 ray_origin, vec3 ray_dir, ScreenSpaceReflection ssr, mat4 pixel_projection, out vec2 hit_pixel, out vec3 hit_point) {
    // Clip to the near plane
    float ray_length = ((ray_origin.z + ray_dir.z * ssr.max_distance) > ssr.z_near) ?
        (ssr.z_near - ray_origin.z) / ray_dir.z : ssr.max_distance;
    vec3 end_point = ray_origin + ray_dir * ray_length;

    // Init to off screen
    hit_pixel = vec2(-1.0);

    // Project into homogeneous clip space
    vec4 H0 = pixel_projection * vec4(ray_origin, 1.0);
    vec4 H1 = pixel_projection * vec4(end_point, 1.0);

    float k0 = 1.0 / H0.w;
    float k1 = 1.0 / H1.w;

    // The interpolated homogeneous version of the camera-space points
    vec3 Q0 = ray_origin * k0;
    vec3 Q1 = end_point * k1;

    // Screen-space endpoints
    vec2 P0 = H0.xy * k0;
    vec2 P1 = H1.xy * k1;

    P1 += vec2(distanceSquared(P0, P1) < 0.0001 ? 0.01 : 0.0);
    vec2 delta = P1 - P0;

    // the primary iteration is in x direction so permute P if vertical line
    bool permute = false;
    if (abs(delta.x) < abs(delta.y)) {
        // more-vertical line
        permute = true;
        delta = delta.yx;
        P0 = P0.yx;
        P1 = P1.yx;
    }

    float step_dir = sign(delta.x);
    float invdx = step_dir / delta.x;

    // derivatives of Q and k
    vec3 dQ = (Q1 - Q0) * invdx;
    float dk = (k1 - k0) * invdx;
    vec2 dP = vec2(step_dir, delta.y * invdx);

    //
    dP *= ssr.stride;
    dQ *= ssr.stride;
    dk *= ssr.stride;

    // Offset starting point by the jitter
    P0 += dP * ssr.jitter;
    Q0 += dQ * ssr.jitter;
    k0 += dk * ssr.jitter;

    // Slide P from P0 to P1, Q from Q0 to Q1, k from k0 to k1
    vec2 P = P0;
    vec3 Q = Q0;
    float k = k0;

    float end = P1.x * step_dir;

    float step_count = 0.0;
    float prev_z_max_estimate = ray_origin.z;
    float ray_z_min = prev_z_max_estimate;
    float ray_z_max = prev_z_max_estimate;
    float scene_z_max = ray_z_max + 1e4;

    // trace until intersection reached max steps
    while (((P.x * step_dir) <= end) && (step_count < ssr.max_steps) && ((ray_z_max < (scene_z_max - ssr.z_thickness)) || (ray_z_min > scene_z_max)) && (scene_z_max != 0.0)) {
        ray_z_min = prev_z_max_estimate;
        ray_z_max = (dQ.z * 0.5 + Q.z) / (dk * 0.5 + k);
        prev_z_max_estimate = ray_z_max;

        if (ray_z_min > ray_z_max) {
            swap(ray_z_min, ray_z_max);
        }

        hit_pixel = permute ? P.yx : P;

        {
            float z_b = texelFetch(depth_buffer, ivec2(hit_pixel), 0).r;
            float z_n = 2.0 * z_b - 1.0;
            float z_e = 2.0 * ssr.z_near * ssr.z_far / (ssr.z_far + ssr.z_near - z_n * (ssr.z_far - ssr.z_near));
            scene_z_max = z_e;
        }

        // Step
        P += dP;
        Q.z += dQ.z;
        k += dk;
        step_count++;
    }

    // Advance Q based on the number of steps
    Q.xy += dQ.xy * step_count;
    hit_point = Q * (1.0 / k);
    return (ray_z_max >= scene_z_max - ssr.z_thickness) && (ray_z_min < scene_z_max);
}


vec3 naive_impl(vec3 ray_origin, vec3 ray_dir, ScreenSpaceReflection ssr) {
    return vec3(0);
}

vec3 getScreenPos(vec3 view_pos) {
    vec4 screen_pos_h = projection_matrix * vec4(view_pos, 1.0);
    vec3 screen_pos = screen_pos_h.xyz / screen_pos_h.w;
    return screen_pos + vec3(1.0) / 2.0;
}

void main() {
    // REVIEW: Does `texture` filter the result? maybe opt for `texelFetch` to get unfiltered value
    vec3 color = texture(pixel_buffer, In.tex_coord, 0).xyz;
    // NOTE: The depth buffer is hyperbolic i.e. not linear
    float depth = texture(depth_buffer, In.tex_coord, 0).x;

    // Construct Screen Space position - each component is in range [-1, 1]
    vec3 screen_space_position = vec3(In.tex_coord, depth) * 2.0 - vec3(1);

    // View Space
    vec4 view_pos_h = inverse(projection_matrix) * vec4(screen_space_position, 1.0);
    vec3 view_pos = view_pos_h.xyz / view_pos_h.w;

    // Since `view_pos` is reconstructed from screen space, it gives the position relative to the camera at the origin.
    // We just normalize it to get the view direction
    vec3 view_dir = normalize(view_pos);

    // Intersection with water plane
    vec3 view_water_normal = normalize((view_matrix * vec4(0.0, 1.0, 0.0, 0.0)).xyz);
    vec4 view_water_point_h = view_matrix * vec4(0.0, water.height, 0.0, 1.0);
    vec3 view_water_point = view_water_point_h.xyz / view_water_point_h.w;

    float dist_to_water;
    bool intersected = rayPlaneIntersection(vec3(0), view_dir, view_water_point, view_water_normal, dist_to_water);

    // Calculate the reflected ray direction on the water surface
    vec4 world = inverse(view_matrix) * vec4(view_dir * dist_to_water, 1.0);
    vec4 view_normal_offset = view_matrix * vec4(sin(world.x / 10) * 0.05, 0, 0, 0.0);

    // vec3 reflection_dir = normalize(reflect(view_dir, view_water_normal + view_normal_offset.xyz));
    vec3 reflection_dir = normalize(reflect(view_dir, view_water_normal));

    float foam_mask;
    vec3 out_color = color;

    if (intersected) {
        if (dist_to_water < length(view_pos)) {
            float f = length(view_pos) - dist_to_water;

            foam_mask += max(1.0 - f / water.foam_distance, 0);
            foam_mask *= max(sin(f / 1.5 + sin(current_time * 1.0) * 4), 0);

            foam_mask += max(1.0 - f / (water.foam_distance / 2.0), 0);

            out_color = normalize((inverse(view_matrix) * vec4(reflection_dir, 0.0)).xyz);

            // reflection based on paper
            // -----------
            vec2 hit_pixel;
            vec3 view_hit_point;
            bool reflection_hit = trace(view_dir * abs(dist_to_water-0.01), reflection_dir, ssr, hit_pixel, view_hit_point);
            vec4 reflection_color = texture(pixel_buffer, hit_pixel);

            if (reflection_hit) {
                out_color = mix(reflection_color.xyz, out_color, 0.1);
                // out_color = vec3(normalize(hit_pixel), 0.0);
                // out_color = normalize(reflection_dir);
            }
            else {
                out_color = vec3(0.0, 0.0, 0.0);
                out_color = normalize(reflection_dir);
            }

            // naive reflection
            // -----------
            // {
            //     vec3 screen_pos = getScreenPos(view_pos);
            //     vec2 screen_tex_pos = screen_pos.xy;

            //     float texture_depth = texture(depth_buffer, screen_tex_pos).x;
            //     float world_depth = screen_pos.z;

            //     bool run = true;
            //     int count = 0;
            //     vec3 P = view_pos;
            //     while (run) {
            //         // texture_depth = linearizeDepth(texture(depth_buffer, screen_tex_pos).x);
            //         texture_depth = texture(depth_buffer, screen_tex_pos).x;
            //         world_depth = screen_pos.z;

            //         if (texture_depth < world_depth) {
            //             screen_tex_pos.y = 1.0 - screen_tex_pos.y;
            //             out_color = texture(pixel_buffer, screen_tex_pos).xyz;
            //             break;
            //         }

            //         P += reflection_dir * ssr.stride;
            //         screen_pos = getScreenPos(P);
            //         screen_tex_pos = screen_pos.xy;

            //         count += 1;
            //         run = count < ssr.max_steps;
            //     }

            //     if (!run) {
            //         out_color = vec3(0.0, 1.0, 0.0);
            //     }
            // }
        }
    }

    // fragmentColor = vec4(reflected_dir, 1.0);

    // out_color = dist_to_water * color;
    // out_color = (dot(view_dir, view_water_normal) + 1.0)/2.0 * color;
    // fragmentColor = vec4(vec3(abs(view_pos.z), 0.0, 0.0) / water_height, 1.0);

    fragmentColor = vec4(out_color, 1.0);
    // fragmentColor = vec4(out_color + vec3(foam_mask * 0.5), 1.0);
}
