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
} p;

const float RE = 6371.0;
const float D2R = 0.0174533;

float h31(vec3 q) {
    q = fract(q * 0.3183099 + 0.1);
    q *= 17.0;
    return fract(q.x * q.y * q.z * (q.x + q.y + q.z));
}

float h11(float n) { return fract(sin(n * 91.345) * 47453.5453); }

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

vec3 dirLL(float lat, float lon) {
    lat *= D2R;
    lon *= D2R;
    return vec3(cos(lat) * cos(lon), cos(lat) * sin(lon), sin(lat));
}

float blob(vec3 P, float lat, float lon, float rad) {
    float cr = cos(rad * D2R);
    return (dot(P, dirLL(lat, lon)) - cr) / (1.0 - cr);
}

float landq(vec3 P) {
    float q = -1.0;
    q = max(q, blob(P, 55.0, 60.0, 30.0));
    q = max(q, blob(P, 52.0, 95.0, 28.0));
    q = max(q, blob(P, 48.0, 25.0, 18.0));
    q = max(q, blob(P, 62.0, 125.0, 24.0));
    q = max(q, blob(P, 38.0, 100.0, 22.0));
    q = max(q, blob(P, 66.0, 45.0, 14.0));
    q = max(q, blob(P, 24.0, 78.0, 10.0));
    q = max(q, blob(P, 14.0, 77.0, 7.0));
    q = max(q, blob(P, 14.0, 103.0, 9.0));
    q = max(q, blob(P, 23.0, 46.0, 12.0));
    q = max(q, blob(P, 28.0, 14.0, 18.0));
    q = max(q, blob(P, 6.0, 20.0, 22.0));
    q = max(q, blob(P, -14.0, 24.0, 17.0));
    q = max(q, blob(P, -28.0, 24.0, 9.0));
    q = max(q, blob(P, 48.0, -100.0, 24.0));
    q = max(q, blob(P, 62.0, -108.0, 17.0));
    q = max(q, blob(P, 36.0, -86.0, 13.0));
    q = max(q, blob(P, 20.0, -100.0, 9.0));
    q = max(q, blob(P, 73.0, -40.0, 11.0));
    q = max(q, blob(P, -6.0, -62.0, 18.0));
    q = max(q, blob(P, -26.0, -60.0, 13.0));
    q = max(q, blob(P, -43.0, -68.0, 6.0));
    q = max(q, blob(P, -25.0, 134.0, 15.0));
    q = max(q, blob(P, -3.0, 115.0, 7.0));
    q = max(q, blob(P, 54.0, -3.0, 4.0));
    q = max(q, (-P.z - 0.9) * 10.0);
    return q;
}

vec3 earthColor(vec3 P, vec3 sun, vec3 view, float pxk) {
    float n = fbm(P * 6.0 + 2.0);
    float lv = landq(P) + 0.55 * (n - 0.5) - 0.1;
    float land = smoothstep(-0.01, 0.01, lv);
    vec3 K = P * RE;
    float dn = 0.0;
    float sz[5] = float[5](260.0, 60.0, 14.0, 3.2, 0.7);
    float am[5] = float[5](0.25, 0.22, 0.2, 0.18, 0.15);
    for (int i = 0; i < 5; i++)
        dn += am[i] * (vnoise(K / sz[i]) - 0.5) * smoothstep(pxk * 1.5, pxk * 6.0, sz[i]);
    float dry = smoothstep(0.45, 0.72, fbm(P * 9.0 + 5.0)) + 0.55 * exp(-pow((abs(P.z) - 0.45) / 0.15, 2.0));
    vec3 ground = mix(vec3(0.13, 0.27, 0.09), vec3(0.56, 0.45, 0.26), clamp(dry, 0.0, 1.0));
    ground *= 0.8 + 0.5 * fbm(P * 20.0);
    ground *= 1.0 + 4.0 * dn;
    float ice = smoothstep(0.86, 0.93, abs(P.z) + 0.05 * (n - 0.5));
    ground = mix(ground, vec3(0.9, 0.93, 0.97), ice);
    vec3 deep = vec3(0.008, 0.045, 0.16);
    vec3 shallow = vec3(0.03, 0.2, 0.34);
    vec3 ocean = mix(deep, shallow, smoothstep(-0.3, 0.0, lv)) * (1.0 + 1.0 * dn);
    float lit = smoothstep(-0.08, 0.3, dot(P, sun));
    vec3 col = mix(ocean, ground, land);
    float diff = max(dot(P, sun), 0.0);
    col = col * (0.025 + 1.05 * diff * lit);
    vec3 refl = reflect(-sun, P);
    float glint = pow(max(dot(refl, -view), 0.0), 70.0) * (1.0 - land) * lit;
    col += vec3(0.9, 0.85, 0.75) * glint * 0.8;
    float cl = smoothstep(0.5, 0.75, fbm(P * 4.5 + 7.0 + p.c.w * 0.002));
    col = mix(col, vec3(0.95, 0.96, 1.0) * (0.04 + diff * lit), cl * 0.55 * smoothstep(0.4, 4.0, pxk));
    return col;
}

