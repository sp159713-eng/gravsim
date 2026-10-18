#version 450

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform P {
    vec4 F;
    vec4 R;
    vec4 U;
    vec4 Z;
    vec4 N;
    vec4 E;
    vec4 q;
} p;

float h31(vec3 q) {
    q = fract(q * 0.3183099 + 0.1);
    q *= 17.0;
    return fract(q.x * q.y * q.z * (q.x + q.y + q.z));
}

float vnoise(vec3 x) {
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(h31(i), h31(i + vec3(1, 0, 0)), f.x), mix(h31(i + vec3(0, 1, 0)), h31(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(h31(i + vec3(0, 0, 1)), h31(i + vec3(1, 0, 1)), f.x), mix(h31(i + vec3(0, 1, 1)), h31(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

float fbm(vec3 x) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) {
        v += a * vnoise(x);
        x = x * 2.07 + 11.3;
        a *= 0.5;
    }
    return v;
}

float hillAlt(float az) {
    return 0.035 + 0.020 * sin(3.0 * az + 1.0) + 0.012 * sin(7.0 * az + 2.0) + 0.006 * sin(17.0 * az + 0.5);
}

const vec3 GP = vec3(-0.8676, -0.1980, 0.4560);
const vec3 GC = vec3(-0.0549, -0.8734, -0.4838);

void main() {
    float th = p.q.x, asp = p.q.y, blend = p.q.z;
    vec3 d = normalize(p.F.xyz + p.R.xyz * (inNdc.x * th * asp) - p.U.xyz * (inNdc.y * th));
    vec3 col = vec3(0.003, 0.005, 0.012);

    float g = dot(d, GP);
    float band = exp(-g * g / (2.0 * 0.10 * 0.10));
    float core = 0.45 + 0.9 * max(dot(d, GC), 0.0);
    float n = fbm(d * 10.0);
    float dust = fbm(d * 22.0 + 3.0);
    float mw = band * core * (0.25 + 1.1 * n) * (1.0 - 0.5 * smoothstep(0.4, 0.75, dust));
    col += vec3(0.42, 0.46, 0.62) * mw * 0.085;
    col += vec3(0.5, 0.42, 0.34) * band * band * max(dot(d, GC), 0.0) * 0.03;

    float s = dot(d, p.Z.xyz);
    float alt = asin(clamp(s, -1.0, 1.0));
    float az = atan(dot(d, p.E.xyz), dot(d, p.N.xyz));
    if (blend > 0.001) {
        float glow = exp(-max(alt, 0.0) * 7.0);
        col += vec3(0.30, 0.20, 0.13) * glow * 0.12 * blend;
        col += vec3(0.008, 0.014, 0.03) * (1.0 - smoothstep(0.0, 1.2, alt)) * blend;
        float h = hillAlt(az);
        float m = smoothstep(h + 0.0012, h - 0.0012, alt) * blend;
        vec3 ground = vec3(0.004, 0.004, 0.006) + vec3(0.03, 0.02, 0.014) * exp(-max(h - alt, 0.0) * 40.0) * 0.15;
        col = mix(col, ground, m);
    }
    col += (h31(vec3(gl_FragCoord.xy, 1.0)) - 0.5) * 0.004;
    outColor = vec4(col, 1.0);
}
