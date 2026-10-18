#version 450

layout(location = 0) in vec3 inColor;
layout(location = 1) in float inSize;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 d = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(d, d);
    if (r2 >= 1.0) discard;
    float r = sqrt(r2);
    float rpx = r * inSize * 0.5;
    float core = exp(-rpx * rpx * 1.2);
    float halo = exp(-r * 4.5) * (1.0 - r2) * 0.45;
    float glow = core + halo;
    vec3 hot = inColor * glow + vec3(core * core) * 0.35 * inColor.b * min(length(inColor), 1.0);
    outColor = vec4(hot, 1.0);
}
