#include "solarx.h"
#include "ephem.h"
#include "bodydata.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

struct Elem {
    const char* name;
    double aAu, e, inc, node, arg, T, periodDays;
    float col[3];
    const char* fact;
};

const Elem EL[3] = {
    {"HALLEY'S COMET", 17.834, 0.96714, 162.26, 58.42, 111.33, 2446470.5, 75.32 * 365.25, {0.5f, 0.85f, 1.0f},
     "HALLEY'S COMET RETURNS EVERY 76 YEARS - LAST SEEN IN 1986."},
    {"HALE-BOPP", 182.8, 0.995, 89.4, 282.5, 130.6, 2450539.5, 0.0, {0.6f, 0.8f, 1.0f},
     "HALE-BOPP WAS VISIBLE TO THE NAKED EYE FOR 18 MONTHS IN 1996-97. ITS NUCLEUS IS ABOUT 60 KM WIDE."},
    {"ENCKE", 2.2152, 0.8469, 11.8, 334.0, 186.5, 2460239.5, 0.0, {0.75f, 0.9f, 0.7f},
     "ENCKE HAS THE SHORTEST PERIOD OF ANY BRIGHT COMET AND FEEDS THE TAURID METEORS."}
};

const double HALLEY_NEXT_JD = 2474033.5;
const double HALLEY_FIRST_JD = 2446470.5;
const float WHITE[3] = {1.0f, 1.0f, 1.0f}, TEXT[3] = {0.85f, 0.87f, 0.92f}, GREEN[3] = {0.4f, 1.0f, 0.55f};
const float FACT[3] = {0.55f, 0.8f, 1.0f}, HINT[3] = {0.6f, 0.65f, 0.75f};

double periodDays(int k) {
    return EL[k].periodDays > 0.0 ? EL[k].periodDays : std::pow(EL[k].aAu, 1.5) * DAYS_PER_YEAR;
}

double gmTrue() {
    return (double)solarG(true) * 1000.0;
}

OrbitState elemState(int k, double jd) {
    double upa = solarUnitsPerAu(true);
    double M = 6.283185307179586 * (jd - EL[k].T) / periodDays(k);
    return ephemKeplerM(EL[k].aAu * upa, EL[k].e, EL[k].inc, EL[k].node, EL[k].arg, M, gmTrue());
}

float lenOf(const float v[3]) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

void crossf(const float a[3], const float b[3], float o[3]) {
    o[0] = a[1] * b[2] - a[2] * b[1];
    o[1] = a[2] * b[0] - a[0] * b[2];
    o[2] = a[0] * b[1] - a[1] * b[0];
}

void norm3(float v[3]) {
    float l = lenOf(v);
    if (l < 1e-20f) return;
    for (int a = 0; a < 3; a++) v[a] /= l;
}

float smooth(float a, float b, float x) {
    float t = std::max(0.0f, std::min(1.0f, (x - a) / (b - a)));
    return t * t * (3.0f - 2.0f * t);
}

void wrapText(std::vector<CardLine>& lines, const std::string& text, size_t width, const float c[3]) {
    std::string cur;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t sp = text.find(' ', pos);
        std::string word = text.substr(pos, sp == std::string::npos ? std::string::npos : sp - pos);
        pos = sp == std::string::npos ? text.size() : sp + 1;
        if (!cur.empty() && cur.size() + 1 + word.size() > width) {
            lines.push_back({cur, {c[0], c[1], c[2]}});
            cur.clear();
        }
        cur += (cur.empty() ? "" : " ") + word;
    }
    if (!cur.empty()) lines.push_back({cur, {c[0], c[1], c[2]}});
}

