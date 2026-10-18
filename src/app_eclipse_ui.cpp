#include "vk_app.h"
#include "lagrange_pen.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

const float GOLD[3] = {1.0f, 0.82f, 0.35f};
const float WHITE[3] = {0.92f, 0.94f, 1.0f};
const float DIM[3] = {0.58f, 0.64f, 0.78f};
const float HEAD[3] = {0.5f, 0.68f, 1.0f};
const float RED[3] = {1.0f, 0.42f, 0.22f};
const float CYAN[3] = {0.45f, 0.9f, 1.0f};
const float UMB[3] = {0.45f, 0.55f, 0.85f};
const float PEN[3] = {0.8f, 0.62f, 0.3f};
const float ORBC[3] = {0.35f, 0.6f, 1.0f};
const float ECLC[3] = {0.4f, 0.42f, 0.5f};
const float NODEC[3] = {0.9f, 0.5f, 1.0f};
const float PI = 3.14159265f;

const char* FACTS[4] = {
    "THE SUN IS 400X WIDER THAN THE MOON BUT ALSO 400X FARTHER - A COSMIC COINCIDENCE",
    "THE MOON DRIFTS AWAY 3.8 CM/YEAR - IN ~600 MILLION YEARS TOTAL ECLIPSES END",
    "NEXT TOTAL SOLAR ECLIPSES: 2027-08-02 EGYPT, LONGEST OF THE CENTURY AT 6 MIN 23 S - 2028-07-22 AUSTRALIA",
    "NEXT TOTAL ECLIPSE FROM INDIA: 2034-03-20, TOTALITY CROSSES PARTS OF NORTH INDIA",
};

int fit(const LgPen& pen, const std::string& s, int sc, float maxW) {
    while (sc > 1 && pen.width(s, sc) > maxW) sc--;
    return sc;
}

void centered(LgPen& pen, const std::string& s, float cx, float y, int sc, float maxW, const float* c) {
    int f = fit(pen, s, sc, maxW);
    pen.text(s, cx - 0.5f * pen.width(s, f), y, f, c);
}

const char* phaseName(float p) {
    if (p < 0.04f || p >= 0.96f) return "NEW MOON";
    if (p < 0.21f) return "WAXING CRESCENT";
    if (p < 0.29f) return "FIRST QUARTER";
    if (p < 0.46f) return "WAXING GIBBOUS";
    if (p < 0.54f) return "FULL MOON";
    if (p < 0.71f) return "WANING GIBBOUS";
    if (p < 0.79f) return "THIRD QUARTER";
    return "WANING CRESCENT";
}

struct Proj {
    const EclipseState& S;
    LgPen& pen;
    float W, H;
    bool pt(const float* P, float& px, float& py) const {
        float x, y;
        if (!ecProject(S, P, x, y)) return false;
        px = (x * 0.5f + 0.5f) * W;
        py = (y * 0.5f + 0.5f) * H;
        return true;
    }
    void seg(const float* A, const float* B, const float* c, float f) const {
        float x0, y0, x1, y1;
        if (pt(A, x0, y0) && pt(B, x1, y1)) pen.line(x0, y0, x1, y1, c, f);
    }
};

void cone(const Proj& pr, const float* base, float r0, float len, float rate, const float* col, float f) {
    const int G = 6, RS = 28;
    auto at = [&](float d, float ang, float* o) {
        float r = std::max(0.0f, r0 * (1.0f + rate * d));
        o[0] = base[0] + d;
        o[1] = base[1] + r * std::cos(ang);
        o[2] = base[2] + r * std::sin(ang);
    };
    for (int g = 0; g < G; g++) {
        float ang = 2.0f * PI * (float)g / (float)G;
        float a[3], b[3];
        at(0.0f, ang, a);
        at(len, ang, b);
        pr.seg(a, b, col, f);
    }
    for (int k = 0; k < 2; k++) {
        float d = len * (float)k;
        for (int s = 0; s < RS; s++) {
            float a[3], b[3];
            at(d, 2.0f * PI * (float)s / (float)RS, a);
            at(d, 2.0f * PI * (float)(s + 1) / (float)RS, b);
            pr.seg(a, b, col, f * 0.8f);
        }
    }
}

