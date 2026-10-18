#version 450

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform P {
    vec4 a;
    vec4 s0;
    vec4 s1;
    vec4 s2;
    vec4 s3;
    vec4 ids;
} pc;

float hash(vec2 p) {
    vec3 q = fract(vec3(p.xyx) * 0.1031);
    q += dot(q, q.yzx + 33.33);
    return fract((q.x + q.y) * q.z);
}

float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i), hash(i + vec2(1, 0)), f.x), mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}

float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) {
        s += a * vnoise(p);
        p = p * 2.03 + 11.7;
        a *= 0.5;
    }
    return s;
}

vec3 bb(float T) {
    vec3 c = vec3(1.0, 0.30, 0.10);
    c = mix(c, vec3(1.0, 0.42, 0.16), smoothstep(3300.0, 4000.0, T));
    c = mix(c, vec3(1.0, 0.62, 0.30), smoothstep(4000.0, 5000.0, T));
    c = mix(c, vec3(1.0, 0.90, 0.62), smoothstep(5000.0, 5800.0, T));
    c = mix(c, vec3(1.0, 0.96, 0.88), smoothstep(5800.0, 7500.0, T));
    c = mix(c, vec3(0.80, 0.88, 1.0), smoothstep(7500.0, 11000.0, T));
    c = mix(c, vec3(0.40, 0.58, 1.0), smoothstep(11000.0, 40000.0, T));
    return c;
}

vec3 background(vec2 ndc) {
    vec3 col = vec3(0.004, 0.005, 0.010);
    for (int l = 0; l < 3; l++) {
        float n = 40.0 + 44.0 * float(l);
        vec2 g = ndc * n + float(l) * 17.0;
        vec2 id = floor(g);
        float h = hash(id + 3.1);
        if (h < 0.94) continue;
        vec2 f = fract(g) - 0.5 - (vec2(hash(id + 7.7), hash(id + 1.3)) - 0.5) * 0.6;
        float s = exp(-dot(f, f) * 70.0) * (0.2 + 0.8 * (h - 0.94) / 0.06);
        col += 0.55 * s * mix(vec3(1.0, 0.85, 0.7), vec3(0.75, 0.85, 1.0), hash(id + 9.9));
    }
    return col;
}

vec3 earth(vec3 n, float mu, float time, float r, out float a, float aaw) {
    a = clamp((1.0 - r) / aaw + 0.5, 0.0, 1.0);
    float w = time * 0.03;
    vec3 m = vec3(n.x * cos(w) + n.z * sin(w), n.y, -n.x * sin(w) + n.z * cos(w));
    vec2 ll = vec2(atan(m.x, m.z), m.y);
    float land = smoothstep(0.50, 0.54, fbm(ll * vec2(1.1, 1.9) * 2.2 + 3.0));
    float lat = abs(m.y);
    vec3 ocean = mix(vec3(0.03, 0.12, 0.36), vec3(0.07, 0.26, 0.55), fbm(ll * 6.0));
    vec3 ground = mix(vec3(0.16, 0.38, 0.14), vec3(0.55, 0.45, 0.25), smoothstep(0.35, 0.75, fbm(ll * 3.0 + 7.0)));
    vec3 surf = mix(ocean, ground, land);
    surf = mix(surf, vec3(0.92, 0.95, 1.0), smoothstep(0.82, 0.92, lat));
    float cloud = smoothstep(0.50, 0.72, fbm(vec2(ll.x + time * 0.02, ll.y * 2.2) * 3.0 + 9.0));
    surf = mix(surf, vec3(0.95), cloud * 0.85);
    vec3 L = normalize(vec3(-0.55, -0.45, 0.7));
    float diff = clamp(dot(n, L), 0.0, 1.0);
    vec3 col = surf * (0.05 + 0.95 * diff);
    col += vec3(0.25, 0.5, 1.0) * pow(1.0 - mu, 3.0) * (0.15 + 0.85 * diff) * 0.9;
    return col;
}

