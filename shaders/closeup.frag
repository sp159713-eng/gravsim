#version 450

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PC {
    vec4 a;
    vec4 b;
    vec4 d[6];
} pc;

float hash21(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i), hash21(i + vec2(1, 0)), f.x), mix(hash21(i + vec2(0, 1)), hash21(i + vec2(1, 1)), f.x), f.y);
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

mat2 rot(float a) {
    float c = cos(a), s = sin(a);
    return mat2(c, -s, s, c);
}

float pxPerUv() {
    return pc.b.x * 0.5 / pc.b.y;
}

vec3 starLayer(vec2 p, float scale, float density, float gain) {
    vec2 g = p * scale;
    vec2 id = floor(g);
    vec2 f = fract(g) - 0.5;
    if (hash21(id) > density) return vec3(0.0);
    vec2 off = (vec2(hash21(id + 3.7), hash21(id + 9.2)) - 0.5) * 0.5;
    float dpx = length(f - off) / scale * pxPerUv();
    float br = hash21(id + 5.1);
    br = br * br * br;
    vec3 tint = mix(vec3(1.0, 0.78, 0.55), vec3(0.6, 0.75, 1.0), hash21(id + 9.1));
    float core = exp(-dpx * dpx / (1.1 + 2.5 * br));
    float halo = br * 0.5 * exp(-dpx * 0.45);
    return tint * (core * (0.35 + br) + halo) * gain;
}

vec3 glowStar(vec2 uv, vec2 c, float rpx, vec3 col) {
    float d = length(uv - c) * pxPerUv();
    float core = exp(-d * d / (rpx * rpx));
    float halo = 0.4 * rpx / (d + rpx) * exp(-d / (7.0 * rpx));
    return col * (core * 1.6 + halo);
}

vec3 emission(vec2 uv, float carina) {
    uv += carina * vec2(7.3, 3.1);
    vec2 q = vec2(fbm(uv * 1.3 + 1.7), fbm(uv * 1.3 + 8.3));
    float warp = fbm(uv * 1.5 + 2.2 * q);
    vec2 cen = vec2(0.15, -0.1) + carina * vec2(7.3, 3.1);
    float r = length(uv - cen);
    float core = exp(-r * r * 0.9);
    float shell = smoothstep(1.9, 0.2, r);
    float gas = warp * warp * 2.4 * (0.2 + shell);
    vec3 ha = vec3(1.0, 0.15, 0.12);
    vec3 oiii = vec3(0.1, 0.85, 0.75);
    vec3 hb = vec3(0.25, 0.35, 1.0);
    float oMix = smoothstep(0.3, 0.95, core * 1.3 + 0.35 * fbm(uv * 3.0 + q));
    vec3 col = mix(ha, oiii, oMix) * gas;
    col += hb * 0.4 * pow(fbm(uv * 2.0 + q * 2.0 + 4.0), 3.0) * shell;
    float ridge = 1.0 - abs(2.0 * fbm(uv * 2.4 + 3.0 * q) - 1.0);
    col += mix(ha, vec3(1.0, 0.65, 0.55), core) * pow(ridge, 6.0) * 1.0 * shell;
    float lane = smoothstep(0.5, 0.72, fbm(uv * 2.2 + q * 3.0 + 11.0));
    col *= 1.0 - (carina > 0.5 ? 0.92 : 0.78) * lane;
    col += vec3(1.0, 0.75, 0.6) * core * core * 0.8;
    if (carina > 0.5) col = col * vec3(1.05, 0.8, 0.75) + vec3(0.25, 0.1, 0.03) * lane * shell;
    float sd = 0.0;
    vec3 st = starLayer(uv, 9.0, 0.5, 1.2) + starLayer(uv + 3.1, 18.0, 0.4, 1.0);
    col += st * (0.4 + 2.0 * gas);
    vec2 tz = cen + vec2(0.0, 0.0);
    col += glowStar(uv, tz + vec2(0.05, 0.02), 4.0, vec3(0.7, 0.8, 1.0));
    col += glowStar(uv, tz + vec2(-0.05, 0.04), 3.2, vec3(0.8, 0.85, 1.0));
    col += glowStar(uv, tz + vec2(0.02, -0.06), 3.5, vec3(0.9, 0.9, 1.0));
    col += glowStar(uv, tz + vec2(-0.02, -0.01), 4.5, vec3(1.0, 0.95, 0.9));
    if (carina > 0.5) col += glowStar(uv, vec2(-0.4, 0.3) + vec2(7.3, 3.1), 6.0, vec3(1.0, 0.85, 0.6));
    return col;
}

