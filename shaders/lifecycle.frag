#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform P {
    vec4 a;
    vec4 b;
    vec4 c;
    vec4 d;
    vec4 e;
    vec4 f;
} pc;

const float PI = 3.14159265;

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

vec2 rot(vec2 p, float a) {
    float c = cos(a), s = sin(a);
    return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}

vec3 bb(float T) {
    vec3 c = vec3(1.0, 0.22, 0.04);
    c = mix(c, vec3(1.0, 0.34, 0.1), smoothstep(2500.0, 3500.0, T));
    c = mix(c, vec3(1.0, 0.52, 0.2), smoothstep(3500.0, 4500.0, T));
    c = mix(c, vec3(1.0, 0.78, 0.4), smoothstep(4500.0, 5800.0, T));
    c = mix(c, vec3(1.0, 0.92, 0.72), smoothstep(5800.0, 7500.0, T));
    c = mix(c, vec3(0.82, 0.9, 1.0), smoothstep(7500.0, 11000.0, T));
    c = mix(c, vec3(0.6, 0.75, 1.0), smoothstep(11000.0, 30000.0, T));
    return c;
}

vec3 starfield(vec2 ndc, float dim) {
    vec3 col = vec3(0.0);
    for (int l = 0; l < 3; l++) {
        float n = 38.0 + 46.0 * float(l);
        vec2 g = ndc * n + float(l) * 17.0;
        vec2 id = floor(g);
        float h = hash(id + 3.1);
        if (h < 0.93) continue;
        vec2 f = fract(g) - 0.5 - (vec2(hash(id + 7.7), hash(id + 1.3)) - 0.5) * 0.6;
        float s = exp(-dot(f, f) * 60.0) * (0.25 + 0.75 * (h - 0.93) / 0.07);
        col += s * mix(vec3(1.0, 0.8, 0.6), vec3(0.7, 0.8, 1.0), hash(id + 9.9)) * (0.75 + 0.25 * sin(pc.a.y * 2.0 + h * 80.0));
    }
    return col * dim;
}

vec3 nebulaField(vec2 p, float aux, float t) {
    float k = 1.0 + 0.55 * aux;
    vec2 q0 = rot(p, 0.03 * t) * 1.6 * k;
    vec2 w = vec2(fbm(q0 + t * 0.012), fbm(q0 + 5.2 - t * 0.01));
    float d = fbm(q0 * 1.15 + 2.3 * w);
    float tint = fbm(q0 * 0.5 + 9.0);
    vec3 gas = mix(vec3(0.95, 0.22, 0.32), vec3(0.2, 0.42, 1.0), smoothstep(0.35, 0.65, tint));
    gas = mix(gas, vec3(1.0, 0.65, 0.3), smoothstep(0.55, 0.8, fbm(q0 * 0.8 + 3.0)) * 0.7);
    float e = pow(smoothstep(0.28, 0.85, d), 2.2);
    float dust = smoothstep(0.52, 0.72, fbm(q0 * 1.9 + w * 3.0 + 20.0));
    vec3 col = gas * e * 1.6;
    col *= 1.0 - 0.92 * dust * (0.5 + 0.5 * aux);
    float r2 = dot(p, p);
    col += vec3(1.0, 0.7, 0.45) * aux * aux * exp(-r2 * 18.0) * 2.6 * (0.6 + 0.4 * fbm(q0 * 3.0));
    col += vec3(1.0, 0.9, 0.8) * aux * aux * aux * exp(-r2 * 260.0) * 6.0;
    return col;
}

vec3 youngStars(vec2 p, float aux) {
    vec3 col = vec3(0.0);
    for (int l = 0; l < 2; l++) {
        float n = 14.0 + 11.0 * float(l);
        vec2 g = p * n + 40.0 * float(l);
        vec2 id = floor(g);
        float h = hash(id);
        if (h < 0.93) continue;
        vec2 f = fract(g) - 0.5 - (vec2(hash(id + 4.0), hash(id + 8.0)) - 0.5) * 0.5;
        float s = exp(-dot(f, f) * 50.0) * (0.5 + 0.5 * sin(pc.a.y * 1.5 + h * 60.0));
        col += s * mix(vec3(0.8, 0.9, 1.0), vec3(1.0, 0.7, 0.5), hash(id + 2.0)) * (0.6 + 0.8 * aux);
    }
    return col;
}

