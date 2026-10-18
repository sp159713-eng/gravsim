#version 450

layout(location = 0) in vec4 inPos;
layout(location = 1) in vec3 inColor;

layout(push_constant) uniform PushConsts {
    mat4 viewProj;
    vec4 params;
    vec4 params2;
} pc;

layout(location = 0) out vec3 outColor;
layout(location = 1) out float outSize;

void main() {
    vec4 clip = pc.viewProj * vec4(inPos.xyz, 1.0);
    gl_Position = clip;
    float dist = max(clip.w, 1.0);
    float lumScale = pc.params2.y > 0.5 ? 1.0 : clamp(pow(inPos.w, 0.4), 0.6, 1.9);
    float size = clamp(pc.params.y + pc.params.z / dist, pc.params.y, pc.params.w) * lumScale;
    size = clamp(size, 3.0, pc.params2.x);
    gl_PointSize = size;
    outSize = size;
    outColor = pow(inColor, vec3(2.2)) * inPos.w * pc.params.x * 0.6;
}