void spaceLines(const Proj& pr, const EclipseState& S, const EcSpace& sp, float k, const float* nodeCol) {
    LgPen& pen = pr.pen;
    const int N = 96;
    float e1[3], e2[3];
    ecNodeAxes(S.sigma, e1, e2);
    for (int i = 0; i < N; i++) {
        float a0 = 2.0f * PI * (float)i / (float)N, a1 = 2.0f * PI * (float)(i + 1) / (float)N;
        float P0[3] = {EC_ORBIT_R * (std::cos(a0) * e1[0] + std::sin(a0) * e2[0]), 0, EC_ORBIT_R * (std::cos(a0) * e1[2] + std::sin(a0) * e2[2])};
        float P1[3] = {EC_ORBIT_R * (std::cos(a1) * e1[0] + std::sin(a1) * e2[0]), 0, EC_ORBIT_R * (std::cos(a1) * e1[2] + std::sin(a1) * e2[2])};
        pr.seg(P0, P1, ECLC, 0.7f);
        float ci = std::cos(EC_INCL), si = std::sin(EC_INCL);
        float Q0[3], Q1[3];
        for (int j = 0; j < 3; j++) {
            Q0[j] = EC_ORBIT_R * (std::cos(a0) * e1[j] + std::sin(a0) * ci * e2[j]);
            Q1[j] = EC_ORBIT_R * (std::cos(a1) * e1[j] + std::sin(a1) * ci * e2[j]);
        }
        Q0[1] += EC_ORBIT_R * std::sin(a0) * si;
        Q1[1] += EC_ORBIT_R * std::sin(a1) * si;
        pr.seg(Q0, Q1, ORBC, 1.0f);
    }
    float n0[3] = {-e1[0] * (EC_ORBIT_R + 2.6f), 0, -e1[2] * (EC_ORBIT_R + 2.6f)};
    float n1[3] = {e1[0] * (EC_ORBIT_R + 2.6f), 0, e1[2] * (EC_ORBIT_R + 2.6f)};
    pr.seg(n0, n1, nodeCol, 0.9f);
    float px, py;
    if (pr.pt(n1, px, py)) pen.text("NODE", px + 6.0f * k, py + 6.0f * k, std::max(1, (int)std::lround(1.6f * k)), nodeCol);
    if (pr.pt(n0, px, py)) pen.text("NODE", px - 6.0f * k - 4.0f * 6.0f * 1.6f * k, py + 6.0f * k, std::max(1, (int)std::lround(1.6f * k)), nodeCol);
    float drop[3] = {sp.moon[0], 0.0f, sp.moon[2]};
    pr.seg(sp.moon, drop, DIM, 0.8f);
    float ea[3] = {0, 0, 0};
    cone(pr, ea, EC_EARTH_R, 13.0f, -1.0f / EC_E_UMBRA_LEN, UMB, 0.55f);
    cone(pr, ea, EC_EARTH_R, 13.0f, 1.0f / EC_E_UMBRA_LEN, PEN, 0.5f);
    cone(pr, sp.moon, EC_MOON_R, EC_M_UMBRA_LEN, -1.0f / EC_M_UMBRA_LEN, UMB, 0.7f);
    cone(pr, sp.moon, EC_MOON_R, EC_M_UMBRA_LEN, 1.0f / EC_M_UMBRA_LEN, PEN, 0.5f);
}

void spaceLabels(const Proj& pr, const EcSpace& sp, float k) {
    LgPen& pen = pr.pen;
    int sc = std::max(1, (int)std::lround(1.8f * k));
    int sm = std::max(1, sc - 1);
    float px, py;
    float eu[3] = {0, -1.5f, 0};
    float me[3] = {sp.moon[0], sp.moon[1] + 0.6f, sp.moon[2]};
    float su[3] = {-EC_SUN_DIST, EC_SUN_R + 0.7f, 0};
    float pp[3] = {8.5f, 1.0f + 8.5f / EC_E_UMBRA_LEN + 0.5f, 0};
    if (pr.pt(eu, px, py)) pen.text("EARTH", px - 15.0f * sc, py, sc, WHITE);
    if (pr.pt(me, px, py)) pen.text("MOON", px - 12.0f * sc, py - 18.0f * k, sc, WHITE);
    if (pr.pt(su, px, py)) pen.text("SUN", px - 9.0f * sc, py - 6.0f * k, sc, GOLD);
    if (pr.pt(pp, px, py)) pen.text("EARTH PENUMBRA", px - 40.0f * sm, py - 6.0f * k, sm, PEN);
    float ue[3] = {4.5f, -(1.0f - 4.5f / EC_E_UMBRA_LEN) - 0.5f, 0};
    if (pr.pt(ue, px, py)) pen.text("EARTH UMBRA", px - 20.0f * sm, py + 4.0f * k, sm, UMB);
}

