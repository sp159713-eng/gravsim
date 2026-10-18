#version 450

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform P {
    vec4 a;
} pc;

const float PI = 3.14159265;
const float TANH = 0.57735;

float h31(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.x + p.y) * p.z);
}

float vn(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(h31(i), h31(i + vec3(1, 0, 0)), f.x), mix(h31(i + vec3(0, 1, 0)), h31(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(h31(i + vec3(0, 0, 1)), h31(i + vec3(1, 0, 1)), f.x), mix(h31(i + vec3(0, 1, 1)), h31(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

float fbm(vec3 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) {
        s += a * vn(p);
        p = p * 2.03 + 11.7;
        a *= 0.5;
    }
    return s;
}

vec3 stars(vec3 rd, float t) {
    vec3 col = vec3(0.0);
    for (int L = 0; L < 3; L++) {
        float sc = 90.0 + 110.0 * float(L);
        vec3 q = rd * sc;
        vec3 id = floor(q);
        vec3 f = fract(q) - 0.5;
        float h = h31(id + float(L) * 17.0);
        if (h > 0.972) {
            float d = length(f - (vec3(h31(id + 3.1), h31(id + 7.7), h31(id + 1.3)) - 0.5) * 0.5);
            float tw = 0.65 + 0.35 * sin(t * (1.0 + 4.0 * h) + h * 60.0);
            float b = smoothstep(0.28, 0.0, d) * tw * (0.5 + 1.2 * fract(h * 91.7));
            vec3 tint = mix(vec3(0.7, 0.8, 1.0), vec3(1.0, 0.82, 0.6), fract(h * 33.3));
            col += tint * b;
        }
    }
    float band = exp(-pow(dot(rd, normalize(vec3(0.3, 0.9, 0.2))) * 3.2, 2.0));
    col += vec3(0.05, 0.06, 0.10) * band * (0.4 + fbm(rd * 6.0));
    return col;
}

void main() {
    float aspect = pc.a.x, t = pc.a.y, fade = pc.a.z;
    vec2 p = vec2(inNdc.x * aspect, inNdc.y);
    p += 0.025 * vec2(sin(t * 0.13), cos(t * 0.09));
    vec3 rd = normalize(vec3(p * TANH, -1.0));
    vec3 sunDir = normalize(vec3(0.62, 0.38, -0.70));
    vec3 C = vec3(1.7 + 0.08 * sin(t * 0.11), -0.35 + 0.05 * cos(t * 0.07), -5.2);
    float R = 1.9;
    vec3 col = stars(rd, t);

    vec3 ringN = normalize(vec3(0.22, 1.0, 0.62));
    vec3 ringU = normalize(cross(ringN, vec3(0.0, 0.0, 1.0)));
    vec3 ringV = cross(ringN, ringU);

    vec3 oc = -C;
    float b = dot(oc, rd);
    float cc = dot(oc, oc) - R * R;
    float disc = b * b - cc;
    float tPlanet = 1e9;
    vec3 planet = vec3(0.0);
    float halo = 0.0;
    vec3 closest = rd * max(-b, 0.0);
    float dmin = length(closest - C);

    if (disc > 0.0) {
        tPlanet = -b - sqrt(disc);
        vec3 hit = rd * tPlanet;
        vec3 n = normalize(hit - C);
        float ang = t * 0.018;
        float ca = cos(ang), sa = sin(ang);
        vec3 on = vec3(ca * n.x + sa * n.z, n.y, -sa * n.x + ca * n.z);
        float land = smoothstep(0.50, 0.54, fbm(on * 2.4 + 3.0));
        float cloud = smoothstep(0.52, 0.8, fbm(on * 3.6 + vec3(t * 0.01, 0.0, 7.0)));
        vec3 ocean = mix(vec3(0.02, 0.06, 0.16), vec3(0.05, 0.12, 0.26), fbm(on * 8.0));
        vec3 ground = mix(vec3(0.07, 0.06, 0.05), vec3(0.14, 0.12, 0.10), fbm(on * 10.0));
        vec3 alb = mix(ocean, ground, land);
        alb = mix(alb, vec3(0.30, 0.28, 0.27), cloud * 0.8);
        float ndl = dot(n, sunDir);
        float lit = smoothstep(-0.08, 0.35, ndl);
        vec3 day = alb * lit * 2.4;
        float night = 1.0 - smoothstep(-0.25, 0.10, ndl);
        float fireField = fbm(on * 10.0 + 21.0);
        float fire = smoothstep(0.66, 0.76, fireField) * land * 0.8;
        float spark = smoothstep(0.80, 1.0, vn(on * 55.0 + t * 0.6)) * land * smoothstep(0.58, 0.64, fireField);
        float flick = 0.75 + 0.25 * sin(t * 3.0 + fireField * 40.0);
        vec3 glow = (vec3(1.0, 0.36, 0.08) * fire * 0.9 + vec3(1.0, 0.7, 0.3) * spark * 1.6) * flick * night;
        vec3 smoke = vec3(0.12, 0.03, 0.01) * smoothstep(0.55, 0.70, fireField) * night * 0.35;
        vec3 amb = alb * 0.012;
        float fres = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);
        vec3 atm = mix(vec3(0.28, 0.06, 0.02), vec3(0.30, 0.50, 1.0), smoothstep(-0.2, 0.5, ndl)) * fres * (0.25 + 1.6 * max(ndl + 0.25, 0.0));
        planet = day + glow + smoke + amb + atm;
        col = planet;
    } else {
        float edge = dmin - R;
        vec3 outward = normalize(closest - C);
        float sunSide = clamp(dot(outward, sunDir) * 0.7 + 0.5, 0.0, 1.0);
        halo = exp(-max(edge, 0.0) * 9.0) * step(0.0, b);
        vec3 hc = mix(vec3(0.20, 0.05, 0.02), vec3(0.35, 0.55, 1.0), sunSide);
        col += hc * halo * (0.12 + 1.1 * sunSide * sunSide);
    }

    float denom = dot(rd, ringN);
    if (abs(denom) > 1e-4) {
        float tr = dot(C - vec3(0.0), ringN) / denom;
        if (tr > 0.0 && tr < tPlanet) {
            vec3 q = rd * tr - C;
            float rr = length(q) / R;
            if (rr > 1.35 && rr < 2.1) {
                float an = atan(dot(q, ringV), dot(q, ringU));
                float spin = t * 0.02 / rr;
                float band = 0.35 + 0.65 * vn(vec3(rr * 40.0, 3.0, 0.0));
                float dust = 0.05 * band * smoothstep(1.35, 1.6, rr) * smoothstep(2.1, 1.85, rr);
                vec3 cell = vec3(rr * 55.0, (an + spin) * 70.0 * rr, 2.0);
                float chunk = smoothstep(0.80, 0.9, vn(cell)) * smoothstep(1.35, 1.55, rr) * smoothstep(2.1, 1.9, rr);
                float shadow = (dot(q, sunDir) < 0.0 && length(q - sunDir * dot(q, sunDir)) < R) ? 0.08 : 1.0;
                float sl = 0.35 + 0.65 * max(dot(normalize(q), sunDir) * 0.5 + 0.5, 0.0);
                vec3 rc = vec3(0.55, 0.47, 0.40) * (dust * 3.0 + chunk * 1.4) * shadow * sl;
                col += rc;
            }
        }
    }

    vec2 sp = sunDir.xy / (-sunDir.z) / TANH;
    float sd = length(p - sp);
    col += vec3(1.0, 0.85, 0.6) * (0.012 / (sd * sd + 0.006) + 0.8 * exp(-sd * 14.0)) * 0.5;
    col += vec3(1.0, 0.9, 0.7) * 0.05 * exp(-abs(p.y - sp.y) * 35.0) * exp(-abs(p.x - sp.x) * 0.9);

    col = col / (1.0 + 0.5 * col);
    col = pow(col, vec3(0.92));
    float vigL = 0.30 + 0.70 * smoothstep(-0.78, 0.18, inNdc.x);
    float vigR = 1.0 - 0.35 * dot(inNdc, inNdc) * 0.5;
    col *= vigL * vigR * fade;
    outColor = vec4(col, 1.0);
}
