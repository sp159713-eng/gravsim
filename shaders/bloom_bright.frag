#version 450

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D src;
layout(push_constant) uniform Push {
    vec2 texel;
    float threshold;
    float strength;
} pc;

vec3 bright(vec2 uv) {
    vec3 c = texture(src, uv).rgb;
    float l = max(c.r, max(c.g, c.b));
    return c * (max(l - pc.threshold, 0.0) / max(l, 1e-4));
}

void main() {
    vec2 uv = inNdc * 0.5 + 0.5;
    vec3 sum = bright(uv + pc.texel * vec2(-1.0, -1.0)) + bright(uv + pc.texel * vec2(1.0, -1.0))
             + bright(uv + pc.texel * vec2(-1.0, 1.0)) + bright(uv + pc.texel * vec2(1.0, 1.0));
    outColor = vec4(sum * 0.25, 1.0);
}