void drawTail(AppContext& app, OverlayVertex* out, uint32_t& used, const float vp[16], const float P[3], const float Vrel[3], const float S[3], double upa, float lit) {
    float d[3] = {P[0] - S[0], P[1] - S[1], P[2] - S[2]};
    float rU = lenOf(d);
    if (rU < 1e-6f || lit <= 0.001f) return;
    for (int a = 0; a < 3; a++) d[a] /= rU;
    float rAu = rU / (float)upa;
    float I = std::min(5.0f, 1.0f / (rAu * rAu));
    float fade = (1.0f - smooth(3.6f, 5.0f, rAu)) * lit;
    float px = 2.0f / (float)app.swapchainExtent.width, py = 2.0f / (float)app.swapchainExtent.height;
    float cx, cy;
    if (!sxProject(vp, P, cx, cy)) return;
    if (fade > 0.002f) {
        float bright = std::min(1.0f, 0.3f + 0.7f * std::sqrt(I / 2.0f)) * fade;
        float Li = I * (float)upa * fade, Ld = 0.5f * Li;
        float ref[3] = {0.0f, 1.0f, 0.0f};
        if (std::fabs(d[1]) > 0.9f) { ref[0] = 1.0f; ref[1] = 0.0f; }
        float u1[3], u2[3];
        crossf(d, ref, u1);
        norm3(u1);
        crossf(d, u1, u2);
        const float ionC[3] = {0.3f, 0.55f, 1.0f};
        for (int s = 0; s < 21 && used + 40 <= MAX_VERTS; s++) {
            float dir[3] = {d[0], d[1], d[2]};
            float w = 1.0f;
            if (s > 0) {
                float ang = 6.2831853f * (float)(s - 1) * 0.381966f, sp = 0.004f + 0.032f * (float)(s % 5 + 1) / 5.0f;
                for (int a = 0; a < 3; a++) dir[a] += (std::cos(ang) * u1[a] + std::sin(ang) * u2[a]) * sp;
                norm3(dir);
                w = 0.42f;
            }
            const int segs = 12;
            for (int j = 0; j < segs; j++) {
                float t0 = (float)j / segs, t1 = (float)(j + 1) / segs;
                float p0[3], p1[3], x0, y0, x1, y1;
                for (int a = 0; a < 3; a++) {
                    p0[a] = P[a] + dir[a] * Li * t0;
                    p1[a] = P[a] + dir[a] * Li * t1;
                }
                if (!sxProject(vp, p0, x0, y0) || !sxProject(vp, p1, x1, y1)) continue;
                float f0 = bright * w * std::pow(1.0f - t0, 1.2f), f1 = bright * w * std::pow(1.0f - t1, 1.2f);
                float c0[3] = {ionC[0] * f0, ionC[1] * f0, ionC[2] * f0}, c1[3] = {ionC[0] * f1, ionC[1] * f1, ionC[2] * f1};
                sxLine(out, used, x0, y0, x1, y1, c0, c1);
            }
        }
        float wv[3] = {-Vrel[0], -Vrel[1], -Vrel[2]};
        float vd = wv[0] * d[0] + wv[1] * d[1] + wv[2] * d[2];
        for (int a = 0; a < 3; a++) wv[a] -= vd * d[a];
        norm3(wv);
        float nrm[3];
        crossf(d, wv, nrm);
        const float dustC[3] = {1.0f, 0.9f, 0.62f};
        const int strands = 44;
        for (int s = 0; s < strands && used + 50 <= MAX_VERTS; s++) {
            float lam = 0.25f + 0.75f * (float)s / (strands - 1);
            float lat = (((s * 17) % strands) / (float)(strands - 1) - 0.5f) * 0.34f;
            const int segs = 16;
            for (int j = 0; j < segs; j++) {
                float t0 = (float)j / segs, t1 = (float)(j + 1) / segs;
                float p0[3], p1[3], x0, y0, x1, y1;
                for (int a = 0; a < 3; a++) {
                    p0[a] = P[a] + Ld * (t0 * d[a] + 0.9f * lam * t0 * t0 * wv[a] + lat * t0 * nrm[a]);
                    p1[a] = P[a] + Ld * (t1 * d[a] + 0.9f * lam * t1 * t1 * wv[a] + lat * t1 * nrm[a]);
                }
                if (!sxProject(vp, p0, x0, y0) || !sxProject(vp, p1, x1, y1)) continue;
                float b = bright * 0.42f;
                float f0 = b * std::pow(1.0f - t0, 1.3f), f1 = b * std::pow(1.0f - t1, 1.3f);
                float c0[3] = {dustC[0] * f0, dustC[1] * f0, dustC[2] * f0}, c1[3] = {dustC[0] * f1, dustC[1] * f1, dustC[2] * f1};
                sxLine(out, used, x0, y0, x1, y1, c0, c1);
            }
        }
    }
    float comaI = std::min(I, 5.0f) * lit;
    if (comaI > 0.08f && used + 900 <= MAX_VERTS) {
        float R = 4.0f + 11.0f * std::min(comaI, 4.0f);
        float core = 0.16f + 0.08f * std::min(comaI, 4.0f);
        const int spokes = 48;
        float c0[3] = {0.85f * core, 0.95f * core, 1.0f * core}, c1[3] = {0.0f, 0.0f, 0.0f};
        for (int s = 0; s < spokes; s++) {
            float ang = 6.2831853f * (float)s / spokes;
            sxLine(out, used, cx, cy, cx + std::cos(ang) * R * px, cy + std::sin(ang) * R * py, c0, c1);
        }
        for (int ring = 1; ring <= 10; ring++) {
            float f = (float)ring / 10.0f, rr = R * f, b = core * 0.9f * (1.0f - f) * std::sqrt(1.0f - f);
            float c[3] = {0.7f * b, 0.88f * b, 1.0f * b};
            for (int j = 0; j < 20; j++) {
                float a0 = 6.2831853f * (float)j / 20.0f, a1 = 6.2831853f * (float)(j + 1) / 20.0f;
                sxLine(out, used, cx + std::cos(a0) * rr * px, cy + std::sin(a0) * rr * py, cx + std::cos(a1) * rr * px, cy + std::sin(a1) * rr * py, c, c);
            }
        }
    }
}

}