vec3 starBall(vec2 p, float Ru, float T, float gran, float lum, out float alpha) {
    float r = length(p) / Ru;
    alpha = 0.0;
    if (r >= 1.0) return vec3(0.0);
    alpha = smoothstep(1.0, 0.985, r);
    float mu = sqrt(max(1.0 - r * r, 0.0));
    float limb = 1.0 - 0.62 * (1.0 - mu);
    vec2 sp = rot(p / Ru, pc.a.y * 0.02) * (1.0 + 0.2 * (1.0 - mu));
    float gn = fbm(sp * gran + vec2(pc.a.y * 0.06, 0.0));
    float cells = 0.55 + 0.95 * gn;
    float spots = smoothstep(0.62, 0.74, fbm(sp * 2.2 + 31.0)) * step(gran, 40.0) * (gran > 12.0 ? 0.55 : 0.35);
    vec3 base = bb(T);
    vec3 col = base * limb * cells * (1.0 - spots) * lum;
    return col;
}

vec3 halo(vec2 p, float Ru, float T, float amount) {
    float d = max(length(p), Ru * 0.5);
    float x = d / Ru;
    float g = 0.38 / (1.0 + x * x * 1.1) + 0.12 * exp(-x * 0.9);
    return bb(T) * g * amount * smoothstep(0.0, 1.0, x - 0.2);
}

float ringLine(float d, float w) {
    return 1.0 - smoothstep(0.0, w, abs(d));
}

vec3 orbitsOver(vec2 p, float Ru, float s, float px) {
    float set = pc.e.z;
    float alpha = pc.e.w;
    vec3 col = vec3(0.0);
    if (alpha <= 0.0) return col;
    for (int i = 0; i < 4; i++) {
        float au = set < 0.5 ? (i == 0 ? 0.387 : i == 1 ? 0.723 : i == 2 ? 1.0 : 1.524) : (i == 0 ? 1.0 : i == 1 ? 1.524 : i == 2 ? 5.203 : 9.537);
        float ru = au / s;
        float pr = length(p) - ru;
        float ln = ringLine(pr, px * 1.3) * 0.45;
        float ang = pc.a.y * (0.7 / (au * sqrt(au))) + float(i) * 1.9;
        vec2 dp = p - ru * vec2(cos(ang), sin(ang));
        float dr = max(px * 2.2, 0.006);
        float hot = 1.0 + 3.0 * smoothstep(1.5, 0.9, ru / max(Ru, 1e-6));
        float dot_ = exp(-dot(dp, dp) / (dr * dr)) * hot;
        float alive = step(Ru * 1.0, ru);
        col += alpha * (ln * vec3(0.5, 0.65, 1.0) * 0.8 + alive * dot_ * vec3(0.75, 0.9, 1.0) * (i == 2 && set < 0.5 ? vec3(0.4, 0.8, 1.0) : vec3(1.0)));
    }
    return col;
}

vec3 jets(vec2 p, float len, float aux) {
    float y = abs(p.y);
    float w = 0.012 + 0.06 * y;
    float lenF = smoothstep(len, len * 0.5, y);
    float core = exp(-(p.x * p.x) / (w * w)) * lenF / (1.0 + 3.0 * y);
    float knots = 0.6 + 0.4 * sin(y * 22.0 - pc.a.y * 4.0);
    float flare = exp(-(p.x * p.x) / (w * w * 9.0)) * lenF * 0.18;
    return (vec3(0.55, 0.78, 1.0) * core * knots * 1.8 + vec3(0.9, 0.5, 0.35) * flare) * aux;
}