float segDist(vec3 ro, vec3 d, vec3 S0, vec3 S1, out float w, out float s) {
    vec3 e = S1 - S0;
    vec3 g0 = ro - S0;
    float a = dot(e, e);
    float b = dot(e, d);
    float dd = dot(e, g0);
    float ee = dot(d, g0);
    float den = a - b * b;
    w = den > 1e-12 ? clamp((dd - ee * b) / den, 0.0, 1.0) : 0.0;
    s = max(w * b - ee, 0.0);
    return length(g0 + d * s - e * w);
}

vec3 fireRamp(float h) {
    vec3 c = mix(vec3(0.45, 0.07, 0.02), vec3(1.0, 0.4, 0.08), smoothstep(0.0, 0.35, h));
    c = mix(c, vec3(1.0, 0.82, 0.4), smoothstep(0.3, 0.7, h));
    return mix(c, vec3(1.0, 0.97, 0.9), smoothstep(0.7, 1.1, h));
}

void main() {
    float tf = p.a.w, asp = p.b.w;
    float cxN = 0.5 * (p.f.w - 1.0);
    vec3 F = p.a.xyz, Rg = p.b.xyz;
    vec3 U = cross(Rg, F);
    vec3 d = normalize(F + Rg * ((inNdc.x - cxN) * tf * asp) - U * (inNdc.y * tf));
    vec3 ro = p.c.xyz;
    float clk = p.c.w;
    vec3 I = p.d.xyz;
    float tHit = p.d.w;
    vec3 A = p.e.xyz;
    bool miss = p.e.w > 0.5;
    vec3 B = p.f.xyz;
    float R1 = p.g.x, R5 = p.g.y, Rc = p.g.z, Rf = p.g.w;
    float veil = p.h.x, ej = p.h.y, u = p.h.z;
    bool preview = p.h.w > 0.5;
    float pxn = fwidth(inNdc.y);

    vec3 ee = normalize(cross(vec3(0.0, 0.0, 1.0), I) + vec3(1e-5, 0.0, 0.0));
    vec3 nn = cross(I, ee);
    vec3 sun = normalize(I * 0.5 - ee * 0.7 + nn * 0.35);

    float bq = dot(ro, d);
    float cq = dot(ro, ro) - 1.0;
    float disc = bq * bq - cq;
    float tS = disc > 0.0 ? -bq - sqrt(disc) : -bq;
    bool hit = disc > 0.0 && tS > 0.0;
    vec3 Pq = normalize(ro + d * max(tS, 0.0));
    float dI = RE * atan(length(cross(Pq, I)), dot(Pq, I));
    float pxd = fwidth(dI);
    float alt = max(length(ro) - 1.0, 1e-6);
    float pxk = max(alt * RE * tf * pxn, 1e-4);

    float hitDepth = hit ? tS : 1e9;
    float vv = veil * smoothstep(1.0, 14.0, tHit);
    vec3 col = vec3(0.003, 0.005, 0.011);

    if (!hit) {
        vec3 g = d * 230.0;
        vec3 id = floor(g);
        float hs = h31(id);
        if (hs > 0.993) col += vec3(0.8, 0.85, 1.0) * ((hs - 0.993) / 0.007) * smoothstep(0.42, 0.0, length(fract(g) - 0.5));
        if (bq < 0.0) {
            float m = length(ro + d * (-bq));
            if (m > 1.0) {
                float sh = exp(-(m - 1.0) * 55.0);
                vec3 pc = normalize(ro + d * (-bq));
                float lt = smoothstep(-0.25, 0.4, dot(pc, sun));
                vec3 rim = mix(vec3(0.18, 0.42, 1.0), vec3(0.5, 0.26, 0.1), vv);
                col += rim * sh * 0.75 * (0.08 + lt);
            }
        }
    } else {
        vec3 P = normalize(ro + d * tS);
        col = earthColor(P, sun, d, pxk);
        float lit = smoothstep(-0.2, 0.4, dot(P, sun));
        float fres = pow(1.0 - max(dot(P, -d), 0.0), 3.0);
        col += mix(vec3(0.2, 0.45, 1.0), vec3(0.5, 0.26, 0.1), vv) * fres * 0.6 * (0.1 + lit);
        float dark = vv * (0.78 + 0.22 * fbm(P * 5.0 + clk * 0.01));
        col = mix(col, col * 0.16 + vec3(0.06, 0.035, 0.022) * (0.2 + lit), dark * 0.93);

        float lw = max(pxd * 1.5, 1e-4);
        if (preview) {
            float rm = pxk * 9.0 * (1.0 + 0.12 * sin(clk * 3.0));
            float mk = 1.0 - smoothstep(0.0, lw, abs(dI - rm));
            col += vec3(1.0, 0.9, 0.5) * mk * 0.9;
            col += vec3(1.0, 0.9, 0.5) * (1.0 - smoothstep(0.0, lw, dI)) * 0.6;
            if (!miss) {
                float l1 = 1.0 - smoothstep(0.0, lw, abs(dI - R1));
                float l5 = 1.0 - smoothstep(0.0, lw, abs(dI - R5));
                col += vec3(1.0, 0.88, 0.35) * l1 * 0.5;
                col += vec3(1.0, 0.42, 0.2) * l5 * 0.5;
            }
        } else if (!miss) {
            float x = clamp(tHit / 10.0, 0.0, 1.0);
            float rs = R1 * (1.0 - (1.0 - x) * (1.0 - x));
            float scorch = step(dI, rs) * smoothstep(R5 * 1.1, R5 * 0.15, dI);
            col = mix(col, col * 0.5 + vec3(0.09, 0.03, 0.005), scorch * 0.75);
            float in5 = step(dI, min(rs, R5));
            float in1 = step(dI, min(rs, R1));
            col += vec3(0.5, 0.12, 0.03) * in5 * 0.16;
            col += vec3(0.5, 0.4, 0.05) * in1 * 0.07;
            float a1 = smoothstep(0.985, 1.0, rs / max(R1, 1e-3));
            float a5 = smoothstep(0.985, 1.0, rs / max(R5, 1e-3));
            col += vec3(1.0, 0.88, 0.35) * (1.0 - smoothstep(0.0, lw, abs(dI - R1))) * 0.9 * a1;
            col += vec3(1.0, 0.42, 0.2) * (1.0 - smoothstep(0.0, lw, abs(dI - R5))) * 0.9 * a5;
            float fadeF = 1.0 - smoothstep(10.0, 18.0, tHit);
            float front = exp(-pow((dI - rs) / max(pxd * 2.5, 0.02 * rs), 2.0)) * (1.0 - 0.6 * x) * fadeF;
            col += vec3(1.0, 0.8, 0.5) * front * 1.4 * step(0.0, tHit);
            if (Rc > 0.0) {
                float crAmt = smoothstep(0.3, 1.2, tHit) * step(0.0, tHit);
                float inC = 1.0 - smoothstep(Rc - lw, Rc, dI);
                col = mix(col, col * 0.25 + vec3(0.25, 0.07, 0.01) * exp(-tHit * 0.15), inC * crAmt);
                col += vec3(1.0, 0.85, 0.6) * (1.0 - smoothstep(0.0, lw, abs(dI - Rc))) * 0.8 * crAmt;
            }
        }
    }

    if (!miss && tHit > 0.0 && Rf > 0.0) {
        float rr = Rf * (0.12 + 0.88 * smoothstep(0.0, 2.5, tHit)) / RE;
        vec3 oc = ro - B;
        float qb = dot(oc, d);
        float oc2 = dot(oc, oc);
        float dd = qb * qb - (oc2 - rr * rr);
        float fade = exp(-max(tHit - 3.0, 0.0) / 7.0) * smoothstep(0.0, 0.15, tHit);
        float m = sqrt(max(oc2 - qb * qb, 0.0));
        if (dd > 0.0) {
            float sq = sqrt(dd);
            float t0 = max(-qb - sq, 0.0);
            float t1 = min(-qb + sq, hitDepth);
            if (t1 > t0) {
                float chord = (t1 - t0) / rr;
                float rho = m / rr;
                vec3 mid = ro + d * (0.5 * (t0 + t1)) - B;
                float nz = fbm(mid / rr * 2.5 + vec3(0.0, 0.0, clk * 0.6));
                float dens = pow(max(1.0 - rho * rho, 0.0), 0.7) * (0.6 + 0.8 * nz);
                float heat = (1.0 - rho) * (1.25 - 0.7 * clamp((tHit - 2.5) / 12.0, 0.0, 1.0)) + 0.25 * nz;
                float al = (1.0 - exp(-chord * 1.6)) * dens;
                col += fireRamp(heat) * al * fade * 2.2;
            }
        }
        if (qb < 0.0) {
            float halo = exp(-max(m - rr, 0.0) / (rr * 1.3));
            col += vec3(1.0, 0.55, 0.2) * halo * 0.3 * fade;
        }
    }

    if (!miss && ej > 0.0 && tHit > 0.0) {
        for (int i = 0; i < 64; i++) {
            float fi = float(i);
            float h1 = h11(fi + 1.0), h2 = h11(fi + 71.0), h3 = h11(fi + 141.0);
            float sp = 0.3 + 0.7 * h2;
            float rng = ej * sp;
            float Tf = 2.0 + 5.0 * sp;
            float f = (tHit - 0.15 - 0.4 * h3) / Tf;
            if (f <= 0.0 || f >= 1.0) continue;
            float az = h1 * 6.2831853;
            vec3 hd = cos(az) * nn + sin(az) * ee;
            float ang = rng * f / RE;
            float hgt = 4.0 * f * (1.0 - f) * rng * (0.25 + 0.5 * h3) / RE;
            vec3 pos = (I * cos(ang) + hd * sin(ang)) * (1.0 + hgt);
            vec3 v = pos - ro;
            float z = dot(v, F);
            if (z <= 1e-5) continue;
            if (length(v) > hitDepth + 0.0004) continue;
            vec2 sc = vec2(dot(v, Rg) / z / (tf * asp) + cxN, -dot(v, U) / z / tf);
            vec2 dv = (inNdc - sc) * vec2(asp, 1.0);
            float rad = max(ej * 0.02 / RE / z / tf, 1.6 * pxn);
            float gl = rad * rad / (dot(dv, dv) + rad * rad);
            gl *= gl;
            col += mix(vec3(1.0, 0.85, 0.55), vec3(1.0, 0.3, 0.08), f) * gl * sqrt(1.0 - f) * 1.4;
        }
    }

    if (u > 0.0) {
        vec3 head = mix(A, B, u);
        float trailFade = (!miss && tHit > 0.0) ? exp(-tHit / 5.0) : 1.0;
        if (miss && u >= 1.0) trailFade = 0.0;
        if (trailFade > 0.01) {
            float w, s;
            float dist = segDist(ro, d, A, head, w, s);
            vec3 q = mix(A, head, w);
            if (s < hitDepth + 0.0005) {
                float wpx = s * tf * pxn;
                float ah = max(length(q) - 1.0, 0.0);
                float heat = smoothstep(0.3, 0.0, ah);
                float widthK = miss ? 3.0 : 2.0;
                float core = exp(-pow(dist / (wpx * widthK), 2.0));
                float halo = exp(-pow(dist / (wpx * widthK * 4.0), 2.0));
                float tail = pow(w, 1.6);
                float br = (0.18 + 0.82 * heat) * tail;
                col += mix(vec3(1.0, 0.5, 0.15), vec3(1.0, 0.95, 0.85), core) * (core * 1.2 + halo * 0.35) * br * trailFade;
            }
            if (tHit <= 0.0) {
                vec3 hv = head - ro;
                float hz = dot(hv, d);
                if (hz > 0.0 && hz < hitDepth + 0.0005) {
                    float hd2 = length(hv - d * hz);
                    float hw = hz * tf * pxn * (miss ? 6.0 : 4.0);
                    float ah = max(length(head) - 1.0, 0.0);
                    float hb = 0.5 + 0.5 * smoothstep(0.3, 0.0, ah);
                    col += vec3(1.0, 0.9, 0.7) * exp(-pow(hd2 / hw, 2.0)) * 1.5 * hb;
                }
            }
        }
    }

    if (miss) {
        vec3 M = 0.5 * (A + B);
        float w, s;
        float dist = segDist(ro, d, I, M, w, s);
        if (s < hitDepth + 0.0005) {
            float wpx = s * tf * pxn;
            float dash = step(0.5, fract(w * 26.0));
            col += vec3(0.4, 0.9, 1.0) * exp(-pow(dist / (wpx * 1.1), 2.0)) * dash * 0.8;
        }
        if (abs(d.z) > 1e-5) {
            float sp = -ro.z / d.z;
            if (sp > 0.0 && sp < hitDepth) {
                vec3 hp = ro + d * sp;
                float rg = length(hp.xy);
                float lwd = sp * tf * pxn * 1.1;
                col += vec3(0.35, 0.55, 1.0) * (1.0 - smoothstep(0.0, lwd, abs(rg - 6.62))) * 0.55;
            }
        }
    }

    if (!miss && tHit > 0.0) {
        float amp = clamp(log(R1 + 1.0) / 6.9, 0.15, 1.0);
        float fl = exp(-tHit * 2.2) * smoothstep(0.0, 0.05, tHit);
        vec3 vB = B - ro;
        float z = dot(vB, F);
        float lf = 0.0;
        if (z > 0.0) {
            vec2 sc = vec2(dot(vB, Rg) / z / (tf * asp) + cxN, -dot(vB, U) / z / tf);
            lf = exp(-length((inNdc - sc) * vec2(asp, 1.0)) * 5.0);
        }
        col += vec3(1.0, 0.93, 0.8) * fl * (0.4 * amp + 2.4 * lf * amp);
    }

    float mx = max(col.r, max(col.g, col.b));
    float ex = max(mx - 1.0, 0.0);
    col = col / (1.0 + 0.7 * ex) + vec3((1.0 - exp(-ex)) * 0.6);
    col *= mix(1.0, 0.4, smoothstep(p.f.w - 0.015, p.f.w + 0.015, inNdc.x));
    col += (h31(vec3(gl_FragCoord.xy, 1.0)) - 0.5) * 0.004;
    outColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