float pillarDens(vec2 uv) {
    float d = 0.0;
    for (int k = 0; k < 3; k++) {
        float fk = float(k);
        float cx = k == 0 ? -0.62 : (k == 1 ? 0.1 : 0.78);
        float top = k == 0 ? 0.42 : (k == 1 ? 0.8 : 0.1);
        float w0 = k == 0 ? 0.17 : (k == 1 ? 0.2 : 0.13);
        float wob = 0.05 * sin(uv.y * 3.1 + fk * 1.7) + 0.08 * (fbm(vec2(uv.y * 2.2, fk * 4.0)) - 0.5);
        float xc = cx + wob + (0.14 * fk - 0.12) * uv.y;
        float w = w0 * (1.0 + 0.9 * smoothstep(top - 0.5, -1.3, uv.y)) + 0.05 * fbm(uv * 5.0 + fk);
        float body = smoothstep(0.03, -0.03, abs(uv.x - xc) - w);
        float dxn = (uv.x - xc) / w;
        float topK = top - 0.2 * dxn * dxn + 0.12 * (fbm(uv * 5.0 + fk) - 0.4);
        float cap = smoothstep(topK + 0.05, topK - 0.07, uv.y);
        d = max(d, body * cap);
    }
    return d;
}

vec3 pillars(vec2 uv) {
    vec2 q = vec2(fbm(uv * 1.6 + 2.0), fbm(uv * 1.6 + 6.0));
    float n = fbm(uv * 2.0 + 2.0 * q);
    float glow = smoothstep(-0.6, 0.9, uv.y) * 0.6 + 0.4;
    vec3 gas = mix(vec3(0.9, 0.35, 0.2), vec3(0.25, 0.9, 0.6), smoothstep(0.3, 0.8, fbm(uv * 1.2 + q))) * n * n * 2.2 * glow;
    gas += vec3(1.0, 0.7, 0.4) * pow(fbm(uv * 3.0 + q * 2.0), 4.0) * 1.2 * glow;
    float dens = pillarDens(uv);
    vec2 L = normalize(vec2(-0.5, 0.85));
    float lit = clamp(dens - pillarDens(uv + L * 0.045), 0.0, 1.0);
    float lit2 = clamp(dens - pillarDens(uv + L * 0.12), 0.0, 1.0);
    vec3 col = gas * (1.0 - 0.96 * dens);
    col += dens * vec3(0.05, 0.02, 0.015) * (0.5 + fbm(uv * 8.0));
    col += lit * vec3(1.0, 0.65, 0.35) * 1.8 + lit2 * vec3(0.9, 0.35, 0.2) * 0.6;
    float mist = smoothstep(0.5, 0.0, abs(uv.y - 0.55)) * fbm(uv * 4.0 + q * 3.0) * (1.0 - dens);
    col += vec3(1.0, 0.45, 0.25) * mist * 0.35;
    col += (starLayer(uv, 10.0, 0.5, 1.2) + starLayer(uv + 1.3, 20.0, 0.4, 1.0)) * (1.0 - dens) * 1.2;
    return col;
}

