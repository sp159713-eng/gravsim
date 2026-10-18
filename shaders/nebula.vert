#version 450

layout(location = 0) in vec4 inPos;
layout(location = 1) in vec4 inCol;

layout(push_constant) uniform PushConsts {
    mat4 viewProj;
    vec4 params;
} pc;

layout(location = 0) out vec3 outColor;
layout(location = 1) out float outSeed;

void main() {
    vec4 clip = pc.viewProj * vec4(inPos.xyz, 1.0);
    gl_Position = clip;
    float w = max(clip.w, 1e-3);
    float raw = inPos.w * 2.0 * pc.params.x / w;
    float px = clamp(raw, pc.params.z, pc.params.w);
    float a = pc.params.y;
    if (raw < pc.params.z) a *= 0.35 + 0.65 * raw / pc.params.z;
    if (raw > pc.params.w) a *= pc.params.w / raw;
    a *= smoothstep(0.1, 1.2, w / inPos.w);
    gl_PointSize = px;
    outColor = pow(inCol.rgb, vec3(2.2)) * a;
    outSeed = inCol.w;
}