vec3 starDisc(vec3 n, float mu, float T, float seed, float time, float r, out float a, float aaw) {
    a = clamp((1.0 - r) / aaw + 0.5, 0.0, 1.0);
    float w = time * 0.02 + seed;
    vec3 m = vec3(n.x * cos(w) + n.z * sin(w), n.y, -n.x * sin(w) + n.z * cos(w));
    vec2 ll = vec2(atan(m.x, m.z), m.y);
    float giant = smoothstep(5200.0, 4200.0, T);
    float midStar = smoothstep(9000.0, 6000.0, T) * (1.0 - giant);
    float big = fbm(ll * vec2(1.5, 2.6) * 1.7 + seed * 7.3);
    float mid = fbm(ll * vec2(4.0, 7.0) * 1.7 + seed * 3.1);
    float fine = fbm(ll * vec2(14.0, 24.0) + seed * 5.0);
    float v = 1.0 + 0.95 * giant * (big - 0.5) * 2.0 + 0.35 * giant * (mid - 0.5) * 2.0 + 0.22 * midStar * (fine - 0.5) * 2.0 + 0.05 * (fine - 0.5);
    float spots = smoothstep(0.64, 0.74, fbm(ll * vec2(2.2, 4.0) * 1.7 + seed * 11.0));
    v *= 1.0 - 0.55 * spots * (giant * 0.8 + midStar * 0.22);
    float u = mix(0.35, 0.65, giant + midStar * 0.5);
    float limb = 1.0 - u * (1.0 - mu);
    vec3 base = bb(T);
    vec3 warm = bb(T + 900.0);
    vec3 col = mix(base, warm, clamp((v - 1.0) * 1.5, 0.0, 1.0)) * limb * max(v, 0.2);
    return col;
}

vec3 starGlow(float x, float T, float scale) {
    vec3 tint = pow(bb(T), vec3(1.7));
    float g = 0.26 * exp(-x * 8.0 / scale) + 0.05 * exp(-x * 1.8 / scale);
    return tint * g;
}

void main() {
    float aspect = pc.a.x;
    float time = pc.a.y;
    float pxu = pc.a.z;
    vec2 p = vec2(inNdc.x * aspect, inNdc.y);
    vec3 col = background(inNdc);
    vec4 st[4];
    st[0] = pc.s0; st[1] = pc.s1; st[2] = pc.s2; st[3] = pc.s3;
    float disk[4];
    vec3 diskCol[4];
    for (int j = 0; j < 4; j++) {
        disk[j] = 0.0;
        diskCol[j] = vec3(0.0);
        float id = pc.ids[j];
        if (id < -0.5 || st[j].z <= 0.0) continue;
        float R = max(st[j].z, 1.4 * pxu);
        vec2 d = p - st[j].xy;
        float dist = length(d);
        float r = dist / R;
        float cap = min(R, 0.5);
        float gx = max(dist - R, 0.0) / cap;
        if (gx > 12.0 && r > 1.0) continue;
        float aaw = pxu / R;
        float T = st[j].w;
        if (id < 0.5) {
            col += vec3(0.2, 0.45, 1.0) * 0.35 * exp(-gx * 22.0) * step(1.0, r);
        } else {
            col += starGlow(gx, T, 1.0 + 0.35 * smoothstep(8000.0, 30000.0, T));
        }
        if (r < 1.0 + aaw) {
            float rr = min(r, 1.0);
            float mu = sqrt(max(1.0 - rr * rr, 0.0));
            vec3 n = vec3(d / R, mu);
            float a;
            vec3 c;
            if (id < 0.5) c = earth(n, mu, time, r, a, aaw);
            else c = starDisc(n, mu, T, id, time, r, a, aaw);
            disk[j] = a;
            diskCol[j] = c;
        }
    }
    for (int j = 0; j < 4; j++) col = mix(col, diskCol[j], disk[j]);
    outColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