vec3 crab(vec2 uv) {
    vec2 e = rot(0.45) * uv * vec2(1.0, 1.3);
    float r = length(e);
    float env = smoothstep(1.05, 0.7, r + 0.1 * (fbm(uv * 3.0) - 0.5));
    vec2 w = vec2(fbm(uv * 3.0 + 1.0), fbm(uv * 3.0 + 7.7));
    float f1 = pow(1.0 - abs(2.0 * fbm(uv * 4.0 + 2.5 * w) - 1.0), 5.0);
    float f2 = pow(1.0 - abs(2.0 * fbm(uv * 7.5 - 1.7 * w) - 1.0), 7.0);
    float edge = 0.35 + 0.9 * smoothstep(0.15, 0.85, r);
    vec3 fc = mix(vec3(1.0, 0.32, 0.1), vec3(0.35, 1.0, 0.65), smoothstep(0.45, 0.75, fbm(uv * 2.0 + 3.0)) * 0.6);
    vec3 col = fc * (f1 * 1.1 + f2 * 0.9) * env * edge;
    col += vec3(0.3, 0.5, 1.0) * pow(fbm(uv * 2.2 + w), 2.0) * smoothstep(0.9, 0.0, r) * 1.3;
    float pulse = 0.65 + 0.35 * pow(sin(pc.a.y * 5.0), 2.0);
    vec2 p = uv;
    float ring = exp(-pow((length(vec2(p.x, p.y * 2.3)) - 0.17) / 0.018, 2.0));
    col += vec3(0.7, 0.85, 1.0) * ring * 0.8;
    float jet = exp(-pow(p.x * 9.0, 2.0)) * exp(-abs(p.y) * 3.0) * step(0.16, abs(p.y));
    col += vec3(0.6, 0.8, 1.0) * jet * 0.5;
    col += glowStar(uv, vec2(0.0), 4.5 * pulse, vec3(1.0, 1.0, 1.0));
    col += (starLayer(uv, 9.0, 0.5, 1.0) + starLayer(uv + 4.0, 20.0, 0.4, 0.9)) * (1.0 - 0.7 * env);
    return col;
}

vec3 ringNeb(vec2 uv, float helix) {
    vec2 e = rot(helix > 0.5 ? 0.9 : 0.5) * uv;
    e.y *= 1.25;
    float r = length(e);
    float ang = atan(e.y, e.x);
    float R0 = helix > 0.5 ? 0.6 : 0.5;
    float th = helix > 0.5 ? 0.16 : 0.11;
    float clump = 0.5 + 1.0 * fbm(vec2(ang * 2.2, r * 5.0) + vec2(cos(ang), sin(ang)) * 2.0);
    float ringV = exp(-pow((r - R0) / th, 2.0)) * clump;
    float t = clamp((r - (R0 - th)) / (2.0 * th), 0.0, 1.0);
    vec3 rc = mix(vec3(0.1, 0.85, 0.9), mix(vec3(0.5, 0.9, 0.4), vec3(1.0, 0.28, 0.18), smoothstep(0.35, 1.0, t)), smoothstep(0.0, 0.5, t));
    vec3 col = rc * ringV * 1.8;
    col += vec3(0.1, 0.45, 0.6) * smoothstep(R0, 0.0, r) * (0.2 + 0.2 * fbm(uv * 4.0));
    col += vec3(1.0, 0.25, 0.2) * exp(-pow((r - R0 - 0.2) / 0.22, 2.0)) * 0.22 * (0.4 + fbm(uv * 3.0 + 5.0));
    if (helix > 0.5) {
        float spokes = pow(fbm(vec2(ang * 16.0, r * 2.5)), 3.0);
        col += vec3(0.8, 0.45, 0.35) * spokes * exp(-pow((r - R0 - 0.05) / 0.25, 2.0)) * 1.2;
    }
    col += glowStar(uv, vec2(0.0), 3.6, vec3(0.8, 0.9, 1.0));
    col += (starLayer(uv, 9.0, 0.5, 1.0) + starLayer(uv + 4.0, 18.0, 0.4, 0.9));
    return col;
}

float sdCap(vec2 p, vec2 a, vec2 b, float r) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - r;
}

float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