vec3 protostar(vec2 p, float Ru, float aux, float elev) {
    float se = max(sin(elev), 0.1);
    vec2 q = vec2(p.x, p.y / se);
    float rho = length(q);
    float Rd = 0.18 + 0.5 * aux;
    float r0 = Ru * 1.3;
    float ang = atan(q.y, q.x);
    float prof = pow(clamp(r0 / max(rho, r0), 0.0, 1.0), 1.1) * smoothstep(Rd, Rd * 0.55, rho);
    float bandPx = fwidth(rho * 55.0);
    float bandK = 0.35 * (1.0 - smoothstep(0.6, 1.8, bandPx));
    float bands = 0.65 + bandK * sin(rho * 55.0 + fbm(vec2(rho * 12.0, ang * 2.0 + pc.a.y * 0.3 / max(rho, 0.05))) * 6.0);
    float inner = smoothstep(r0 * 0.8, r0, rho);
    vec3 dcol = mix(vec3(1.0, 0.8, 0.5), vec3(0.75, 0.22, 0.1), clamp(rho / Rd, 0.0, 1.0));
    vec3 disc = dcol * prof * bands * inner * 1.7;
    float back = step(0.0, p.y);
    float lane = exp(-p.y * p.y / (0.0025 * se + 0.00002)) * smoothstep(Ru * 0.5, Ru * 4.0, abs(p.x)) * smoothstep(Rd, 0.0, abs(p.x));
    disc *= 1.0 - 0.5 * lane * (1.0 - back);
    return disc;
}

vec3 envelope(vec2 p, float amt) {
    float n = fbm(p * 3.5 + pc.a.y * 0.02);
    float r2 = dot(p, p);
    return vec3(0.75, 0.3, 0.2) * amt * n * n * exp(-r2 * 1.8) * 1.4;
}

vec3 shells(vec2 p, float s, float u) {
    vec3 col = vec3(0.0);
    float len = length(p);
    float ang = atan(p.y, p.x);
    for (int k = 0; k < 5; k++) {
        float uk = 0.44 + 0.095 * float(k);
        float rsAu = max(u - uk, 0.0) * 16.0;
        if (rsAu <= 0.0) continue;
        float rs = rsAu / s;
        float th = 0.05 * rs + 0.004;
        float clump = fbm(vec2(ang * 3.0 + float(k) * 5.0, len * 4.0 / max(rs, 0.05)) + 3.0);
        float sh = exp(-pow((len - rs) / th, 2.0)) * (0.35 + 1.1 * clump) * exp(-(u - uk) * 2.2);
        col += sh * mix(vec3(1.0, 0.55, 0.3), vec3(0.45, 0.8, 1.0), fract(float(k) * 0.37 + 0.2));
    }
    return col;
}

vec3 ringNebula(vec2 p, float Rr, float alpha, float incl) {
    vec2 q = vec2(p.x, p.y / max(cos(incl * 0.9), 0.35));
    float rho = length(q);
    float x = rho / Rr;
    float ang = atan(q.y, q.x);
    float fil = pow(fbm(q / Rr * 6.0 + vec2(0.0, pc.a.y * 0.02)), 2.0) * 2.4 + 0.35;
    float rim = exp(-pow((x - 1.0) / 0.17, 2.0));
    float outer = smoothstep(1.9, 1.05, x) * smoothstep(1.0, 1.12, x);
    float innerFill = smoothstep(1.0, 0.0, x) * 0.35;
    vec3 col = rim * mix(vec3(0.3, 1.0, 0.6), vec3(1.0, 0.45, 0.2), smoothstep(0.85, 1.2, x)) * fil;
    col += outer * vec3(0.9, 0.12, 0.2) * 0.55 * (0.5 + fbm(q / Rr * 4.0 + 9.0));
    col += innerFill * vec3(0.2, 0.6, 1.0) * (0.6 + 0.8 * fbm(q / Rr * 5.0 + 4.0));
    col *= 1.0 + 0.5 * cos(ang * 2.0);
    return col * alpha;
}

