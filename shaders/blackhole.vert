#version 450

layout(location = 0) in vec4 inPos;
layout(location = 1) in vec4 inVel;

layout(push_constant) uniform PushConsts {
    mat4 viewProj;
    vec4 right;
    vec4 up;
    vec4 back;
    vec4 proj;
} pc;

layout(location = 0) out vec2 outQ;
layout(location = 1) flat out vec3 outN;
layout(location = 2) flat out float outSeed;

const float BH_K = 0.06;
const float QUAD = 6.0;
const float MIN_PX = 2.0;
const float MIN_PX_GALAXY = 1.5;

void main() {
    vec2 corners[6] = vec2[6](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));
    vec2 c = corners[gl_VertexIndex];
    vec4 clip = pc.viewProj * vec4(inPos.xyz, 1.0);
    float m = inPos.w;
    if (m <= 0.0 || clip.w < 0.05) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        outQ = vec2(0.0);
        outN = vec3(0.0, 0.0, 1.0);
        outSeed = 0.0;
        return;
    }
    float base = BH_K * pow(m, 1.0 / 3.0);
    if (pc.proj.w > 0.0) base = pc.proj.w;
    if (pc.back.w > 1.5) base = m;
    float rs = max(base, (pc.proj.z > 0.5 ? MIN_PX_GALAXY : MIN_PX) * clip.w / pc.up.w);
    clip.xy += c * QUAD * rs * pc.proj.xy;
    gl_Position = clip;
    outQ = c * QUAD;

    vec3 n = vec3(0.0, 1.0, 0.0);
    if (pc.proj.z > 0.5 || pc.back.w > 1.5) n = normalize(inVel.xyz);
    if (pc.back.w > 0.5 && pc.back.w < 1.5 && gl_InstanceIndex == 1) {
        float sx = sin(0.9), cx = cos(0.9), sy = sin(0.35), cy = cos(0.35);
        n = vec3(sx * sy, cx, sx * cy);
    }
    outN = vec3(dot(n, pc.right.xyz), dot(n, pc.up.xyz), dot(n, pc.back.xyz));
    outSeed = float(gl_InstanceIndex) * 17.0 + pc.right.w * 0.0;
}
