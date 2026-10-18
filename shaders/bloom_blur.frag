#version 450

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D src;
layout(push_constant) uniform Push {
    vec2 step;
    float pad0;
    float pad1;
} pc;

void main() {
    vec2 uv = inNdc * 0.5 + 0.5;
    vec3 c = texture(src, uv).rgb * 0.2270270270;
    c += (texture(src, uv + pc.step * 1.3846153846).rgb + texture(src, uv - pc.step * 1.3846153846).rgb) * 0.3162162162;
    c += (texture(src, uv + pc.step * 3.2307692308).rgb + texture(src, uv - pc.step * 3.2307692308).rgb) * 0.0702702703;
    outColor = vec4(c, 1.0);
}
