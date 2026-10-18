#version 450

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform P {
    vec4 a;
    vec4 b;
    vec4 c;
    vec4 d;
    vec4 e;
} pc;

const float PI = 3.14159265;
const float DIST = 6.0;
const float MOON_R = 0.3;
const float TILT = 0.40841;
const float INCL = 0.2443;
const float AMP_M = 1.0;
const float AMP_S = 0.46;
const float SHELL_BASE = 1.04;
const float SHELL_K = 0.2;
const vec3 SUN = vec3(1.0, 0.0, 0.0);
const vec3 CITY = vec3(1.0, 0.0, 0.0);

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
        p = p * 2.03 + 7.1;
        a *= 0.5;
    }
    return s;
}

float rs(vec3 ro, vec3 rd, vec3 c, float r) {
    vec3 oc = ro - c;
    float b = dot(oc, rd);
    float cc = dot(oc, oc) - r * r;
    float h = b * b - cc;
    if (h < 0.0) return -1.0;
    h = sqrt(h);
    float t = -b - h;
    if (t > 0.0) return t;
    t = -b + h;
    return t > 0.0 ? t : -1.0;
}

float p2(float x) {
    return 0.5 * (3.0 * x * x - 1.0);
}

vec3 moonPos() {
    float ph = pc.c.w;
    return DIST * vec3(cos(ph) * cos(INCL), cos(ph) * sin(INCL), -sin(ph));
}

float shellR(vec3 n, vec3 mh) {
    float h = AMP_M * p2(dot(n, mh)) + AMP_S * p2(dot(n, SUN));
    return SHELL_BASE + SHELL_K * h;
}

vec3 earthBody(vec3 n) {
    vec3 A = vec3(sin(TILT), cos(TILT), 0.0);
    vec3 E1 = vec3(cos(TILT), -sin(TILT), 0.0);
    float x = dot(n, E1), y = dot(n, A), z = n.z;
    float th = pc.d.w;
    return vec3(x * cos(th) - z * sin(th), y, x * sin(th) + z * cos(th));
}

vec3 moonSurf(vec3 l) {
    float m = fbm(l * 3.0);
    float maria = smoothstep(0.50, 0.62, fbm(l * 1.6 + vec3(4.0)));
    maria *= smoothstep(-0.9, 0.2, l.z);
    vec3 base = mix(vec3(0.66, 0.64, 0.60), vec3(0.28, 0.29, 0.33), maria);
    float cr = fbm(l * 9.0);
    return base * (0.72 + 0.5 * m) * (0.85 + 0.3 * cr);
}

vec3 space(vec3 rd) {
    vec3 p = rd * 150.0;
    vec3 i = floor(p);
    vec3 f = fract(p) - 0.5;
    float h = h31(i);
    vec3 off = (vec3(h31(i + 1.0), h31(i + 2.0), h31(i + 3.0)) - 0.5) * 0.6;
    float s = step(0.965, h) * smoothstep(0.3, 0.0, length(f - off)) * (0.35 + 0.65 * h31(i + 7.0));
    vec3 col = vec3(0.0015, 0.002, 0.004) + vec3(s) * mix(vec3(0.7, 0.8, 1.0), vec3(1.0, 0.9, 0.75), h31(i + 5.0));
    float sd = dot(rd, SUN);
    float disc = smoothstep(cos(0.05), cos(0.04), sd);
    col += vec3(1.0, 0.92, 0.65) * disc * 3.0;
    col += vec3(1.0, 0.75, 0.35) * pow(max(sd, 0.0), 90.0) * 0.8 + vec3(1.0, 0.7, 0.3) * pow(max(sd, 0.0), 12.0) * 0.06;
    return col;
}

vec3 earthColor(vec3 p, vec3 rd) {
    vec3 n = p;
    vec3 b = earthBody(n);
    float land = fbm(b * 2.2 + 3.0);
    land += 0.5 * smoothstep(0.55, 0.0, distance(b, CITY));
    float lm = smoothstep(0.52, 0.56, land);
    vec3 ocean = mix(vec3(0.015, 0.09, 0.32), vec3(0.04, 0.26, 0.5), smoothstep(0.35, 0.52, land));
    float veg = fbm(b * 5.0 + 11.0);
    vec3 ground = mix(vec3(0.10, 0.34, 0.10), vec3(0.50, 0.42, 0.24), smoothstep(0.4, 0.7, veg));
    ground = mix(ground, vec3(0.85, 0.85, 0.8), smoothstep(0.84, 0.92, abs(b.y)));
    vec3 surf = mix(ocean, ground, lm);
    float cl = smoothstep(0.56, 0.78, fbm(b * 3.5 + vec3(pc.e.w * 0.02)));
    surf = mix(surf, vec3(0.95), cl * 0.75);
    float ndl = dot(n, SUN);
    float day = smoothstep(-0.05, 0.28, ndl);
    vec3 col = surf * (day * 1.15 + 0.02);
    float oc = (1.0 - lm) * (1.0 - cl);
    vec3 hv = normalize(SUN - rd);
    col += vec3(1.0, 0.95, 0.85) * pow(max(dot(n, hv), 0.0), 90.0) * oc * day * 0.9;
    float fres = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);
    col += vec3(0.25, 0.5, 1.0) * fres * (0.1 + 0.9 * day) * 0.8;
    float ang = acos(clamp(dot(normalize(b), CITY), -1.0, 1.0));
    float pulse = 0.5 + 0.5 * sin(pc.e.w * 5.0);
    col = mix(col, vec3(1.0, 0.25, 0.1) * 1.6, smoothstep(0.05, 0.035, ang));
    col = mix(col, vec3(1.0, 0.95, 0.8) * 2.0, smoothstep(0.02, 0.012, ang));
    col += vec3(1.0, 0.3, 0.1) * smoothstep(0.012, 0.0, abs(ang - (0.08 + 0.03 * pulse))) * 0.9;
    return col;
}