vec3 spikes(vec2 p, float amt, float size) {
    float d = length(p);
    float cross = exp(-abs(p.x) / (size * 0.03)) * exp(-abs(p.y) / size) + exp(-abs(p.y) / (size * 0.03)) * exp(-abs(p.x) / size);
    return vec3(0.75, 0.85, 1.0) * (cross * 0.9 + exp(-d / (size * 0.12)) * 1.3) * amt;
}

vec3 supernova(vec2 p, float Rs, float s, float u, float flash) {
    float rs = Rs / s;
    float len = length(p);
    float ang = atan(p.y, p.x);
    float wob = (fbm(vec2(ang * 2.5, 1.0) + 7.0) - 0.5) * 0.28 + (fbm(vec2(ang * 9.0, 4.0)) - 0.5) * 0.08;
    float rr = rs * (1.0 + wob);
    float thick = 0.16 * rs + 0.002;
    float shell = exp(-pow((len - rr) / thick, 2.0));
    float rn = max(rs, 1e-4);
    float n1 = fbm(p / rn * 9.0 + 2.0);
    float ridge = pow(1.0 - abs(2.0 * n1 - 1.0), 3.0);
    float heat = exp(-(u - 0.2) * 1.8);
    vec3 cA = mix(vec3(1.0, 0.3, 0.12), vec3(0.25, 1.0, 0.65), smoothstep(0.45, 0.7, fbm(p / rn * 3.0 + 11.0)));
    vec3 col = shell * (0.25 + 2.6 * ridge) * mix(cA, vec3(0.6, 0.8, 1.0), heat * 0.6) * (1.0 + 3.0 / (1.0 + rs * 40.0));
    float n2 = fbm(p / rn * 6.0 + 17.0);
    float ridge2 = pow(1.0 - abs(2.0 * n2 - 1.0), 4.0);
    col += smoothstep(rr, 0.0, len) * ridge2 * vec3(0.9, 0.45, 0.85) * 0.9 * (0.4 + heat);
    col += smoothstep(rr * 1.4, rr, len) * smoothstep(rr * 0.9, rr * 1.05, len) * vec3(0.35, 0.5, 1.0) * 0.35;
    float fl = flash * (exp(-len * len * 1.5) * 4.0 + exp(-len * 0.7) * 1.2);
    col += vec3(1.0, 0.95, 0.9) * fl;
    return col;
}

vec3 pulsarBeams(vec2 p, float Ru, float phase, vec2 axisDir) {
    float alpha = 1.05;
    vec3 m = vec3(sin(alpha) * cos(phase), cos(alpha), sin(alpha) * sin(phase));
    vec2 q = vec2(dot(p, vec2(axisDir.y, -axisDir.x)), dot(p, axisDir));
    vec2 md = m.xy;
    float ml = length(md);
    vec3 col = vec3(0.0);
    float lb = 7.5 * Ru;
    for (int sgn = -1; sgn <= 1; sgn += 2) {
        vec2 dir = float(sgn) * normalize(md + 1e-5);
        float along = dot(q, dir);
        float perp = abs(q.x * dir.y - q.y * dir.x);
        float len = lb * ml;
        float facing = float(sgn) * m.z;
        float vis = 0.25 + 0.75 * smoothstep(-0.2, 1.0, facing);
        float w = Ru * (0.35 + 0.22 * max(along, 0.0) / Ru);
        float b = step(0.0, along) * exp(-pow(perp / w, 2.0)) * smoothstep(len, len * 0.3, along);
        col += b * vec3(0.55, 0.78, 1.0) * vis * 2.2 / (1.0 + 0.1 * max(along, 0.0) / Ru);
        float pulse = pow(max(facing, 0.0), 8.0);
        col += pulse * vec3(0.8, 0.92, 1.0) * (exp(-dot(q, q) / (Ru * Ru * 8.0)) * 3.2 + exp(-length(q) / (Ru * 6.0)) * 0.8);
    }
    for (int i = 0; i < 3; i++) {
        float L = (2.4 + 1.5 * float(i)) * Ru;
        vec2 u2 = vec2(dot(q, md / max(ml, 1e-4)), dot(q, vec2(-md.y, md.x) / max(ml, 1e-4)));
        float rho = length(u2) + 1e-5;
        float c = u2.x / rho;
        float target = L * (1.0 - c * c);
        col += exp(-pow((rho - target) / (Ru * 0.1), 2.0)) * vec3(0.25, 0.5, 1.0) * 0.35 * smoothstep(0.1, 0.3, rho / Ru - 0.9);
    }
    return col;
}

