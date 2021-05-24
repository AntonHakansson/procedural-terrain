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

uniform float water_height;

struct ScreenSpaceReflection {
    ivec2 depth_buffer_size;
    float z_near;

    // Thickness to assign each pixel in the depth buffer
    float z_thickness;
    // The camera-space distance to step in each iteration
    float stride;
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
    vec4 view_water_point_h = view_matrix * vec4(0.0, water_height, 0.0, 1.0);
    vec3 view_water_point = view_water_point_h.xyz / view_water_point_h.w;

    float dist_to_water;
    bool intersected = rayPlaneIntersection(vec3(0.0, 0.0, 0.0), view_dir, view_water_point, view_water_normal, dist_to_water);

    // Calculate the reflected ray direction on the water surface
    vec3 reflected_dir = normalize(reflect(view_dir, view_water_normal));

    vec3 out_color;
    if (intersected) {
        if (dist_to_water < -view_pos.z) {
            out_color = reflected_dir;
        }
        else {
            out_color = color;
        }
    }
    else {
        out_color = color;
    }
    fragmentColor = vec4(out_color, 1.0);
    // out_color = dist_to_water * color;
    // out_color = (dot(view_dir, view_water_normal) + 1.0)/2.0 * color;
    // fragmentColor = vec4(vec3(abs(view_pos.z), 0.0, 0.0) / water_height, 1.0);
}