vec3 moonColor(vec3 p, vec3 mc, vec3 u, vec3 right, vec3 upm) {
    vec3 mn = (p - mc) / MOON_R;
    vec3 l = vec3(dot(mn, right), dot(mn, upm), dot(mn, -u));
    float ndl = dot(mn, SUN);
    float lit = smoothstep(-0.01, 0.06, ndl);
    return moonSurf(l) * (0.035 + 1.15 * lit);
}

void main() {
    vec2 q = inNdc;
    vec3 ro = pc.a.xyz;
    vec3 rd = normalize(pc.b.xyz + pc.c.xyz * (q.x * pc.b.w * pc.a.w) + pc.d.xyz * (-q.y * pc.b.w));
    vec3 M = moonPos();
    vec3 u = normalize(M);
    vec3 right = normalize(cross(u, vec3(0, 1, 0)));
    vec3 upm = cross(right, u);

    float tE = rs(ro, rd, vec3(0.0), 1.0);
    float tM = rs(ro, rd, M, MOON_R);
    float tO = -1.0;
    int which = 0;
    if (tE > 0.0) { tO = tE; which = 1; }
    if (tM > 0.0 && (tO < 0.0 || tM < tO)) { tO = tM; which = 2; }

    vec3 col;
    if (which == 1) col = earthColor(ro + rd * tE, rd);
    else if (which == 2) col = moonColor(ro + rd * tM, M, u, right, upm);
    else {
        col = space(rd);
        float dEarth = length(cross(ro, rd));
        float along = -dot(ro, rd);
        if (along > 0.0 && dEarth > 1.0) {
            float g = exp(-(dEarth - 1.0) * 16.0);
            float day = 0.2 + 0.8 * smoothstep(-0.3, 0.5, dot(normalize(ro + rd * along), SUN));
            col += vec3(0.2, 0.45, 1.0) * g * 0.5 * day;
        }
    }

    float bb = dot(ro, rd);
    float hh = bb * bb - (dot(ro, ro) - 1.8 * 1.8);
    if (hh > 0.0) {
        float sq = sqrt(hh);
        float t0 = max(0.0, -bb - sq);
        float t1 = -bb + sq;
        if (tO > 0.0) t1 = min(t1, tO);
        if (t1 > t0) {
            vec3 mh = u;
            float dt = (t1 - t0) / 56.0;
            float thick = 0.0, litSum = 0.0;
            for (int i = 0; i < 56; i++) {
                vec3 p = ro + rd * (t0 + (float(i) + 0.5) * dt);
                float r = length(p);
                vec3 n = p / r;
                if (r > 1.0 && r < shellR(n, mh)) {
                    thick += dt;
                    litSum += dt * (0.12 + 0.88 * smoothstep(-0.15, 0.45, dot(n, SUN)));
                }
            }
            if (thick > 0.0) {
                float lit = litSum / thick;
                vec3 sc = mix(vec3(0.04, 0.18, 0.6), vec3(0.4, 0.85, 1.0), lit) * (0.2 + 1.0 * lit);
                float al = (1.0 - exp(-thick * 5.0)) * 0.85;
                col = mix(col, sc, al);
            }
        }
    }

    vec2 dq = vec2((q.x - pc.e.x) * pc.a.w, q.y - pc.e.y);
    float rr = length(dq) / pc.e.z;
    if (rr < 1.22) {
        vec3 ic = vec3(0.012, 0.016, 0.035);
        if (rr < 1.0) {
            vec3 n = vec3(dq.x / pc.e.z, -dq.y / pc.e.z, sqrt(max(0.0, 1.0 - rr * rr)));
            vec3 L = vec3(dot(SUN, right), dot(SUN, upm), dot(SUN, -u));
            float lit = smoothstep(-0.012, 0.035, dot(n, L));
            ic = moonSurf(n) * (0.04 + 1.15 * lit);
            ic *= 0.8 + 0.2 * n.z;
        }
        float ring = smoothstep(0.02, 0.0, abs(rr - 1.13));
        ic = mix(ic, vec3(0.45, 0.58, 0.85), ring);
        col = ic;
    }

    col = clamp(col, 0.0, 1.5);
    outColor = vec4(col, 1.0);
}
