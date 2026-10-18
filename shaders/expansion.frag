#version 450

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform P {
    vec4 col;
    vec4 p;
} pc;

void main() {
    float v = inNdc.y * 0.5 + 0.5;
    float mid = smoothstep(pc.p.x - 0.03, pc.p.x + 0.01, v) * (1.0 - smoothstep(pc.p.y - 0.01, pc.p.y + 0.03, v));
    vec2 q = inNdc * vec2(pc.p.w, 1.0);
    float glow = exp(-dot(q, q) * 0.9);
    vec3 base = vec3(0.012, 0.012, 0.025);
    vec3 c = base + pc.col.rgb * mid * (0.6 + 0.4 * glow) + pc.col.rgb * 0.06 * (1.0 - mid);
    outColor = vec4(c, 1.0);
}
