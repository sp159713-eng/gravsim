#version 450

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConsts {
    vec4 eye;
    vec4 fwd;
    vec4 right;
    vec4 up;
    vec4 halo;
    vec4 params;
} pc;

void main() {
    vec3 d = normalize(pc.fwd.xyz + inNdc.x * pc.params.y * pc.right.xyz - inNdc.y * pc.params.z * pc.up.xyz);
    vec3 oc = pc.eye.xyz - pc.halo.xyz;
    float R = pc.params.w;
    float core = pc.halo.w;
    float t0 = -dot(oc, d);
    float b2 = max(dot(oc, oc) - t0 * t0, 0.0);
    if (b2 >= R * R) discard;
    float L = sqrt(R * R - b2);
    float tA = max(t0 - L, 0.0);
    float tB = t0 + L;
    if (tB <= tA) discard;
    float s2 = core * core + b2;
    float xa = tA - t0;
    float xb = tB - t0;
    float column = (xb / (s2 * sqrt(s2 + xb * xb)) - xa / (s2 * sqrt(s2 + xa * xa))) * core * core * 0.5;
    float edge = 1.0 - smoothstep(0.55 * R, R, sqrt(b2));
    vec3 purple = vec3(0.50, 0.20, 1.0);
    outColor = vec4(purple * column * edge * pc.params.x, 1.0);
}