bool sxProject(const float m[16], const float p[3], float& x, float& y) {
    float clip[4];
    for (int row = 0; row < 4; row++)
        clip[row] = m[row] * p[0] + m[row + 4] * p[1] + m[row + 8] * p[2] + m[row + 12];
    if (clip[3] <= 1e-9f) return false;
    x = clip[0] / clip[3];
    y = clip[1] / clip[3];
    return true;
}

void sxLine(OverlayVertex* out, uint32_t& used, float x0, float y0, float x1, float y1, const float c0[3], const float c1[3]) {
    if (used + 2 > MAX_VERTS) return;
    out[used++] = {x0, y0, c0[0], c0[1], c0[2]};
    out[used++] = {x1, y1, c1[0], c1[1], c1[2]};
}

bool cometState(const AppContext& app, int k, float pos[3], float vel[3]) {
    if (!app.displayValid || k < 1 || k > 2) return false;
    if (g_sx.gone) {
        float dt = (float)(app.simTime - g_sx.snapT);
        for (int a = 0; a < 3; a++) {
            pos[a] = g_sx.snapPos[k][a] + g_sx.snapVel[k][a] * dt;
            vel[a] = g_sx.snapVel[k][a];
        }
        return true;
    }
    OrbitState o = elemState(k, solarxJd(app));
    const Particle& s = app.displayBodies[0];
    for (int a = 0; a < 3; a++) {
        pos[a] = s.pos[a] + (float)o.pos[a];
        vel[a] = s.vel[a] + (float)o.vel[a];
    }
    return true;
}

void cometsSnapshot(const AppContext& app) {
    for (int k = 1; k <= 2; k++) cometState(app, k, g_sx.snapPos[k], g_sx.snapVel[k]);
}

void cometsLines(AppContext& app, OverlayVertex* out, uint32_t& used, const float vp[16], bool home) {
    if (!app.displayValid) return;
    const double upa = solarUnitsPerAu(app.trueScale);
    const Particle& sun = app.displayBodies[0];
    float S[3] = {sun.pos[0], sun.pos[1], sun.pos[2]};
    auto litAt = [&](const float P[3]) {
        if (!g_sx.gone) return 1.0f;
        float d[3] = {P[0] - S[0], P[1] - S[1], P[2] - S[2]};
        float travel = lenOf(d) / (float)upa * (float)SX_LIGHT_AU_SEC;
        return 1.0f - smooth(travel - 30.0f, travel, (float)solarxElapsed(app));
    };
    if (home && app.trueScale && !g_sx.gone) {
        double jdNow = solarxJd(app);
        (void)jdNow;
        const double up = solarUnitsPerAu(true);
        for (int k = 0; k < 3; k++) {
            const int N = 160;
            float px0 = 0, py0 = 0;
            bool prev = false;
            float c[3] = {EL[k].col[0] * 0.3f, EL[k].col[1] * 0.3f, EL[k].col[2] * 0.3f};
            for (int j = 0; j <= N; j++) {
                OrbitState o = ephemKeplerE(EL[k].aAu * up, EL[k].e, EL[k].inc, EL[k].node, EL[k].arg, 6.283185307179586 * j / N, gmTrue());
                float p[3] = {S[0] + (float)o.pos[0], S[1] + (float)o.pos[1], S[2] + (float)o.pos[2]};
                float x, y;
                bool ok = sxProject(vp, p, x, y);
                if (ok && prev) sxLine(out, used, px0, py0, x, y, c, c);
                px0 = x; py0 = y; prev = ok;
            }
        }
    }
    if (app.n > 14) {
        const Particle& h = app.displayBodies[14];
        float P[3] = {h.pos[0], h.pos[1], h.pos[2]};
        float V[3] = {h.vel[0] - sun.vel[0], h.vel[1] - sun.vel[1], h.vel[2] - sun.vel[2]};
        drawTail(app, out, used, vp, P, V, S, upa, litAt(P));
    }
    if (home && app.trueScale) {
        for (int k = 1; k <= 2; k++) {
            float P[3], V[3];
            if (!cometState(app, k, P, V)) continue;
            float Vrel[3] = {V[0] - sun.vel[0], V[1] - sun.vel[1], V[2] - sun.vel[2]};
            drawTail(app, out, used, vp, P, Vrel, S, upa, litAt(P));
            float x, y;
            if (sxProject(vp, P, x, y)) {
                float px = 2.0f / (float)app.swapchainExtent.width, py = 2.0f / (float)app.swapchainExtent.height;
                const float* c = EL[k].col;
                sxLine(out, used, x - 4.0f * px, y, x + 4.0f * px, y, c, c);
                sxLine(out, used, x, y - 4.0f * py, x, y + 4.0f * py, c, c);
            }
        }
    }
}