vec3 blackHole(vec2 p, float rsU, float elev, float t) {
    vec2 q = p / rsU;
    float se = max(sin(elev), 0.08);
    float rin = 3.0, rout = 8.5;
    float shadow = 2.6;
    float len = length(q);
    vec3 col = vec3(0.0);
    float cx = q.x / max(len, 1e-4);
    vec2 d = vec2(q.x, q.y / se);
    float rho = length(d);
    float ang = atan(d.y, d.x);
    float swirl = fbm(vec2(rho * 1.6, ang * 1.7 - t * 0.9 / pow(max(rho, 1.0), 1.5)) + 3.0);
    float prof = pow(rin / max(rho, rin), 2.0) * smoothstep(rout, rout * 0.55, rho) * smoothstep(rin * 0.9, rin * 1.1, rho);
    float dop = clamp(1.0 + 0.7 * cx, 0.15, 2.2);
    vec3 hot = mix(vec3(1.0, 0.5, 0.15), vec3(1.0, 0.9, 0.7), clamp(rin / max(rho, rin) * 1.3 - 0.1, 0.0, 1.0));
    float front = step(q.y, 0.0) + step(0.0, q.y) * step(shadow, len);
    col += hot * prof * (0.55 + 0.9 * swirl) * pow(dop, 3.0) * front * 1.3;
    float rl = len;
    float arc = pow(rin / max(rl, rin), 2.0) * smoothstep(rout * 0.55, rin * 1.1, rl) * smoothstep(rin * 0.8, rin * 1.15, rl);
    arc *= smoothstep(-0.25, 0.9, q.y / max(len, 1e-3)) * 0.8 + 0.12;
    float arcLow = pow(rin / max(rl, rin), 3.0) * smoothstep(rin * 0.9, rin * 1.1, rl) * 0.28;
    col += hot * (arc + arcLow * step(q.y, 0.0)) * (0.5 + 0.8 * swirl) * pow(dop, 2.0) * smoothstep(rout * 0.9, rin, rl);
    col += vec3(1.0, 0.85, 0.65) * exp(-pow((len - 2.72) / 0.09, 2.0)) * 0.95;
    float lensed = 1.0 + 0.5 * exp(-pow((len - 3.2) / 1.6, 2.0));
    return col * lensed;
}

vec3 whiteDwarf(vec2 p, float Ru, float T, float lum) {
    float r = length(p) / Ru;
    float mu = sqrt(max(1.0 - r * r, 0.0));
    vec3 base = T > 12000.0 ? mix(bb(12000.0), vec3(0.62, 0.78, 1.0), clamp((T - 12000.0) / 30000.0, 0.0, 1.0)) : bb(T);
    vec3 col = base * (r < 1.0 ? (1.0 - 0.45 * (1.0 - mu)) * smoothstep(1.0, 0.985, r) : 0.0) * lum;
    float d = max(length(p), Ru * 0.5) / Ru;
    col += base * lum * (0.4 / (1.0 + d * d * 0.9)) * smoothstep(0.5, 1.4, d);
    return col;
}

#include "lifecycle2.glsl"

