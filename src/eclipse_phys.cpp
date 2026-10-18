#include "vk_app.h"
#include <algorithm>
#include <cmath>

namespace {

const float PI = 3.14159265f;

float dot3(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

void cross3(const float* a, const float* b, float* o) {
    o[0] = a[1] * b[2] - a[2] * b[1];
    o[1] = a[2] * b[0] - a[0] * b[2];
    o[2] = a[0] * b[1] - a[1] * b[0];
}

void unit(float* v) {
    float l = std::sqrt(dot3(v, v));
    if (l > 1e-12f) { v[0] /= l; v[1] /= l; v[2] /= l; }
}

float lens(float d, float r1, float r2) {
    if (d >= r1 + r2) return 0.0f;
    if (d <= std::fabs(r1 - r2)) {
        float m = std::min(r1, r2);
        return PI * m * m;
    }
    float a1 = std::acos(std::max(-1.0f, std::min(1.0f, (d * d + r1 * r1 - r2 * r2) / (2.0f * d * r1))));
    float a2 = std::acos(std::max(-1.0f, std::min(1.0f, (d * d + r2 * r2 - r1 * r1) / (2.0f * d * r2))));
    float k = (-d + r1 + r2) * (d + r1 - r2) * (d - r1 + r2) * (d + r1 + r2);
    return r1 * r1 * a1 + r2 * r2 * a2 - 0.5f * std::sqrt(std::max(0.0f, k));
}

}

void ecNodeAxes(float sigma, float* e1, float* e2) {
    float cs = std::cos(sigma), sn = std::sin(sigma);
    e1[0] = -cs; e1[1] = 0.0f; e1[2] = -sn;
    e2[0] = -sn; e2[1] = 0.0f; e2[2] = cs;
}

void ecMoonPos(float sigma, float phase, float* out) {
    float e1[3], e2[3];
    ecNodeAxes(sigma, e1, e2);
    float th = sigma + 2.0f * PI * phase;
    float c = std::cos(th), s = std::sin(th);
    float ci = std::cos(EC_INCL), si = std::sin(EC_INCL);
    for (int i = 0; i < 3; i++) out[i] = EC_ORBIT_R * (c * e1[i] + s * ci * e2[i]);
    out[1] += EC_ORBIT_R * s * si;
}

EcSpace ecSpace(const EclipseState& S) {
    EcSpace r{};
    ecMoonPos(S.sigma, S.ph[0], r.moon);
    r.height = r.moon[1];
    const float A[3] = {1, 0, 0};
    float d = dot3(r.moon, A);
    float md = -d;
    if (md > 0.0f) {
        float rel[3] = {-r.moon[0] - A[0] * md, -r.moon[1] - A[1] * md, -r.moon[2] - A[2] * md};
        float perp = std::sqrt(dot3(rel, rel));
        float ru = EC_MOON_R * (1.0f - md / EC_M_UMBRA_LEN);
        float rp = EC_MOON_R * (1.0f + md / EC_M_UMBRA_LEN);
        if (perp < EC_EARTH_R && ru > 0.0f) r.solar = 2;
        else if (perp < EC_EARTH_R + rp) r.solar = 1;
    }
    if (d > 0.0f) {
        float rel[3] = {r.moon[0] - A[0] * d, r.moon[1] - A[1] * d, r.moon[2] - A[2] * d};
        float perp = std::sqrt(dot3(rel, rel));
        float ru = EC_EARTH_R * (1.0f - d / EC_E_UMBRA_LEN);
        float rp = EC_EARTH_R * (1.0f + d / EC_E_UMBRA_LEN);
        if (perp + EC_MOON_R <= ru) r.lunar = 3;
        else if (perp - EC_MOON_R < ru) r.lunar = 2;
        else if (perp - EC_MOON_R < rp) r.lunar = 1;
    }
    return r;
}

EcGround ecGround(const EclipseState& S) {
    EcGround g{};
    float s = 2.0f * S.ph[1] - 1.0f;
    g.t = EC_SPAN_MIN * s * std::fabs(s);
    g.rs = EC_SUN_RAD_DEG;
    g.rm = S.apogee ? EC_MOON_APO_DEG : EC_MOON_PERI_DEG;
    g.sepX = -EC_REL_SPEED * g.t;
    g.sepY = 0.003f;
    g.d = std::sqrt(g.sepX * g.sepX + g.sepY * g.sepY);
    float ov = lens(g.d, g.rs, g.rm);
    g.exposed = std::max(0.0f, 1.0f - ov / (PI * g.rs * g.rs));
    float margin = std::fabs(g.rm - g.rs);
    bool total = g.rm > g.rs;
    if (g.d >= g.rs + g.rm) g.stage = 0;
    else if (g.d > margin + 0.012f) g.stage = 1;
    else if (g.d > margin) g.stage = g.t < 0.0f ? 2 : 4;
    else g.stage = total ? 3 : 5;
    return g;
}

EcLunar ecLunar(const EclipseState& S) {
    EcLunar l{};
    l.xs = (2.0f * S.ph[2] - 1.0f) * 8.0f;
    l.ys = EC_LUN_B;
    l.dd = std::sqrt(l.xs * l.xs + l.ys * l.ys);
    if (l.dd - 1.0f >= EC_LUN_RP) l.stage = 0;
    else if (l.dd - 1.0f >= EC_LUN_RU) l.stage = 1;
    else if (l.dd + 1.0f > EC_LUN_RU) l.stage = 2;
    else l.stage = 3;
    return l;
}

void ecCamera(EclipseState& S) {
    float ce = std::cos(S.el), se = std::sin(S.el);
    float tgt[3] = {-2.5f, 0.0f, 0.0f};
    float u[3] = {std::sin(S.az) * ce, se, std::cos(S.az) * ce};
    for (int i = 0; i < 3; i++) {
        S.camPos[i] = tgt[i] + S.dist * u[i];
        S.F[i] = -u[i];
    }
    const float up[3] = {0, 1, 0};
    cross3(S.F, up, S.R);
    unit(S.R);
    cross3(S.R, S.F, S.U);
    unit(S.U);
}

bool ecProject(const EclipseState& S, const float* P, float& x, float& y) {
    float v[3] = {P[0] - S.camPos[0], P[1] - S.camPos[1], P[2] - S.camPos[2]};
    float z = dot3(v, S.F);
    if (z < 1e-3f) return false;
    x = dot3(v, S.R) / z / (S.tanf * S.aspect);
    y = -dot3(v, S.U) / z / S.tanf;
    return true;
}
