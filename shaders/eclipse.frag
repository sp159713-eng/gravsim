#version 450

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform P {
    vec4 a;
    vec4 b;
    vec4 c;
    vec4 d;
    vec4 e;
    vec4 f;
    vec4 g;
    vec4 h;
} pc;

const float PI = 3.14159265;
const float ER = 1.0;
const float MR = 0.27;
const float ORB = 9.0;
const float INC = 0.2618;
const float SD = 16.0;
const float SR = 1.8;
const float LE = 22.0;
const float LM = 11.5;
const float TILT = 0.40841;
const vec3 TOSUN = vec3(-1.0, 0.0, 0.0);
const vec3 AX = vec3(1.0, 0.0, 0.0);

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

float fbm4(vec3 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 3; i++) {
        s += a * vn(p);
        p = p * 2.03 + 7.1;
        a *= 0.5;
    }
    return s * 1.15;
}

float rsph(vec3 ro, vec3 rd, vec3 c, float r) {
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

vec3 stars(vec3 rd, float scale, float thr) {
    vec3 p = rd * scale;
    vec3 i = floor(p);
    vec3 f = fract(p) - 0.5;
    float h = h31(i);
    vec3 off = (vec3(h31(i + 1.0), h31(i + 2.0), h31(i + 3.0)) - 0.5) * 0.6;
    float s = step(thr, h) * smoothstep(0.3, 0.0, length(f - off)) * (0.35 + 0.65 * h31(i + 7.0));
    return vec3(s) * mix(vec3(0.7, 0.8, 1.0), vec3(1.0, 0.9, 0.75), h31(i + 5.0));
}

vec3 moonSurf(vec3 l) {
    float m = fbm(l * 3.0);
    float maria = smoothstep(0.50, 0.62, fbm(l * 1.6 + vec3(4.0)));
    maria *= smoothstep(-0.9, 0.2, l.z);
    vec3 base = mix(vec3(0.66, 0.64, 0.60), vec3(0.28, 0.29, 0.33), maria);
    float cr = fbm(l * 9.0);
    return base * (0.72 + 0.5 * m) * (0.85 + 0.3 * cr);
}

vec3 moonPos() {
    float sg = pc.d.w, ph = pc.c.w;
    vec3 e1 = vec3(-cos(sg), 0.0, -sin(sg));
    vec3 e2 = vec3(-sin(sg), 0.0, cos(sg));
    float th = sg + 2.0 * PI * ph;
    float c = cos(th), s = sin(th);
    vec3 p = ORB * (c * e1 + s * cos(INC) * e2);
    p.y += ORB * s * sin(INC);
    return p;
}

vec2 earthShade(vec3 p) {
    float d = p.x;
    if (d <= 0.0) return vec2(1.0, 0.0);
    float perp = length(p.yz);
    float ru = ER * (1.0 - d / LE), rp = ER * (1.0 + d / LE);
    if (perp >= rp) return vec2(1.0, 0.0);
    float t = clamp((rp - perp) / (rp - max(ru, 0.0)), 0.0, 1.0);
    float pen = mix(1.0, 0.4, t * t * (3.0 - 2.0 * t));
    float um = ru > 0.0 ? 1.0 - smoothstep(ru - 0.06, ru + 0.06, perp) : 0.0;
    return vec2(mix(pen, 0.0, um), um);
}

float moonShade(vec3 p, vec3 M) {
    vec3 q = p - M;
    float d = q.x;
    if (d <= 0.0) return 1.0;
    float perp = length(q.yz);
    float ru = MR * (1.0 - d / LM), rp = MR * (1.0 + d / LM);
    if (perp >= rp) return 1.0;
    float t = clamp((rp - perp) / (rp - max(ru, 0.0)), 0.0, 1.0);
    float pen = mix(1.0, 0.4, t * t * (3.0 - 2.0 * t));
    float um = ru > 0.0 ? 1.0 - smoothstep(ru - 0.02, ru + 0.02, perp) : 0.0;
    return mix(pen, 0.0, um);
}

vec3 earthBody(vec3 n) {
    vec3 A = vec3(sin(TILT), cos(TILT), 0.0);
    vec3 E1 = vec3(cos(TILT), -sin(TILT), 0.0);
    float x = dot(n, E1), y = dot(n, A), z = n.z;
    float th = pc.a.z * 0.12;
    return vec3(x * cos(th) - z * sin(th), y, x * sin(th) + z * cos(th));
}

vec3 earthColor(vec3 p, vec3 rd, vec3 M) {
    vec3 n = p;
    vec3 b = earthBody(n);
    float land = fbm(b * 2.2 + 3.0);
    float lm = smoothstep(0.52, 0.56, land);
    vec3 ocean = mix(vec3(0.015, 0.09, 0.32), vec3(0.04, 0.26, 0.5), smoothstep(0.35, 0.52, land));
    float veg = fbm(b * 5.0 + 11.0);
    vec3 ground = mix(vec3(0.10, 0.34, 0.10), vec3(0.50, 0.42, 0.24), smoothstep(0.4, 0.7, veg));
    ground = mix(ground, vec3(0.85, 0.85, 0.8), smoothstep(0.84, 0.92, abs(b.y)));
    vec3 surf = mix(ocean, ground, lm);
    float cl = smoothstep(0.56, 0.78, fbm(b * 3.5 + vec3(pc.a.z * 0.02)));
    surf = mix(surf, vec3(0.95), cl * 0.75);
    float ndl = dot(n, TOSUN);
    float day = smoothstep(-0.05, 0.28, ndl);
    float sh = moonShade(p, M);
    vec3 col = surf * (day * 1.15 * mix(0.25, 1.0, sh) + 0.02);
    float oc = (1.0 - lm) * (1.0 - cl);
    vec3 hv = normalize(TOSUN - rd);
    col += vec3(1.0, 0.95, 0.85) * pow(max(dot(n, hv), 0.0), 90.0) * oc * day * 0.9 * sh;
    float fres = pow(1.0 - max(dot(n, -rd), 0.0), 3.0);
    col += vec3(0.25, 0.5, 1.0) * fres * (0.1 + 0.9 * day) * 0.8;
    return col;
}

vec3 moonColorSpace(vec3 p, vec3 M) {
    vec3 mn = (p - M) / MR;
    vec3 l = vec3(mn.x, mn.y, -mn.z);
    float ndl = dot(mn, TOSUN);
    float lit = smoothstep(-0.01, 0.08, ndl);
    vec2 es = earthShade(p);
    vec3 surf = moonSurf(l);
    vec3 col = surf * (0.035 + 1.15 * lit * es.x);
    col += surf * vec3(0.9, 0.2, 0.06) * es.y * lit * 0.55;
    return col;
}

vec2 boxHit(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax) {
    vec3 inv = 1.0 / (rd + vec3(1e-6));
    vec3 t0 = (bmin - ro) * inv, t1 = (bmax - ro) * inv;
    vec3 tm = min(t0, t1), tM = max(t0, t1);
    return vec2(max(max(tm.x, tm.y), tm.z), min(min(tM.x, tM.y), tM.z));
}

vec3 spaceView(vec2 q) {
    vec3 ro = pc.b.xyz;
    vec3 rd = normalize(pc.c.xyz + pc.d.xyz * (q.x * pc.a.y * pc.b.w) + pc.e.xyz * (-q.y * pc.b.w));
    vec3 M = moonPos();
    vec3 SC = vec3(-SD, 0.0, 0.0);
    float tE = rsph(ro, rd, vec3(0.0), ER);
    float tM = rsph(ro, rd, M, MR);
    float tS = rsph(ro, rd, SC, SR);
    float tO = 1e9;
    int which = 0;
    if (tE > 0.0) { tO = tE; which = 1; }
    if (tM > 0.0 && tM < tO) { tO = tM; which = 2; }
    if (tS > 0.0 && tS < tO) { tO = tS; which = 3; }

    vec3 col = vec3(0.0015, 0.002, 0.004) + stars(rd, 150.0, 0.965);
    if (which == 1) col = earthColor(ro + rd * tE, rd, M);
    else if (which == 2) col = moonColorSpace(ro + rd * tM, M);
    else if (which == 3) {
        vec3 n = normalize(ro + rd * tS - SC);
        float mu = max(dot(n, -rd), 0.0);
        col = vec3(1.0, 0.72, 0.32) * (0.9 + 1.3 * pow(mu, 0.8));
    }

    float bd = dot(SC - ro, rd);
    float dist = length(SC - ro - rd * bd);
    if (bd > 0.0 && which != 3) {
        col += vec3(1.0, 0.75, 0.35) * 0.3 / (1.0 + pow(dist / SR, 2.0) * 1.6);
        col += vec3(1.0, 0.8, 0.5) * 0.12 * exp(-max(dist - SR, 0.0) * 0.6);
    }

    vec2 bx = boxHit(ro, rd, vec3(-17.5, -3.6, -17.0), vec3(15.5, 3.6, 17.0));
    float t0 = max(bx.x, 0.0), t1 = min(bx.y, tO);
    if (t1 > t0) {
        const int N = 72;
        float dt = (t1 - t0) / float(N);
        float jit = h31(vec3(gl_FragCoord.xy, 3.7));
        vec3 fog = vec3(0.0);
        for (int i = 0; i < N; i++) {
            vec3 p = ro + rd * (t0 + (float(i) + jit) * dt);
            float w = exp(-pow(p.y / 1.5, 2.0));
            float rad = smoothstep(17.0, 8.0, length(vec2(p.x + 1.5, p.z)));
            float lit = earthShade(p).x * moonShade(p, M);
            float tint = smoothstep(-17.0, 6.0, p.x);
            fog += mix(vec3(1.0, 0.8, 0.45), vec3(0.8, 0.85, 1.0), tint) * (w * rad * lit * dt);
        }
        col += fog * 0.085;
    }
    return col;
}

float limbN(float am) {
    return 0.5 * sin(7.0 * am + 1.0) + 0.35 * sin(13.0 * am + 4.0) + 0.25 * sin(29.0 * am + 2.0) + 0.15 * sin(53.0 * am + 0.7);
}

float coronaI(vec2 p, float rsun) {
    float len = length(p);
    float r = len / rsun;
    if (r < 1.0) return 0.0;
    vec2 u = p / max(len, 1e-5);
    float eq = abs(u.x);
    float L = 1.0 + 2.4 * pow(eq, 3.0);
    float st = clamp(fbm4(vec3(u * 2.6 + 3.0, 1.7)) * 1.9 - 0.25, 0.0, 1.5);
    float base = 1.5 * exp(-(r - 1.0) * 3.4);
    float stream = (0.3 + 1.7 * st * st) * exp(-(r - 1.0) / (0.5 + 0.9 * L * (0.4 + st)));
    float fine = 0.5 + 0.9 * fbm4(vec3(u * 8.0, r * 1.6));
    float plume = 0.5 * pow(abs(u.y), 5.0) * exp(-(r - 1.0) * 4.5);
    return (base + stream * fine + plume) * smoothstep(3.4, 1.3, r) * 0.9;
}

vec3 closeup(vec2 p) {
    float rsun = pc.f.z, rm = pc.f.w;
    vec2 mc = pc.f.xy;
    float f = pc.g.y;
    float px = pc.g.x * 0.0028;
    float r = length(p);
    vec2 dm = p - mc;
    float rmm = length(dm);
    float edge = rm + 0.0055 * limbN(atan(dm.y, dm.x));
    float moonMask = 1.0 - smoothstep(edge - px, edge + px, rmm);
    float sunMask = 1.0 - smoothstep(rsun - px, rsun + px, r);

    float skyB = 0.97 * pow(f, 0.55);
    vec3 sky = mix(vec3(0.002, 0.004, 0.016), vec3(0.30, 0.52, 0.95), skyB);
    float tw = (1.0 - smoothstep(0.0, 0.3, f)) * exp(-r * 0.8);
    sky += vec3(0.35, 0.14, 0.06) * tw * 0.04;
    sky += vec3(1.0, 0.95, 0.85) * f * (0.9 * exp(-r / 0.30) + 0.25 * exp(-r / 1.4));

    float mu = sqrt(max(0.0, 1.0 - pow(r / rsun, 2.0)));
    float li = 1.0 - 0.62 * (1.0 - mu);
    float gran = 1.0 + 0.05 * (vn(vec3(p * 55.0, pc.a.z * 0.3)) - 0.5);
    vec3 sunCol = vec3(1.0, 0.5 + 0.38 * mu, 0.2 + 0.42 * mu) * 3.6 * li * gran;

    float vis = smoothstep(0.06, 0.0, f);
    float cor = vis > 0.001 ? coronaI(p, rsun) : 0.0;
    vec3 col = sky;
    col += vec3(1.0, 0.96, 0.92) * cor * vis * 0.65;
    col += vec3(1.0, 0.25, 0.3) * exp(-max(r / rsun - 1.0, 0.0) * 70.0) * step(rsun, r) * vis * 0.7;
    col = mix(col, sunCol, sunMask);
    col = mix(col, vec3(0.003, 0.004, 0.008) + sky * 0.02, moonMask);

    if (f < 0.05) {
        float acc = 0.0, wide = 0.0;
        for (int i = 0; i < 48; i++) {
            float a = 2.0 * PI * float(i) / 48.0;
            vec2 qq = 0.994 * rsun * vec2(cos(a), sin(a));
            vec2 d2 = qq - mc;
            float ed = rm + 0.0055 * limbN(atan(d2.y, d2.x));
            if (length(d2) > ed) {
                float dd = length(p - qq);
                acc += exp(-dd * dd / (2.0 * 0.009 * 0.009));
                wide += exp(-dd / 0.12);
            }
        }
        col += vec3(1.0, 0.95, 0.85) * (acc * 3.2 + wide * 0.035);
    }
    return 1.0 - exp(-col * 1.15);
}

vec3 allsky(vec2 p) {
    float r = length(p);
    float f = pc.g.y;
    float skyB = 0.03 + 0.97 * pow(f, 0.55);
    float depth = 1.0 - smoothstep(0.0, 0.35, f);
    float zd = r * 1.5708;
    float az = atan(p.x, p.y);
    vec3 dir = vec3(sin(zd) * p.x / max(r, 1e-5), cos(zd), sin(zd) * p.y / max(r, 1e-5));
    vec3 sunDir = vec3(0.0, cos(0.698), sin(0.698));
    vec3 dayCol = mix(vec3(0.62, 0.76, 0.95), vec3(0.10, 0.26, 0.68), pow(max(dir.y, 0.0), 0.55));
    vec3 nightCol = mix(vec3(0.02, 0.03, 0.07), vec3(0.003, 0.005, 0.02), pow(max(dir.y, 0.0), 0.5));
    vec3 col = mix(nightCol, dayCol, skyB);
    float ang = acos(clamp(dot(dir, sunDir), -1.0, 1.0));
    col += vec3(1.0, 0.95, 0.85) * f * 0.5 * exp(-ang * 7.0);
    float band = pow(clamp((zd - 0.95) / 0.62, 0.0, 1.0), 2.0);
    float mod1 = 0.75 + 0.45 * vn(vec3(cos(az) * 2.0, sin(az) * 2.0, 1.3));
    vec3 gcol = mix(vec3(1.0, 0.42, 0.10), vec3(0.95, 0.22, 0.38), 0.5 + 0.5 * sin(az * 1.0 + 0.6));
    col += gcol * depth * band * mod1 * 1.5;
    float starVis = smoothstep(0.30, 0.04, skyB);
    col += stars(dir, 28.0, 0.93) * starVis * 1.2;

    vec2 sunP = vec2(0.0, 0.444);
    float ds = length(p - sunP);
    float vis = smoothstep(0.06, 0.0, f);
    col += vec3(1.0, 0.95, 0.88) * vis * 0.55 * exp(-max(ds - 0.012, 0.0) * 50.0) * step(0.012, ds);
    float disc = 1.0 - smoothstep(0.011, 0.015, ds);
    col = mix(col, vec3(1.0, 0.96, 0.88) * (0.02 + f * 5.0), disc);

    vec3 pl[3] = vec3[3](vec3(0.5778, 0.5585, 1.0), vec3(0.3778, -2.0944, 1.0), vec3(0.6889, -0.7, 0.7));
    vec3 pc3[3] = vec3[3](vec3(1.0, 0.97, 0.85), vec3(1.0, 0.93, 0.78), vec3(0.9, 0.85, 0.8));
    float pvis = smoothstep(0.34, 0.07, skyB);
    for (int i = 0; i < 3; i++) {
        vec2 pp = pl[i].x * vec2(sin(pl[i].y), cos(pl[i].y));
        float dd = length(p - pp);
        col += pc3[i] * pl[i].z * pvis * (1.8 * exp(-dd * dd / 0.00018) + 0.25 * exp(-dd * 40.0));
    }

    float hor = 1.5708 - 0.05 * (0.4 + vn(vec3(az * 3.0, 0.5, 2.0)) + 0.5 * vn(vec3(az * 11.0, 1.5, 4.0)));
    float ground = smoothstep(hor - 0.004, hor + 0.004, zd);
    vec3 gcol2 = vec3(0.012, 0.014, 0.016) + col * 0.05;
    col = mix(col, gcol2, ground);
    float rim = smoothstep(1.0, 0.99, r);
    col *= rim;
    return 1.0 - exp(-col * 1.1);
}

vec3 groundView(vec2 q) {
    vec2 p = vec2(q.x * pc.a.y, -q.y);
    vec2 ic = vec2(pc.a.y - 0.52, 0.34);
    float Ri = 0.42;
    float swap = pc.a.w;
    vec2 li = (vec2(q.x * pc.a.y, q.y) - ic) / Ri;
    float lr = length(li);
    vec3 col;
    if (swap < 0.5) col = closeup(p * pc.g.x);
    else col = allsky(p / 0.98);
    if (lr < 1.0) {
        vec2 lp = vec2(li.x, -li.y);
        if (swap < 0.5) col = allsky(lp);
        else col = closeup(lp * pc.g.x);
    }
    col = mix(col, vec3(0.55, 0.62, 0.8), smoothstep(0.012, 0.0, abs(lr - 1.0)) * 0.9);
    return col;
}

vec3 lunarView(vec2 q) {
    vec2 p = vec2(q.x * pc.a.y, -q.y);
    float Rm = 0.40;
    vec2 m = p / Rm;
    float r = length(m);
    float Ru = pc.h.x, Rp = pc.h.y;
    vec2 sh = pc.g.zw;
    float dc = length(sh);
    float lightMean = mix(1.0, 0.16, smoothstep(Rp + 1.0, Ru - 1.0, dc));
    vec3 col = vec3(0.002, 0.003, 0.008) + stars(normalize(vec3(p, 1.0)), 170.0, 0.975) * mix(1.1, 0.5, lightMean * lightMean);
    if (r < 1.0) {
        vec3 n = vec3(m.x, m.y, sqrt(max(0.0, 1.0 - r * r)));
        vec3 surf = moonSurf(vec3(n.x, n.y, -n.z + 0.0) * 1.0);
        float ds = length(m - sh);
        float pen = clamp((Rp - ds) / (Rp - Ru), 0.0, 1.0);
        pen = pen * pen * (3.0 - 2.0 * pen);
        float penFac = mix(1.0, 0.42, pen);
        vec3 lit = (surf * 1.55 - 0.18) * (0.9 + 0.2 * n.z) * penFac;
        float um = 1.0 - smoothstep(Ru - 0.14, Ru + 0.10, ds);
        float tU = clamp(ds / Ru, 0.0, 1.0);
        float bright = mix(0.3, 1.0, smoothstep(0.0, 1.0, tU * tU));
        vec3 ucol = (0.04 + surf * 1.7) * vec3(0.68, 0.11, 0.03) * bright * 0.72;
        ucol += vec3(0.04, 0.22, 0.26) * smoothstep(0.78, 1.0, tU) * (1.0 - smoothstep(0.0, 0.14, Ru - ds + 0.14)) * 0.0;
        float fringe = smoothstep(Ru - 0.34, Ru - 0.02, ds) * (1.0 - smoothstep(Ru - 0.02, Ru + 0.12, ds));
        ucol += vec3(0.05, 0.30, 0.34) * fringe * surf * 0.8;
        vec3 mc = mix(lit, ucol, um);
        mc *= 0.85 + 0.15 * pow(n.z, 0.3);
        col = mc;
    } else {
        col += vec3(0.7, 0.75, 0.9) * exp(-(r - 1.0) * 5.0) * 0.07 * lightMean;
    }
    return 1.0 - exp(-col * 1.3);
}

void main() {
    vec2 q = inNdc;
    int view = int(pc.a.x + 0.5);
    vec3 col;
    if (view == 0) col = spaceView(q);
    else if (view == 1) col = groundView(q);
    else col = lunarView(q);
    col = clamp(col, 0.0, 1.5);
    outColor = vec4(col, 1.0);
}
