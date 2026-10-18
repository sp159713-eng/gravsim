#version 450

layout(location = 0) in vec3 inColor;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 d = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(d, d);
    if (r2 >= 1.0) discard;
    float glow = exp(-r2 * 3.5) * (1.0 - r2);
    outColor = vec4(inColor * glow, 1.0);
}
