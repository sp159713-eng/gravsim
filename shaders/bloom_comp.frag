#version 450

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D scene;
layout(set = 0, binding = 1) uniform sampler2D glowNear;
layout(set = 0, binding = 2) uniform sampler2D glowFar;
layout(push_constant) uniform Push {
    vec2 texel;
    float threshold;
    float strength;
} pc;

void main() {
    vec2 uv = inNdc * 0.5 + 0.5;
    vec3 c = texture(scene, uv).rgb;
    vec3 g = texture(glowNear, uv).rgb * 0.8 + texture(glowFar, uv).rgb * 1.1;
    outColor = vec4(c + g * pc.strength, 1.0);
}
