#version 450

layout(location = 0) in vec3 inCol;
layout(location = 1) in float inSharp;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    float d2 = dot(c, c);
    if (d2 > 1.0) discard;
    float wgt = exp(-d2 * mix(3.0, 4.5, inSharp));
    outColor = vec4(inCol * wgt, wgt);
}