void main() {
    vec2 ndc = inNdc;
    float aspect = pc.a.x;
    vec2 p = ndc * vec2(aspect, 1.0);
    float t = pc.a.y;
    int kind = int(pc.a.z + 0.5);
    float u = pc.a.w;
    float viewH = pc.b.x;
    float s = viewH * 0.5;
    float R = pc.b.y;
    float T = pc.b.z;
    float aux = pc.b.w;
    float roll = pc.c.x;
    float elev = pc.c.y;
    float flash = pc.c.z;
    float ringAu = pc.c.w;
    float px = pc.e.x;
    vec2 pr = rot(p, roll);
    float Ru = R / s;
    vec3 col = vec3(0.0);
    float a;

    if (kind == 0) {
        col = nebulaField(pr, aux, t) + youngStars(pr, aux) * 0.9 + starfield(ndc, 0.35);
    } else if (kind == 1) {
        vec3 bg = envelope(pr, 1.0 - 0.5 * aux) + starfield(ndc, 0.25);
        vec2 pj = pr;
        vec3 disc = protostar(pr, Ru, aux, elev);
        float backMask = step(0.0, pr.y);
        col = bg + disc * backMask + jets(pj, 0.15 + 1.1 * aux, aux);
        vec3 sb = starBall(pr, Ru, 4200.0, 6.0, 1.4, a);
        col = mix(col, sb, a);
        col += halo(pr, Ru, 3800.0, 1.7);
        col += disc * (1.0 - backMask);
    } else if (kind == 2 || kind == 3 || kind == 4) {
        col = starfield(ndc, 0.35 * (1.0 - clamp(Ru * 1.2, 0.0, 1.0)));
        if (kind == 4) col += shells(p, s, u);
        if (kind == 3) col += orbitsOver(p, Ru, s, px);
        float gran = kind == 2 && R < 0.01 ? 26.0 : (kind == 2 ? 11.0 : 3.2);
        float hot = smoothstep(9000.0, 20000.0, T);
        float lum = mix(0.95, 1.5, hot);
        vec3 sb = starBall(pr, Ru, T, gran, lum, a);
        if (kind == 2 && R < 0.003) sb *= mix(vec3(1.0), vec3(0.55, 0.78, 1.25), smoothstep(5000.0, 12000.0, T)) * mix(1.0, 0.8, smoothstep(5000.0, 12000.0, T));
        col = mix(col, sb, a);
        float rr = max(Ru, 2.0 * px);
        col += halo(p, rr, T, mix(0.8, 1.5, hot));
        if (T > 9000.0) col += spikes(p, 0.35 * (0.4 + 0.6 * hot), rr * 3.0);
        col += vec3(1.0, 0.97, 0.9) * flash * (exp(-dot(p, p) * 2.2) * 1.8 + exp(-length(p) * 0.6) * 0.5);
    } else if (kind == 5) {
        col = starfield(ndc, 0.35) + ringNebula(pr, ringAu / s, smoothstep(0.0, 0.12, u) * 1.4, elev);
        float rr = max(Ru, 2.0 * px);
        col += spikes(p, 1.1, rr * 6.0);
    } else if (kind == 6) {
        float nebAlpha = 1.0 - smoothstep(0.0, 0.5, u);
        col = starfield(ndc, 0.35);
        if (ringAu > 0.0) col += ringNebula(pr, ringAu / s, 1.4 * nebAlpha * (1.0 - smoothstep(0.9, 2.2, ringAu / s)), elev);
        col += vec3(0.012, 0.016, 0.03) * (0.5 + fbm(ndc * 2.2 + 3.0)) * smoothstep(0.4, 0.7, aux);
        float lum = clamp(pow(T / 6000.0, 3.2), 0.0, 1.7);
        float rr = max(Ru, 1.5 * px);
        float rad = length(p);
        if (rad < rr) col = vec3(0.012, 0.012, 0.02) * (0.5 + fbm(ndc * 2.2 + 3.0));
        vec3 wd = whiteDwarf(p, rr, T, lum);
        col += wd;
    } else if (kind == 8) {
        col = starfield(ndc, 0.25);
        float coreU = clamp(u / 0.2, 0.0, 1.0);
        if (u < 0.21) {
            float sb_a;
            vec3 sb = starBall(pr, Ru, T, 3.0, 1.4 + 3.0 * coreU * coreU, sb_a);
            col = mix(col, sb, sb_a);
            col += halo(p, max(Ru, 2.0 * px), T, 1.2 + 4.0 * coreU);
        }
        col += supernova(p, ringAu, s, u, flash);
        if (u > 0.3) col += spikes(p, 0.5, max(0.012, 3.0 * px));
    } else if (kind == 9) {
        col = starfield(ndc, 0.3);
        col += supernova(p, ringAu, s, 1.0, 0.0) * (1.0 - smoothstep(1.2, 3.0, ringAu / s));
        float rr = max(Ru, 3.0 * px);
        float rad = length(p);
        col += pulsarBeams(pr, rr, pc.e.y, vec2(0.0, 1.0));
        float sba;
        vec3 sb = starBall(pr, rr, 12000.0, 4.0, 2.2, sba);
        col = mix(col, sb * vec3(0.7, 0.85, 1.2) + vec3(0.5, 0.65, 1.0) * 0.8, sba);
        col += halo(pr, rr, 30000.0, 1.1);
    } else if (kind == 11) {
        float rr = max(Ru, 3.0 * px);
        col = starfield(ndc, 0.3) + wolfRayet(pr, rr, ringAu / s, u, t);
    } else if (kind == 12) {
        float rr = max(Ru, 3.0 * px);
        col = starfield(ndc, 0.3) + magnetarField(pr, rr, pc.e.y, u, t);
        float sba;
        vec3 sb = starBall(pr, rr, 18000.0, 4.0, 2.4, sba);
        col = mix(col, sb * vec3(0.8, 0.85, 1.2) + vec3(0.6, 0.6, 1.0) * 0.8, sba);
        col += halo(pr, rr, 30000.0, 1.4);
        col += vec3(1.0, 0.9, 1.0) * flash * (exp(-dot(p, p) * 2.5) * 3.0 + exp(-length(p) * 0.8) * 0.8);
    } else if (kind == 13) {
        col = starfield(ndc, 0.2) + kilonova(pr, u, t);
        col += vec3(1.0, 0.93, 0.85) * flash * (exp(-dot(p, p) * 2.0) * 3.5 + exp(-length(p) * 0.7) * 1.0);
    } else if (kind == 10) {
        col = starfield(ndc, 0.4);
        col += blackHole(rot(p, roll), max(R / s, 1e-6), elev, t);
    }

    float f = pc.d.w;
    col *= f;

    vec2 fc = gl_FragCoord.xy / vec2(2.0 / px * aspect, 2.0 / px);
    float seg = pc.d.y;
    float x0 = 0.06, x1 = 0.94;
    if (fc.y > 0.912 && fc.y < 0.924 && fc.x > x0 && fc.x < x1) {
        float sw = (x1 - x0) / seg;
        float rel = (fc.x - x0) / sw;
        float idx = floor(rel);
        float inSeg = fract(rel);
        float gap = 0.04;
        if (inSeg > gap && inSeg < 1.0 - gap) {
            float cur = pc.d.z;
            float fill = idx < cur ? 1.0 : (idx == cur ? step(inSeg, u) : 0.0);
            vec3 bar = mix(vec3(0.12, 0.14, 0.2), vec3(1.0, 0.8, 0.4), fill);
            if (idx == cur && fill < 0.5) bar = vec3(0.3, 0.34, 0.45);
            col = mix(col, bar, 0.93);
        }
    }

    float barY = 0.11;
    float bx0 = 0.04;
    float bx1 = bx0 + pc.d.x;
    float onBar = step(abs(fc.y - barY), 1.2 * px * 0.5) * step(bx0, fc.x) * step(fc.x, bx1)
        + step(abs(fc.y - barY), 5.0 * px * 0.5) * (step(abs(fc.x - bx0), 1.0 * px * 0.5 / aspect) + step(abs(fc.x - bx1), 1.0 * px * 0.5 / aspect));
    col = mix(col, vec3(0.9, 0.92, 1.0), clamp(onBar, 0.0, 1.0) * 0.9);

    col = 1.0 - exp(-col * 1.25);
    col = pow(col, vec3(0.92));
    if (pc.f.y > 0.5) col = elementTable(col, gl_FragCoord.xy, 2.0 / px * aspect);
    outColor = vec4(col, 1.0);
}
