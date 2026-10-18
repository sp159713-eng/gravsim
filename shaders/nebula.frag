#version 450

layout(location = 0) in vec3 inColor;
layout(location = 1) in float inSeed;
layout(location = 0) out vec4 outColor;

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}

float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) {
        s += a * vnoise(p);
        p = p * 2.03 + 11.7;
        a *= 0.5;
    }
    return s;
}

void main() {
    vec2 uv = gl_PointCoord * 2.0 - 1.0;
    float r = length(uv);
    if (r >= 1.0) discard;
    float fall = smoothstep(1.0, 0.0, r);
    fall *= fall;
    float n = fbm(uv * 2.6 + inSeed * 37.0);
    float shape = fall * (0.15 + 1.5 * n * n);
    outColor = vec4(inColor * shape, 1.0);
}
