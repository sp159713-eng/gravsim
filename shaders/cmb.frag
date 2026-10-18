#version 450

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConsts {
    vec4 eye;
    vec4 fwd;
    vec4 right;
    vec4 up;
    vec4 params;
} pc;

float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(vec3 x) {
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash(i), hash(i + vec3(1, 0, 0)), f.x),
                   mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x),
                   mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

void main() {
    vec3 d = normalize(pc.fwd.xyz + inNdc.x * pc.params.y * pc.right.xyz - inNdc.y * pc.params.z * pc.up.xyz);
    vec3 e = pc.eye.xyz;
    float R = pc.params.w;
    float b = dot(e, d);
    float c = dot(e, e) - R * R;
    float disc = b * b - c;
    if (disc < 0.0) discard;
    float sq = sqrt(disc);
    float t = c < 0.0 ? -b + sq : -b - sq;
    if (t < 0.0) discard;
    vec3 n = normalize(e + d * t);
    float f = 0.0;
    float amp = 0.5;
    vec3 p = n * 6.0;
    for (int i = 0; i < 6; i++) {
        f += amp * noise(p);
        p = p * 2.07 + 11.3;
        amp *= 0.55;
    }
    f = clamp((f - 0.45) * 1.9 + 0.5, 0.0, 1.0);
    vec3 cold = vec3(0.10, 0.12, 0.35);
    vec3 mid = vec3(0.85, 0.34, 0.10);
    vec3 hot = vec3(1.0, 0.72, 0.30);
    vec3 col = f < 0.5 ? mix(cold, mid, f * 2.0) : mix(mid, hot, (f - 0.5) * 2.0);
    float rim = c < 0.0 ? 1.0 : 0.5 + 0.5 * (1.0 - abs(dot(n, d)));
    outColor = vec4(col * rim * pc.params.x, 1.0);
}
