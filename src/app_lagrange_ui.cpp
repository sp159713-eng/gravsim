#include "lagrange_pen.h"
#include <algorithm>
#include <cstdio>

namespace {

const float SUN_C[3] = {1.0f, 0.82f, 0.3f};
const float EARTH_C[3] = {0.35f, 0.65f, 1.0f};
const float JUP_C[3] = {0.95f, 0.65f, 0.35f};
const float UNSTABLE_C[3] = {1.0f, 0.55f, 0.25f};
const float STABLE_C[3] = {0.4f, 1.0f, 0.5f};
const float WHITE[3] = {0.78f, 0.82f, 0.9f};
const float DIM[3] = {0.45f, 0.5f, 0.62f};
const float TITLE[3] = {0.6f, 0.8f, 1.0f};
const float ROCK_C[3] = {0.95f, 0.8f, 0.55f};
const float PALETTE[4][3] = {{0.5f, 0.95f, 1.0f}, {1.0f, 0.6f, 0.9f}, {0.7f, 1.0f, 0.5f}, {1.0f, 0.9f, 0.5f}};

void trail(LgPen& pen, const LgView& v, const LgBody& b, const float col[3], float strength) {
    int n = b.count;
    float px0 = 0.0f, py0 = 0.0f;
    for (int i = 0; i < n; i++) {
        int idx = (b.head - n + i + LG_TRAIL) % LG_TRAIL;
        float px, py;
        v.to(b.tx[idx], b.ty[idx], px, py);
        if (i > 0) {
            float f = strength * (0.2f + 0.8f * (float)i / (float)n);
            pen.line(px0, py0, px, py, col, f);
            pen.line(px0, py0 + 1.0f, px, py + 1.0f, col, f);
        }
        px0 = px;
        py0 = py;
    }
    float hx, hy;
    v.to(b.x, b.y, hx, hy);
    if (n > 0) pen.line(px0, py0, hx, hy, col, strength);
}

void halo(LgPen& pen, const LgView& v, double cx, double cy, const LgHalo& h, float t, const float col[3], float k) {
    const int N = 48;
    float lx = 0.0f, ly = 0.0f;
    for (int i = 0; i <= N; i++) {
        float a = 6.2831853f * (float)i / (float)N;
        LgHalo s = h;
        s.rate = 1.0f;
        s.phase = 0.0f;
        float dx, dy;
        lgHaloPoint(s, a, dx, dy);
        float px, py;
        v.to(cx + dx, cy + dy, px, py);
        if (i > 0) pen.line(lx, ly, px, py, col, 0.55f);
        lx = px;
        ly = py;
    }
    float dx, dy, px, py;
    lgHaloPoint(h, t, dx, dy);
    v.to(cx + dx, cy + dy, px, py);
    pen.dot(px, py, 2.5f * k, col);
    pen.dot(px, py, 1.0f * k, col);
}

}