float horseSd(vec2 p) {
    vec2 q = p - vec2(0.0, 0.16);
    float d = sdCap(p, vec2(0.42, -0.8), vec2(0.32, -0.1), 0.3);
    d = smin(d, sdCap(q, vec2(0.32, -0.1), vec2(0.22, 0.12), 0.27), 0.12);
    d = smin(d, sdCap(q, vec2(0.26, 0.28), vec2(0.0, 0.3), 0.2), 0.12);
    d = smin(d, sdCap(q, vec2(0.0, 0.26), vec2(-0.14, 0.14), 0.22), 0.1);
    d = smin(d, sdCap(q, vec2(-0.16, 0.08), vec2(-0.34, -0.14), 0.17), 0.08);
    d = smin(d, sdCap(q, vec2(-0.34, -0.14), vec2(-0.44, -0.34), 0.115), 0.06);
    d = smin(d, length(q - vec2(-0.46, -0.38)) - 0.1, 0.05);
    d = smin(d, sdCap(q, vec2(-0.03, 0.5), vec2(0.0, 0.56), 0.04), 0.06);
    d = smin(d, p.y + 0.56 + 0.07 * sin(p.x * 2.6 + 1.0) + 0.04 * sin(p.x * 7.0), 0.22);
    return d;
}

vec3 horsehead(vec2 uv) {
    vec2 p = uv + vec2(0.0, 0.05);
    float dd = horseSd(p) + (fbm(p * 7.0) - 0.5) * 0.06 + (fbm(p * 16.0) - 0.5) * 0.025;
    float dust = smoothstep(0.02, -0.02, dd);
    float fil = fbm(vec2(p.x * 2.0, p.y * 6.0) + 3.0);
    float fil2 = fbm(vec2(p.x * 5.0, p.y * 14.0) + 9.0);
    float bright = (0.35 + 0.9 * fil + 0.35 * fil2) * (0.55 + 0.7 * smoothstep(-0.9, 0.5, p.x + 0.2 * p.y));
    vec3 gas = mix(vec3(0.75, 0.03, 0.03), vec3(1.0, 0.22, 0.12), fil2) * bright * 0.8;
    float rim = exp(-max(dd, 0.0) * 9.0) * (1.0 - dust);
    gas += vec3(1.0, 0.45, 0.35) * rim * 0.9 * (0.5 + fil);
    gas += vec3(1.0, 0.2, 0.15) * 0.15 * fbm(p * 2.0);
    float halo = smoothstep(0.35, 0.0, dd) * (1.0 - dust);
    gas *= 1.0 - 0.45 * halo * fbm(p * 5.0);
    vec3 refl = vec3(0.15, 0.3, 0.9) * exp(-length(p - vec2(-1.0, -0.55)) * 1.7) * (0.5 + fbm(p * 4.0));
    vec3 col = (gas + refl) * (1.0 - 0.985 * dust);
    col += dust * vec3(0.03, 0.008, 0.008) * (0.3 + fbm(p * 9.0));
    col += (starLayer(uv, 9.0, 0.3, 1.3) + starLayer(uv + 2.2, 20.0, 0.25, 1.0) + starLayer(uv + 7.0, 40.0, 0.2, 0.8)) * (1.0 - dust);
    return col;
}

vec3 sgrBackdrop(vec2 uv, vec2 ndc) {
    vec3 col = vec3(0.004, 0.003, 0.007);
    col += vec3(0.035, 0.017, 0.009) * exp(-dot(uv, uv) * 0.25);
    col += vec3(0.02, 0.01, 0.006) * fbm(uv * 1.5) * exp(-dot(uv, uv) * 0.15);
    col += (starLayer(uv, 12.0, 0.12, 0.7) + starLayer(uv + 3.1, 26.0, 0.1, 0.55) + starLayer(uv + 8.2, 55.0, 0.08, 0.45)) * vec3(1.0, 0.85, 0.7);
    float aspect = pc.a.w;
    for (int i = 0; i < 6; i++) {
        vec4 s = pc.d[i];
        if (s.z <= 0.0) continue;
        float dpx = length((ndc - s.xy) * vec2(aspect, 1.0)) * pc.b.x * 0.5;
        vec3 c = vec3(1.0, 0.85, 0.35);
        if (i == 1) c = vec3(0.5, 0.85, 1.0);
        if (i == 2) c = vec3(1.0, 0.45, 0.75);
        if (i == 3) c = vec3(0.6, 1.0, 0.55);
        if (i == 4) c = vec3(1.0, 0.6, 0.3);
        if (i == 5) c = vec3(0.8, 0.6, 1.0);
        float core = exp(-dpx * dpx / (s.z * s.z));
        float halo = 0.7 * s.z / (dpx + s.z) * exp(-dpx / (5.0 * s.z));
        col += c * (core * 3.0 + halo);
    }
    return col;
}