std::string wrapSplit(const LgPen& pen, std::string& s, int sc, float maxW) {
    if (pen.width(s, sc) <= maxW) return "";
    size_t cut = s.rfind(' ', s.size() / 2 + 8);
    if (cut == std::string::npos) return "";
    std::string rest = s.substr(cut + 1);
    s = s.substr(0, cut);
    return rest;
}

}

void emitEclipse(AppContext& app, OverlayVertex* out, uint32_t& used) {
    EclipseState& S = app.ec;
    if (!S.ready) return;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float k = H / 720.0f;
    S.aspect = W / H;
    ecCamera(S);
    LgPen pen{out, used, W, H, {}};
    auto scl = [&](float base) { return std::max(1, (int)std::lround(base * k)); };
    char buf[200];
    float freeW = W - 40.0f * k;
    float cx = 0.5f * W;
    if (S.view == 1) {
        float asp = W / H;
        float il = ((asp - 0.52f - 0.42f) / asp * 0.5f + 0.5f) * W;
        freeW = il - 30.0f * k;
        cx = 0.5f * freeW + 14.0f * k;
    }

    pen.text("ECLIPSES", 16.0f * k, 14.0f * k, scl(2.4f), GOLD);
    const char* vn[3] = {"1/3  SPACE VIEW - SHADOW CONES", "2/3  SOLAR ECLIPSE FROM THE GROUND", "3/3  LUNAR ECLIPSE"};
    pen.text(vn[S.view], 16.0f * k, 40.0f * k, scl(2.0f), WHITE);
    const char* hints[3] = {"TAB VIEW  LEFT/RIGHT MONTH  SPACE PLAY  N SEASON  DRAG TURN  ESC EXIT",
                            "TAB VIEW  LEFT/RIGHT TIME  SPACE PLAY  M PERIGEE/APOGEE  W SWAP  ESC EXIT",
                            "TAB VIEW  LEFT/RIGHT TIME  SPACE PLAY  ESC EXIT"};
    pen.text(hints[S.view], 16.0f * k, 66.0f * k, fit(pen, hints[S.view], scl(1.5f), freeW), DIM);

    std::string c1;
    const float* col1 = GOLD;
    float y = 92.0f * k;
    int ss = scl(1.8f);
    auto info = [&](const std::string& s, const float* c) {
        pen.text(s, 16.0f * k, y, fit(pen, s, ss, freeW * 0.5f), c);
        y += 20.0f * k;
    };

    if (S.view == 0) {
        EcSpace sp = ecSpace(S);
        Proj pr{S, pen, W, H};
        spaceLines(pr, S, sp, k, NODEC);
        spaceLabels(pr, sp, k);
        info(std::string("MOON PHASE: ") + phaseName(S.ph[0]), WHITE);
        std::snprintf(buf, sizeof buf, "DAY %.1f OF 29.5", S.ph[0] * 29.5f);
        info(buf, DIM);
        info(sp.height >= 0.0f ? "MOON IS ABOVE THE ECLIPTIC PLANE" : "MOON IS BELOW THE ECLIPTIC PLANE", ORBC);
        const char* seas[3] = {"ECLIPSE SEASON: NODE LINE POINTS AT THE SUN", "EDGE OF THE SEASON: PARTIAL ECLIPSES", "OFF SEASON: NODE LINE POINTS SIDEWAYS"};
        info(seas[S.season], NODEC);
        bool nearNew = S.ph[0] < 0.05f || S.ph[0] > 0.95f, nearFull = std::fabs(S.ph[0] - 0.5f) < 0.05f;
        if (sp.solar == 2) { c1 = "TOTAL SOLAR ECLIPSE - THE MOON'S UMBRA TOUCHES EARTH"; col1 = GOLD; }
        else if (sp.solar == 1) { c1 = "PARTIAL SOLAR ECLIPSE - ONLY THE PENUMBRA REACHES EARTH"; col1 = GOLD; }
        else if (sp.lunar == 3) { c1 = "TOTAL LUNAR ECLIPSE - THE MOON ENTERS EARTH'S UMBRA"; col1 = RED; }
        else if (sp.lunar == 2) { c1 = "PARTIAL LUNAR ECLIPSE - THE MOON GRAZES THE UMBRA"; col1 = RED; }
        else if (sp.lunar == 1) { c1 = "PENUMBRAL LUNAR ECLIPSE - A FAINT DIMMING"; col1 = RED; }
        else if (nearNew || nearFull) { c1 = "NO ECLIPSE - THE MOON PASSES ABOVE OR BELOW THE SHADOW"; col1 = DIM; }
        else { c1 = "ECLIPSES NEED A NEW OR FULL MOON AT THE NODE LINE"; col1 = DIM; }
        const char* note = "SCALE COMPRESSED. ORBIT TILT DRAWN 3X STEEPER THAN THE REAL 5.1 DEG";
        pen.text(note, 16.0f * k, H - 112.0f * k, fit(pen, note, scl(1.4f), freeW), DIM);
    } else if (S.view == 1) {
        EcGround g = ecGround(S);
        float cover = (1.0f - g.exposed) * 100.0f;
        std::snprintf(buf, sizeof buf, "TIME FROM MAXIMUM  %+.1f MIN", g.t);
        info(buf, WHITE);
        std::snprintf(buf, sizeof buf, "SUN COVERED  %.0f%%", cover);
        info(buf, GOLD);
        std::snprintf(buf, sizeof buf, "SUN %.2f DEG   MOON %.2f DEG", 2.0f * g.rs, 2.0f * g.rm);
        info(buf, DIM);
        info(S.apogee ? "MOON AT APOGEE - TOO SMALL TO COVER THE SUN" : "MOON AT PERIGEE - BIG ENOUGH FOR TOTALITY", S.apogee ? RED : CYAN);
        bool total = g.rm > g.rs;
        switch (g.stage) {
            case 0: c1 = g.t > 0.0f ? "THE ECLIPSE IS OVER" : "WAITING FOR FIRST CONTACT"; col1 = DIM; break;
            case 1: c1 = "PARTIAL PHASE - NEVER LOOK AT THE SUN WITHOUT ECLIPSE GLASSES"; col1 = WHITE; break;
            case 2: c1 = total ? "2ND CONTACT - BAILY'S BEADS AND THE DIAMOND RING" : "RING OF FIRE FORMING"; col1 = GOLD; break;
            case 4: c1 = total ? "3RD CONTACT - DIAMOND RING AND BAILY'S BEADS" : "RING OF FIRE BREAKING"; col1 = GOLD; break;
            case 3: c1 = "TOTALITY - THE CORONA, THE SKY DARK, STARS AND PLANETS OUT"; col1 = CYAN; break;
            default: c1 = "ANNULAR - THE RING OF FIRE: MOON TOO FAR TO COVER THE SUN"; col1 = RED; break;
        }
        float bx0 = 16.0f * k, bx1 = freeW + 14.0f * k, by = H - 124.0f * k;
        pen.line(bx0, by, bx1, by, DIM, 0.6f);
        float d1 = g.rs + g.rm, d2 = std::fabs(g.rm - g.rs);
        float ts[4] = {-d1 / EC_REL_SPEED, -d2 / EC_REL_SPEED, d2 / EC_REL_SPEED, d1 / EC_REL_SPEED};
        const char* names[4] = {"C1", "C2", "C3", "C4"};
        for (int i = 0; i < 4; i++) {
            float sv = (ts[i] < 0 ? -1.0f : 1.0f) * std::sqrt(std::fabs(ts[i]) / EC_SPAN_MIN);
            float x = bx0 + (bx1 - bx0) * (0.5f * (sv + 1.0f));
            pen.line(x, by - 5.0f * k, x, by + 5.0f * k, GOLD);
            pen.text(names[i], x - 6.0f * k, by - 18.0f * k, scl(1.4f), GOLD);
        }
        float mx = bx0 + (bx1 - bx0) * S.ph[1];
        pen.line(mx, by - 9.0f * k, mx, by + 9.0f * k, WHITE);
        pen.line(mx - 1.0f, by - 9.0f * k, mx - 1.0f, by + 9.0f * k, WHITE);

        float asp = W / H;
        float icx, icy, ir;
        if (!S.swap) {
            icx = ((asp - 0.52f) / asp * 0.5f + 0.5f) * W;
            icy = (0.34f * 0.5f + 0.5f) * H;
            ir = 0.42f * 0.5f * H;
        } else {
            icx = 0.5f * W;
            icy = 0.5f * H;
            ir = 0.98f * 0.5f * H;
        }
        pen.text(S.swap ? "FULL SKY VIEW" : "ALL-SKY VIEW - 360 DEG HORIZON", icx - 0.5f * pen.width(S.swap ? "FULL SKY VIEW" : "ALL-SKY VIEW - 360 DEG HORIZON", scl(1.3f)), icy - ir - 14.0f * k, scl(1.3f), DIM);
        float skyB = 0.03f + 0.97f * std::pow(g.exposed, 0.55f);
        if (skyB < 0.3f) {
            struct Pl { const char* n; float r, az; };
            const Pl pls[3] = {{"VENUS", 0.5778f, 0.5585f}, {"JUPITER", 0.3778f, -2.0944f}, {"MERCURY", 0.6889f, -0.7f}};
            float a = std::min(1.0f, (0.3f - skyB) / 0.2f);
            float lc[3] = {0.9f * a, 0.9f * a, 0.8f * a};
            for (const Pl& p : pls) {
                float px = icx + ir * p.r * std::sin(p.az);
                float py = icy - ir * p.r * std::cos(p.az);
                pen.text(p.n, px + 8.0f * k, py - 4.0f * k, scl(1.3f), lc);
            }
        }
    } else {
        EcLunar l = ecLunar(S);
        switch (l.stage) {
            case 0: c1 = "OUTSIDE EARTH'S SHADOW"; col1 = DIM; break;
            case 1: c1 = "PENUMBRAL PHASE - THE MOON DIMS ONLY SLIGHTLY"; col1 = WHITE; break;
            case 2: c1 = "PARTIAL PHASE - EARTH'S UMBRA TAKES A BITE"; col1 = RED; break;
            default: c1 = "BLOOD MOON - LIT BY EVERY SUNRISE AND SUNSET ON EARTH AT ONCE"; col1 = RED; break;
        }
        std::snprintf(buf, sizeof buf, "UMBRA %.1f MOON WIDTHS   PENUMBRA %.1f", EC_LUN_RU, EC_LUN_RP);
        info("EARTH'S SHADOW AT THE MOON", WHITE);
        info("UMBRA: 2.7 MOON WIDTHS ACROSS", UMB);
        info("PENUMBRA: 4.6 MOON WIDTHS ACROSS", PEN);
        float s = 15.0f * k;
        float dcx = W - 130.0f * k, dcy = H - 190.0f * k;
        pen.circle(dcx, dcy, EC_LUN_RP * s, PEN, 48);
        pen.circle(dcx, dcy, EC_LUN_RU * s, UMB, 48);
        pen.circle(dcx - l.xs * s, dcy + l.ys * s, s, WHITE, 24);
        pen.text("PENUMBRA", dcx - 24.0f * k, dcy - EC_LUN_RP * s - 14.0f * k, scl(1.2f), PEN);
        pen.text("UMBRA", dcx - 15.0f * k, dcy - 8.0f * k, scl(1.2f), UMB);
    }

    int idx = ((int)(S.clock / 7.0f)) % 4;
    if (S.view == 1 && (idx == 0)) idx = 0;
    std::string fact = FACTS[idx];
    int s1 = fit(pen, c1, scl(2.6f), freeW);
    centered(pen, c1, cx, H - 86.0f * k, s1, freeW, col1);
    int s2 = scl(1.6f);
    std::string rest = wrapSplit(pen, fact, s2, freeW);
    centered(pen, fact, cx, H - 50.0f * k, s2, freeW, DIM);
    if (!rest.empty()) centered(pen, rest, cx, H - 30.0f * k, s2, freeW, DIM);

    uint32_t lineVerts = used;
    pen.flush();
    app.overlayLineVerts[app.currentFrame] = lineVerts;
    app.overlayPointVerts[app.currentFrame] = used - lineVerts;
}