void cometsLabels(AppContext& app, std::vector<ScreenLabel>& labels, const float vp[16]) {
    for (int k = 1; k <= 2; k++) {
        float P[3], V[3], x, y;
        if (!cometState(app, k, P, V) || !sxProject(vp, P, x, y)) continue;
        ScreenLabel l{x, y, EL[k].name, {0.85f, 0.85f, 0.9f}};
        l.prio = 0.6f + 0.01f * (float)k;
        labels.push_back(l);
    }
}

void cometPeriodLines(const AppContext& app, int k, std::string& period, std::string& next) {
    double P = periodDays(k), jd = solarxJd(app);
    char buf[32];
    std::snprintf(buf, sizeof(buf), P / DAYS_PER_YEAR < 100.0 ? "%.1f YEARS" : "%.0f YEARS", P / DAYS_PER_YEAR);
    period = buf;
    double tn;
    if (k == 0) {
        tn = jd <= HALLEY_FIRST_JD + 1.0 ? HALLEY_FIRST_JD : HALLEY_NEXT_JD;
        while (tn < jd - 1.0) tn += P;
    } else {
        double n = std::max(0.0, std::ceil((jd - EL[k].T) / P));
        tn = EL[k].T + n * P;
    }
    next = solarxDate(tn);
}

std::vector<CardLine> cometCard(const AppContext& app, int k) {
    std::vector<CardLine> lines;
    lines.push_back({EL[k].name, {WHITE[0], WHITE[1], WHITE[2]}});
    lines.push_back({"COMET", {EL[k].col[0], EL[k].col[1], EL[k].col[2]}});
    std::string period, next;
    cometPeriodLines(app, k, period, next);
    lines.push_back({"PERIOD " + period, {TEXT[0], TEXT[1], TEXT[2]}});
    lines.push_back({"NEXT PERIHELION " + next, {GREEN[0], GREEN[1], GREEN[2]}});
    char buf[64];
    std::snprintf(buf, sizeof(buf), "ECCENTRICITY %.3f", EL[k].e);
    lines.push_back({buf, {TEXT[0], TEXT[1], TEXT[2]}});
    std::snprintf(buf, sizeof(buf), "CLOSEST %.3f AU", EL[k].aAu * (1.0 - EL[k].e));
    lines.push_back({buf, {TEXT[0], TEXT[1], TEXT[2]}});
    float P[3], V[3];
    if (cometState(app, k, P, V)) {
        const Particle& s = app.displayBodies[0];
        float d[3] = {P[0] - s.pos[0], P[1] - s.pos[1], P[2] - s.pos[2]};
        float v[3] = {V[0] - s.vel[0], V[1] - s.vel[1], V[2] - s.vel[2]};
        double upa = solarUnitsPerAu(true);
        std::snprintf(buf, sizeof(buf), "SUN DISTANCE %.*f AU", lenOf(d) / upa < 10.0 ? 2 : 1, lenOf(d) / upa);
        lines.push_back({buf, {GREEN[0], GREEN[1], GREEN[2]}});
        double kms = lenOf(v) * EARTH_ORBIT_KMS / (6.283185307179586 * upa / SX_YEAR_UNITS);
        std::snprintf(buf, sizeof(buf), "SPEED %.1f KM/S", kms);
        lines.push_back({buf, {GREEN[0], GREEN[1], GREEN[2]}});
    }
    wrapText(lines, std::string("FACT: ") + EL[k].fact, 32, FACT);
    lines.push_back({"ESC = FREE-FLY", {HINT[0], HINT[1], HINT[2]}});
    return lines;
}