vec3 binaryBackdrop(vec2 uv, vec2 ndc, float mode) {
    vec3 col = vec3(0.003, 0.004, 0.009);
    col += (starLayer(uv, 12.0, 0.12, 0.7) + starLayer(uv + 3.1, 28.0, 0.1, 0.5));
    float aspect = pc.a.w;
    float hpx = pc.b.x * 0.5;
    vec2 sp = pc.d[0].xy, bp = pc.d[2].xy;
    float rpx = pc.d[0].z;
    vec2 sPx = sp * vec2(aspect, 1.0) * hpx, bPx = bp * vec2(aspect, 1.0) * hpx, fPx = ndc * vec2(aspect, 1.0) * hpx;
    float stream = pc.d[1].w;
    if (stream > 0.0) {
        vec2 ax = bPx - sPx;
        float len = max(length(ax), 1.0);
        vec2 dir = ax / len, nrm = vec2(-dir.y, dir.x);
        vec2 rel = fPx - sPx;
        float t = dot(rel, dir) / len;
        float perp = dot(rel, nrm) - sin(3.14159 * clamp(t, 0.0, 1.0)) * len * 0.12;
        float wd = mix(0.45 * rpx, 7.0, clamp(t, 0.0, 1.0));
        float inside = smoothstep(0.0, 0.1, t) * smoothstep(1.05, 0.9, t);
        float turb = 0.4 + 0.9 * fbm(vec2(t * 9.0 - pc.a.y * 0.6, perp / max(wd, 1.0) * 1.2));
        float dens = exp(-pow(perp / wd, 2.0)) * inside * turb;
        vec3 sc = mix(pc.d[1].xyz, vec3(1.0, 0.65, 0.35), smoothstep(0.4, 1.0, t));
        col += sc * dens * (0.5 + 1.8 * t * t);
        col += pc.d[1].xyz * 0.12 * exp(-length(fPx - sPx) / (rpx * 2.5)) * 0.5;
    }
    float ds = length(fPx - sPx);
    if (ds < rpx) {
        float u = ds / rpx;
        float limb = 0.35 + 0.65 * sqrt(max(1.0 - u * u, 0.0));
        float gran = mode > 21.5 ? 0.85 + 0.3 * fbm(fPx / rpx * 5.0) : 1.0;
        col = pc.d[1].xyz * limb * gran * 0.95;
    } else {
        col += pc.d[1].xyz * (0.45 * exp(-(ds - rpx) / (0.35 * rpx)) + 0.12 * exp(-(ds - rpx) / (2.5 * rpx)));
    }
    return col;
}

void main() {
    float mode = pc.a.x;
    float aspect = pc.a.w;
    vec2 uv = vec2(inNdc.x * aspect, -inNdc.y) * pc.b.y + vec2(pc.b.z, pc.b.w);
    vec3 col;
    if (mode < 0.5) col = emission(uv, 0.0);
    else if (mode < 1.5) col = pillars(uv);
    else if (mode < 2.5) col = crab(uv);
    else if (mode < 3.5) col = ringNeb(uv, 0.0);
    else if (mode < 4.5) col = emission(uv, 1.0);
    else if (mode < 5.5) col = horsehead(uv);
    else if (mode < 6.5) col = ringNeb(uv, 1.0);
    else if (mode < 20.5) col = sgrBackdrop(uv, inNdc);
    else col = binaryBackdrop(uv, inNdc, mode);
    float ex = mode > 4.5 && mode < 5.5 ? 0.4 : (mode < 6.5 ? 0.55 : 1.0);
    col = 1.0 - exp(-col * 1.25 * ex);
    float A = pc.a.z;
    outColor = vec4(col * A, A);
}
