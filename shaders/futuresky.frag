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
} p;

const float PI = 3.14159265;
const float EL0 = 40.0;
const float TANH = 0.8391;

float h21(vec2 q) {
    q = fract(q * vec2(123.34, 456.21));
    q += dot(q, q + 45.32);
    return fract(q.x * q.y);
}

float h31(vec3 q) {
    q = fract(q * 0.3183099 + 0.1);
    q *= 17.0;
    return fract(q.x * q.y * q.z * (q.x + q.y + q.z));
}

float vn(vec3 x) {
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(h31(i), h31(i + vec3(1, 0, 0)), f.x), mix(h31(i + vec3(0, 1, 0)), h31(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(h31(i + vec3(0, 0, 1)), h31(i + vec3(1, 0, 1)), f.x), mix(h31(i + vec3(0, 1, 1)), h31(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

float fbm(vec3 x) {
    float s = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; i++) {
        s += a * vn(x);
        x = x * 2.03 + 11.7;
        a *= 0.5;
    }
    return s;
}

vec3 azel(float az, float el) {
    float A = radians(az);
    float E = radians(el);
    return vec3(sin(A) * cos(E), sin(E), cos(A) * cos(E));
}

float angd(float x) {
    return abs(mod(x + PI, 2.0 * PI) - PI);
}

float dots(vec2 p, float seed) {
    vec2 id = floor(p);
    vec2 fr = fract(p) - 0.5;
    float h = h21(id + seed);
    if (h < 0.55) return 0.0;
    vec2 o = (vec2(h21(id + seed + 3.1), h21(id + seed + 7.7)) - 0.5) * 0.5;
    float dd = length(fr - o);
    return exp(-dd * dd / 0.014) * h21(id + seed + 9.0);
}

vec4 galaxy(vec3 d, float az, float el, float Rdeg, float tilt, float q, float disc, float ell, float tide, float bright, float seed, float burst) {
    if (bright <= 0.001) return vec4(0.0);
    vec3 cc = azel(az, el);
    float cd = clamp(dot(d, cc), -1.0, 1.0);
    float th = acos(cd);
    float R = radians(Rdeg);
    if (th > R * 3.6 || cd < 0.0) return vec4(0.0);
    vec3 e1 = normalize(cross(vec3(0, 1, 0), cc));
    vec3 e2 = cross(cc, e1);
    vec3 pv = d - cd * cc;
    vec2 uv = vec2(dot(pv, e1), dot(pv, e2));
    float ul = length(uv);
    vec2 xy = ul > 1e-6 ? uv / ul * th : vec2(0.0);
    xy /= R;
    float ta = radians(tilt);
    vec2 rp = vec2(cos(ta) * xy.x + sin(ta) * xy.y, -sin(ta) * xy.x + cos(ta) * xy.y);
    rp.x += tide * 0.16 * sin(rp.y * 3.0 + seed);
    rp.y += tide * 0.10 * sin(rp.x * 3.5 + seed * 2.0);
    vec2 fo = vec2(rp.x, rp.y / q);
    float r = length(fo);
    float ph = atan(fo.y, fo.x);
    float qt = mix(q, 1.0, 0.65);
    vec2 ft = vec2(rp.x, rp.y / qt);
    float rt = length(ft);
    float pt = atan(ft.y, ft.x);

    float edge = smoothstep(1.05, 0.7, r);
    float arms = 0.5 + 0.5 * cos(2.0 * (ph - 3.4 * log(r + 0.06)) + seed + tide * 1.5);
    float armW = mix(1.0, pow(arms, 1.4), smoothstep(0.06, 0.3, r));
    float body = exp(-r * 3.0) * (0.3 + 1.0 * armW) * smoothstep(0.0, 0.05, r) * edge;
    float bulge = 1.3 * exp(-r * 15.0) + 0.5 * exp(-r * 5.0);
    float n1 = fbm(vec3(fo * 6.0, seed));
    float laneW = 0.03 + 0.16 * q;
    float lane = exp(-pow((rp.y - 0.10 * q) / laneW, 2.0)) * smoothstep(0.04, 0.2, r);
    float dust = 1.0 - 0.8 * lane * (0.45 + 0.8 * n1) - 0.4 * (1.0 - arms) * smoothstep(0.1, 0.4, r) * (0.3 + 0.7 * q);
    dust = clamp(dust, 0.05, 1.0);
    vec3 cold = vec3(0.62, 0.78, 1.0);
    vec3 warm = vec3(1.0, 0.82, 0.55);
    vec3 col = (body * mix(cold, vec3(0.9, 0.92, 1.0), exp(-r * 4.0)) + bulge * warm) * dust * disc;

    float kn = dots(fo * 30.0, seed * 3.0 + 1.0);
    float kn2 = fbm(vec3(fo * 5.0, seed + 7.0));
    float knots = 1.6 * kn * armW * exp(-r * 1.4) * smoothstep(0.08, 0.2, r) * edge;
    vec3 kc = mix(vec3(0.45, 0.65, 1.0), vec3(1.0, 0.35, 0.6), smoothstep(0.35, 0.65, kn2));
    col += knots * kc * (0.15 + 2.2 * burst) * disc * dust;

    if (tide > 0.0) {
        float tl = 0.0;
        for (int s = -1; s <= 1; s += 2) {
            float dd = angd(pt - (seed * 0.7 + float(s) * (0.7 + 0.95 * (rt - 0.6)) + (s > 0 ? 0.0 : 3.14159)));
            tl += exp(-pow(dd * (rt + 0.3) / 0.3, 2.0));
        }
        float tn = 0.5 + 0.9 * fbm(vec3(ft * 7.0, seed));
        float tm = smoothstep(0.55, 1.0, rt) * smoothstep(3.5, 1.4, rt);
        vec3 tc = mix(vec3(0.7, 0.8, 1.0), vec3(1.0, 0.6, 0.8), burst * 0.6);
        col += tide * tl * tm * tn * 0.7 * tc * exp(-rt * 0.6);
        float tk = 1.6 * dots(ft * 24.0, seed + 3.0);
        col += tide * tl * tm * tk * 1.6 * kc * (0.4 + burst);
    }

    float re = length(vec2(rp.x, rp.y / mix(q, 0.8, 0.7))) / 0.8;
    float ei = exp(-7.67 * (pow(re + 0.002, 0.25) - 1.0)) * 0.16 * smoothstep(1.6, 0.6, re);
    col += ell * ei * vec3(1.0, 0.8, 0.55) * (0.85 + 0.3 * fbm(vec3(fo * 8.0, seed)));
    col += ell * 0.9 * exp(-re * 12.0) * vec3(1.0, 0.85, 0.6);

    float dens = dot(col, vec3(0.3, 0.5, 0.2));
    return vec4(col * bright, dens * bright);
}

vec3 starfield(vec3 d, float bandD, float pxn) {
    vec2 uv = inNdc / pxn;
    vec3 acc = vec3(0.0);
    for (int L = 0; L < 3; L++) {
        float cell = (6.0 + 5.0 * float(L)) * (2.0 / pxn / 720.0);
        vec2 g = uv / cell;
        vec2 id = floor(g);
        vec2 fr = fract(g) - 0.5;
        float hh = h21(id + float(L) * 17.3);
        float thr = 0.94 - 0.02 * float(L) - 0.05 * bandD;
        if (hh > thr) {
            vec2 off = (vec2(h21(id + 3.1), h21(id + 7.7)) - 0.5) * 0.6;
            float dist = length((fr - off) * cell) * (pxn * 720.0 / 2.0);
            float mag = pow(h21(id + 11.0), 3.0);
            float rad = 0.5 + 0.45 * mag;
            float tw = 0.85 + 0.15 * sin(p.a.z * (2.0 + 3.0 * h21(id)) + hh * 40.0);
            float s = exp(-dist * dist / (rad * rad)) * (0.2 + 1.3 * mag) * tw;
            vec3 sc = mix(vec3(1.0, 0.8, 0.6), vec3(0.7, 0.8, 1.0), h21(id + 5.0));
            acc += s * sc;
        }
    }
    return acc;
}

void main() {
    float t = p.a.x;
    float aspect = p.a.y;
    float sunK = p.a.w;
    float band = p.b.x;
    float warp = p.b.y;
    float burst = p.b.z;

    vec2 sp = vec2(inNdc.x * aspect, -inNdc.y);
    float elc = radians(EL0);
    vec3 fw = vec3(0.0, sin(elc), cos(elc));
    vec3 rt = vec3(1.0, 0.0, 0.0);
    vec3 up = vec3(0.0, cos(elc), -sin(elc));
    vec3 d = normalize(fw + sp.x * TANH * rt + sp.y * TANH * up);
    float pxn = abs(dFdy(inNdc.y));
    float elev = asin(clamp(d.y, -1.0, 1.0));

    vec3 gc = azel(28.0, 24.0);
    vec3 pp = azel(-45.0, 60.0);
    vec3 nB = normalize(cross(gc, pp));
    vec3 eB = gc;
    vec3 fB = cross(nB, gc);
    float sb = dot(d, nB);
    vec3 inpl = d - sb * nB;
    float l = atan(dot(inpl, fB), dot(inpl, eB));
    float nz = fbm(d * 3.0 + 4.0);
    float bb = asin(clamp(sb, -1.0, 1.0));
    bb += warp * (0.20 * sin(l * 2.0 + 0.6) + 0.30 * (nz - 0.5));
    float prof = exp(-pow(abs(bb) / 0.19, 1.3));
    float coreW = 0.45 + 1.1 * exp(-l * l / 0.6);
    float bulgeB = exp(-(l * l + bb * bb * 2.5) / 0.14);
    float cl = fbm(d * 9.0 + 2.0);
    float cl2 = vn(d * 26.0);
    float rift = 1.0 - 0.75 * exp(-pow(bb / 0.045, 2.0)) * smoothstep(0.25, 0.7, fbm(d * 11.0 + 9.0));
    float mwi = (prof * coreW * (0.35 + 0.9 * cl) + 0.9 * bulgeB) * rift * (0.8 + 0.4 * cl2) * band;
    vec3 mwc = mix(vec3(0.55, 0.65, 0.9), vec3(1.0, 0.85, 0.62), clamp(coreW * 0.7 + bulgeB, 0.0, 1.0));
    vec3 mw = mwi * mwc * 0.30;

    vec4 gA = galaxy(d, p.c.x, p.c.y, p.c.z, p.c.w, p.d.x, p.d.y, p.d.z, p.d.w, p.g.x, 1.3, burst);
    vec4 gB = galaxy(d, p.e.x, p.e.y, p.e.z, p.e.w, p.f.x, p.f.y, p.f.z, p.f.w, p.g.y, 4.1, burst);
    vec3 gal = gA.rgb + gB.rgb;
    float dens = gA.a + gB.a;

    vec3 neb = vec3(0.0);
    if (burst > 0.01) {
        float n = fbm(d * 4.5 + 3.0);
        float n2 = fbm(d * 8.0 - 1.0);
        float n3 = fbm(d * 15.0 + 5.0);
        float m = clamp(dens * 1.8 + 0.25 * prof * band, 0.0, 1.0);
        neb = (vec3(1.0, 0.28, 0.5) * pow(n, 2.4) * 2.2 + vec3(0.3, 0.5, 1.0) * pow(n2, 2.4) * 2.0) * burst * m + vec3(1.0, 0.6, 0.8) * pow(n3, 4.0) * burst * m * 0.8;
    }

    float light = dot(gal + mw, vec3(0.33));
    vec3 stars = starfield(d, clamp(mwi, 0.0, 1.0), pxn) * smoothstep(0.0, 0.14, elev) * (1.0 - clamp(light * 1.2, 0.0, 0.85));

    vec3 base = vec3(0.004, 0.007, 0.017);
    float hz = exp(-max(elev, 0.0) / 0.22);
    vec3 glow = mix(vec3(0.015, 0.03, 0.06), vec3(0.09, 0.04, 0.012), sunK) * hz * (0.35 + 1.0 * sunK);
    vec3 col = base + glow + mw + gal + neb + stars;

    vec3 tint = mix(vec3(1.0), vec3(1.12, 0.95, 0.74), sunK * 0.85);
    col *= tint;
    col = 1.0 - exp(-col * 1.5);

    float scrim = 1.0 - 0.7 * smoothstep(0.0, 0.85, inNdc.y);
    scrim *= 1.0 - 0.3 * smoothstep(-0.5, -1.0, inNdc.y);
    col *= scrim;

    float x = inNdc.x * aspect;
    float hill = 0.040 + 0.030 * sin(x * 1.9 + 1.0) + 0.020 * sin(x * 4.7 + 0.3) + 0.008 * sin(x * 11.0);
    hill += 0.03 * smoothstep(0.6, 1.9, abs(x));
    float tw = 0.075;
    float ci = floor(x / tw);
    float tx = (ci + 0.5) * tw + (h21(vec2(ci, 1.0)) - 0.5) * tw * 0.7;
    float tr = h21(vec2(ci, 2.0));
    float th2 = (0.025 + 0.07 * tr) * smoothstep(0.35, 1.3, abs(x)) * step(0.35, h21(vec2(ci, 3.0)));
    float tree = th2 * (1.0 - abs(x - tx) / (0.55 * th2 + 0.004));
    float gy = 1.0 - 2.0 * (hill + max(tree, 0.0));
    float yb = inNdc.y;
    if (yb > gy) {
        float rim = 1.0 - smoothstep(gy, gy + 0.02, yb);
        col = mix(vec3(0.0015, 0.002, 0.003), col * 0.18 + vec3(0.01, 0.008, 0.006) * (0.4 + sunK), rim * 0.6);
    }
    outColor = vec4(col, 1.0);
}