void emitLagrange(AppContext& app, OverlayVertex* out, uint32_t& used) {
    LagrangeState& S = app.lg;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    LgView v = lgView(W, H);
    float k = v.k;
    int sc1 = std::max(1, (int)std::lround(k)), sc2 = std::max(1, (int)std::lround(2.0f * k));
    LgPen pen{out, used, W, H, {}};
    const float contC[3][3] = {{0.25f, 0.8f, 0.4f}, {0.85f, 0.6f, 0.2f}, {0.2f, 0.32f, 0.55f}};
    const float contF[3] = {0.55f, 0.5f, 0.6f};

    for (const LgSeg& g : S.contour) {
        float ax, ay, bx, by;
        v.to(g.x0, g.y0, ax, ay);
        v.to(g.x1, g.y1, bx, by);
        pen.line(ax, ay, bx, by, contC[g.cls], contF[g.cls]);
    }

    float sx, sy, ex, ey;
    v.to(-S.mu, 0.0, sx, sy);
    v.to(1.0 - S.mu, 0.0, ex, ey);
    for (int i = 0; i < 96; i += 2) {
        float a0 = 6.2831853f * (float)i / 96.0f, a1 = 6.2831853f * (float)(i + 1) / 96.0f;
        pen.line(sx + v.s * std::cos(a0), sy - v.s * std::sin(a0), sx + v.s * std::cos(a1), sy - v.s * std::sin(a1), DIM, 0.7f);
    }

    for (float r = 3.0f; r <= 14.0f * k; r += 2.0f) pen.circle(sx, sy, r, SUN_C, 24, 0.5f + 0.5f * r / (14.0f * k));
    const float* pc = S.jupiter ? JUP_C : EARTH_C;
    float pr = (S.jupiter ? 8.0f : 5.0f) * k;
    for (float r = 2.0f; r <= pr; r += 1.5f) pen.circle(ex, ey, r, pc, 20);

    for (int i = 0; i < 5; i++) {
        float px, py;
        v.to(S.pt[i][0], S.pt[i][1], px, py);
        const float* col = i < 3 ? UNSTABLE_C : STABLE_C;
        pen.circle(px, py, 6.0f * k, col, 20);
        pen.dot(px, py, 2.0f * k, col);
        if (i == S.hover || i == S.selected) pen.circle(px, py, 11.0f * k, WHITE, 24);
        char lab[4] = {'L', (char)('1' + i), 0, 0};
        float tx = px - 6.0f * (float)sc2, ty = py + 13.0f * k;
        if (i == 3) ty = py - 13.0f * k - 7.0f * (float)sc2;
        if (i == 0 && !S.jupiter) {
            tx = px - 6.0f * (float)sc2;
            ty = py + 26.0f * k;
        }
        if (S.jupiter && i < 2) {
            ty = py - 14.0f * k - 7.0f * (float)sc2;
            tx += i == 0 ? -10.0f * k : 10.0f * k;
        }
        pen.text(lab, tx, ty, sc2, col);
    }

    if (!S.jupiter) {
        float px, py;
        LgHalo jw = {0.022f, 0.062f, 0.35f, 2.0f, 0.0f};
        halo(pen, v, S.pt[1][0], S.pt[1][1], jw, (float)S.t, STABLE_C, k);
        v.to(S.pt[1][0], S.pt[1][1], px, py);
        pen.text("JWST", px + 20.0f * k, py - 4.0f * k, sc1, STABLE_C);
        LgHalo so = {0.02f, 0.05f, -0.3f, 2.0f, 1.5f}, ds = {0.011f, 0.026f, 0.5f, 2.0f, 3.5f};
        halo(pen, v, S.pt[0][0], S.pt[0][1], so, (float)S.t, ROCK_C, k);
        halo(pen, v, S.pt[0][0], S.pt[0][1], ds, (float)S.t, EARTH_C, k);
        v.to(S.pt[0][0], S.pt[0][1], px, py);
        pen.line(px - 5.0f * k, py - 12.0f * k, px - 30.0f * k, py - 36.0f * k, ROCK_C, 0.6f);
        pen.text("SOHO", px - 30.0f * k - pen.width("SOHO", sc1), py - 44.0f * k, sc1, ROCK_C);
        pen.line(px - 4.0f * k, py + 5.0f * k, px - 34.0f * k, py + 38.0f * k, EARTH_C, 0.6f);
        pen.text("DSCOVR", px - 34.0f * k - pen.width("DSCOVR", sc1), py + 38.0f * k, sc1, EARTH_C);
        trail(pen, v, S.tk7, ROCK_C, 0.5f);
        float tx, ty;
        v.to(S.tk7.x, S.tk7.y, tx, ty);
        pen.dot(tx, ty, 3.0f * k, ROCK_C);
        pen.dot(tx, ty, 1.0f * k, ROCK_C);
        pen.text("2010 TK7", tx + 12.0f * k, ty - 4.0f * k, sc1, ROCK_C);
    } else {
        for (const LgBody& b : S.swarm) {
            float px, py;
            v.to(b.x, b.y, px, py);
            pen.dot(px, py, 1.6f * k, ROCK_C);
        }
        float px, py;
        v.to(S.pt[3][0], S.pt[3][1], px, py);
        pen.text("GREEKS", px + 44.0f * k, py - 30.0f * k, sc1, ROCK_C);
        v.to(S.pt[4][0], S.pt[4][1], px, py);
        pen.text("TROJANS", px + 44.0f * k, py + 24.0f * k, sc1, ROCK_C);
    }

    for (size_t i = 0; i < S.tests.size(); i++) {
        const LgBody& b = S.tests[i];
        const float* col = PALETTE[i % 4];
        trail(pen, v, b, col, 0.9f);
        float px, py;
        v.to(b.x, b.y, px, py);
        pen.circle(px, py, 3.0f * k, col, 10);
        pen.dot(px, py, 1.0f * k, WHITE);
    }

    pen.text("SUN", sx - 9.0f * (float)sc1, sy + 18.0f * k, sc1, SUN_C);
    const char* pname = S.jupiter ? "JUPITER" : "EARTH";
    pen.text(pname, ex - 0.5f * pen.width(pname, sc1), ey + 13.0f * k, sc1, pc);

    float x0 = 14.0f * k, y = 14.0f * k;
    pen.text("LAGRANGE POINTS", x0, y, sc2 + 1, TITLE);
    y += 8.0f * (float)(sc2 + 1) + 8.0f * k;
    pen.text(S.jupiter ? "SUN + JUPITER   TAB: SUN + EARTH" : "SUN + EARTH   TAB: SUN + JUPITER", x0, y, sc2, WHITE);
    y += 8.0f * (float)sc2 + 5.0f * k;
    pen.text("CO-ROTATING VIEW: THE PLANET AND SUN STAND STILL, THE SPACE AROUND THEM SPINS", x0, y, sc1, WHITE);
    y += 8.0f * (float)sc1 + 5.0f * k;
    pen.text(S.jupiter ? "TRUE MASS RATIO (JUPITER = 1/1000 OF THE SUN). THE ASTEROID SWARMS ARE REAL TADPOLE / HORSESHOE ORBITS"
                       : "NOT TO SCALE: EARTH SHOWN 3300X TOO HEAVY AND TOO FAR OUT, SO L1 AND L2 ARE VISIBLE (REAL SCALE: INSET)", x0, y, sc1, UNSTABLE_C);
    y += 8.0f * (float)sc1 + 5.0f * k;
    pen.text("CONTOURS: ENERGY LANDSCAPE IN THE ROTATING FRAME - L4 / L5 ARE HILLTOPS, L1 / L2 / L3 ARE SADDLES", x0, y, sc1, DIM);

    lgEmitInset(app, pen, W, H);
    lgEmitCard(app, pen, W, H);

    char buf[160];
    std::snprintf(buf, sizeof(buf), "TEST PARTICLES: %d", (int)S.tests.size());
    pen.text(buf, 14.0f * k, H - 46.0f * k, sc1, DIM);
    const char* hint = S.paused ? "PAUSED - SPACE RESUME  R RESET  TAB SUN-EARTH / SUN-JUPITER  CLICK = DROP TEST PARTICLE  C CLEAR  HOVER A POINT"
                                : "SPACE PAUSE  R RESET  TAB SUN-EARTH / SUN-JUPITER  CLICK = DROP TEST PARTICLE  C CLEAR  HOVER A POINT";
    float hs = (float)fitScale(hint, W, sc1 + 0);
    pen.text(hint, 0.5f * (W - pen.width(hint, (int)hs)), H - 26.0f * k, (int)hs, DIM);

    uint32_t lineVerts = used;
    pen.flush();
    app.overlayLineVerts[app.currentFrame] = lineVerts;
    app.overlayPointVerts[app.currentFrame] = used - lineVerts;
}
