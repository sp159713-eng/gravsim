#version 450

layout(location = 0) in vec4 inPos;
layout(location = 1) in vec4 inColor;
layout(location = 2) in float inFade;

layout(push_constant) uniform PushConsts {
    mat4 viewProj;
    vec4 params;
} pc;

layout(location = 0) out vec3 outColor;

void main() {
    vec4 clip = pc.viewProj * vec4(inPos.xyz, 1.0);
    gl_Position = clip;
    float w = max(clip.w, 1e-6);
    float real = inPos.w * pc.params.y / w;
    float size = clamp(real, pc.params.z, pc.params.w);
    float k = 1.0;
    if (real < pc.params.z) k = pow(real / pc.params.z, 0.55);
    else if (real > pc.params.w) k = pow(pc.params.w / real, 2.0);
    k /= 1.0 + pow(w / inFade, 3.0);
    gl_PointSize = size;
    outColor = pow(inColor.rgb, vec3(2.2)) * inColor.w * k * pc.params.x;
}
