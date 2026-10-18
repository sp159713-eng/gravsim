#include "vk_app.h"
#include "lagrange_pen.h"
#include "expansion.h"
#include "cosmo.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

const float GOLD[3] = {1.0f, 0.82f, 0.35f};
const float WHITE[3] = {0.92f, 0.94f, 1.0f};
const float DIM[3] = {0.62f, 0.68f, 0.82f};
const float CYAN[3] = {0.45f, 0.9f, 1.0f};
const float RED[3] = {1.0f, 0.5f, 0.35f};
const float GREEN[3] = {0.5f, 1.0f, 0.6f};

struct Ctx {
    LgPen& pen;
    ExTexts& tx;
    float W, H, k;
    double t, a;
    int era;
    float clock;
};

float fit(const std::string& s, float sc, float maxW) {
    float w = (float)s.size() * 6.0f * sc;
    if (w > maxW) sc *= maxW / w;
    return std::max(sc, 0.8f);
}

void txt(Ctx& c, const std::string& s, float x, float y, float sc, const float* col) { c.tx.add(s, x, y, sc, col); }

void centered(Ctx& c, const std::string& s, float cx, float y, float sc, const float* col) {
    float f = fit(s, sc, c.W - 30.0f * c.k);
    txt(c, s, cx - 0.5f * c.tx.width(s, f), y, f, col);
}

float scl(const Ctx& c, float base) { return std::max(1.0f, base * c.k); }

unsigned hash2(int i, int j) {
    unsigned h = (unsigned)(i * 73856093) ^ (unsigned)(j * 19349663);
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}

float unit(unsigned h, int shift) { return (float)((h >> shift) & 0xFFu) / 255.0f; }

void blip(LgPen& pen, float x, float y, float r, const float* c, float f = 1.0f) {
    pen.line(x - r, y, x + r, y, c, f);
    pen.line(x, y - r, x, y + r, c, f);
}

void ellipse(LgPen& pen, float cx, float cy, float rx, float ry, const float* col, float f) {
    const int seg = 10;
    for (int i = 0; i < seg; i++) {
        float a0 = 6.2831853f * (float)i / seg, a1 = 6.2831853f * (float)(i + 1) / seg;
        pen.line(cx + rx * std::cos(a0), cy + ry * std::sin(a0), cx + rx * std::cos(a1), cy + ry * std::sin(a1), col, f);
    }
}

void readouts(Ctx& c) {
    float k = c.k;
    double T = cosmo::tempK(c.a), Hb = cosmo::hubble(c.a), q = cosmo::decel(c.a);
    float x = c.W - 392.0f * k;
    float s = scl(c, 1.7f);
    float y = 12.0f * k, dy = 19.0f * k;
    char buf[96];
    std::string aStr = exFmtScale(c.a);
    std::snprintf(buf, sizeof buf, "SCALE FACTOR A = %s", aStr.c_str());
    txt(c, buf, x, y, s, GOLD);
    txt(c, "AGE " + exFmtAge(c.t), x, y + dy, s, WHITE);
    txt(c, "TEMPERATURE " + exFmtTemp(T), x, y + 2 * dy, s, RED);
    txt(c, "HUBBLE RATE " + exFmtHubble(Hb), x, y + 3 * dy, s, CYAN);
    double z = 1.0 / c.a - 1.0;
    if (c.a < 0.9995) std::snprintf(buf, sizeof buf, "REDSHIFT Z = %.4g  (SPACE %.4g X SMALLER)", z, 1.0 / c.a);
    else if (c.a < 1.0005) std::snprintf(buf, sizeof buf, "REDSHIFT Z = 0  (TODAY)");
    else std::snprintf(buf, sizeof buf, "FUTURE: SPACE IS %.2f X BIGGER THAN TODAY", c.a);
    txt(c, buf, x, y + 4 * dy, scl(c, 1.4f), DIM);
    const char* mode;
    const float* mc;
    if (c.t >= 1e-36 && c.t < 1e-32) {
        mode = "ACCELERATING - INFLATION";
        mc = GOLD;
    } else if (q > 0.0) {
        mode = "DECELERATING - GRAVITY IS WINNING";
        mc = CYAN;
    } else {
        mode = "ACCELERATING - DARK ENERGY IS WINNING";
        mc = GREEN;
    }
    txt(c, mode, x, y + 4 * dy + 20.0f * k, scl(c, 1.5f), mc);
}

void graph(Ctx& c) {
    float k = c.k;
    float x0 = 430.0f * k, x1 = x0 + 330.0f * k, y0 = 28.0f * k, y1 = 108.0f * k;
    const float AX[3] = {0.35f, 0.4f, 0.55f};
    const float CUR[3] = {0.4f, 0.8f, 1.0f};
    c.pen.rect(x0, y0, x1, y1, AX);
    txt(c, "A(T) - SCALE FACTOR, 0 TO 50 BILLION YEARS", x0, 12.0f * k, scl(c, 1.2f), DIM);
    const double amax = 9.0;
    auto px = [&](double tg) { return x0 + (x1 - x0) * (float)(tg / EX_TEND_GYR); };
    auto py = [&](double a) { return y1 - (y1 - y0) * (float)std::min(a, amax) / (float)amax; };
    float ya1 = py(1.0);
    const float G1[3] = {0.25f, 0.3f, 0.4f};
    c.pen.line(x0, ya1, x1, ya1, G1);
    double tn = cosmo::nowSec() / cosmo::GYR_S;
    c.pen.line(px(tn), y0, px(tn), y1, G1);
    txt(c, "NOW", px(tn) + 3.0f * k, y0 + 2.0f * k, scl(c, 1.1f), DIM);
    double tt = cosmo::timeAt(cosmo::turnA()) / cosmo::GYR_S;
    const float TP[3] = {1.0f, 0.9f, 0.4f};
    c.pen.line(px(tt), y1 - 6.0f * k, px(tt), y1 + 4.0f * k, TP);
    txt(c, "SPEED-UP BEGINS", px(tt) - 20.0f * k, y1 + 6.0f * k, scl(c, 1.1f), TP);
    float lx = px(0.0), ly = py(0.0);
    const int N = 120;
    for (int i = 1; i <= N; i++) {
        double tg = EX_TEND_GYR * i / N;
        float nx = px(tg), ny = py(cosmo::scaleAt(tg * cosmo::GYR_S));
        double mid = tg - 0.5 * EX_TEND_GYR / N;
        bool acc = cosmo::decel(cosmo::scaleAt(mid * cosmo::GYR_S)) <= 0.0;
        const float DEC[3] = {0.4f, 0.7f, 1.0f}, ACC[3] = {0.5f, 1.0f, 0.6f};
        c.pen.line(lx, ly, nx, ny, acc ? ACC : DEC);
        lx = nx;
        ly = ny;
    }
    double tg = std::min(c.t / cosmo::GYR_S, EX_TEND_GYR);
    c.pen.circle(px(tg), py(c.a), 3.5f * k, CUR, 12);
    blip(c.pen, px(tg), py(c.a), 5.5f * k, CUR);
}

void grid(Ctx& c) {
    float k = c.k;
    float gt = 140.0f * k, gb = c.H - 264.0f * k, cx = 0.5f * c.W, cy = 0.5f * (gt + gb);
    const float S0 = 46.0f * k;
    float sp = S0 * (float)c.a;
    const float GH[3] = {0.4f, 0.42f, 0.55f};
    float hw = 13.5f * S0, hh = 3.5f * S0, br = 12.0f * k;
    float fx0 = cx - hw, fx1 = cx + hw, fy0 = cy - hh, fy1 = cy + hh;
    float fl = 0.55f;
    c.pen.line(fx0, fy0, fx0 + br, fy0, GH, fl); c.pen.line(fx0, fy0, fx0, fy0 + br, GH, fl);
    c.pen.line(fx1, fy0, fx1 - br, fy0, GH, fl); c.pen.line(fx1, fy0, fx1, fy0 + br, GH, fl);
    c.pen.line(fx0, fy1, fx0 + br, fy1, GH, fl); c.pen.line(fx0, fy1, fx0, fy1 - br, GH, fl);
    c.pen.line(fx1, fy1, fx1 - br, fy1, GH, fl); c.pen.line(fx1, fy1, fx1, fy1 - br, GH, fl);
    txt(c, "SIZE TODAY (A = 1)", fx0 + 4.0f * k, fy0 - 11.0f * k, scl(c, 1.1f), GH);

    int stage = c.t < 3.79e5 * cosmo::YR_S ? 0 : c.t < 1e8 * cosmo::YR_S ? 1 : c.t < 2e8 * cosmo::YR_S ? 2 : 3;
    double gyr = c.t / cosmo::GYR_S;
    float born = std::clamp((float)((gyr - 0.2) / 0.8), 0.0f, 1.0f);
    float fade = 1.0f / (1.0f + std::max(0.0f, (float)c.a - 2.0f) / 1.2f);
    fade *= fade;
    float starF = std::clamp((float)((gyr - 0.1) / 0.1), 0.0f, 1.0f);
    for (int i = -13; i <= 13; i++) {
        for (int j = -3; j <= 3; j++) {
            if (i == 0 && j == 0) continue;
            unsigned h = hash2(i, j);
            float jx = (unit(h, 0) - 0.5f) * 0.45f, jy = (unit(h, 8) - 0.5f) * 0.45f;
            float x = cx + ((float)i + jx) * sp, y = cy + ((float)j + jy) * sp;
            if (x < 6.0f * k || x > c.W - 6.0f * k || y < gt || y > gb) continue;
            if (stage == 0) {
                float tw = 0.55f + 0.45f * std::sin(c.clock * 5.0f + unit(h, 16) * 6.28f);
                const float PL[3] = {1.0f, 0.85f, 0.6f};
                blip(c.pen, x, y, 1.3f * k, PL, tw);
            } else if (stage == 1) {
                const float DK[3] = {0.42f, 0.32f, 0.75f};
                blip(c.pen, x, y, 1.3f * k, DK, 0.8f);
            } else if (stage == 2) {
                const float ST[3] = {0.7f, 0.85f, 1.0f};
                if (unit(h, 16) < 0.6f) blip(c.pen, x, y, 1.4f * k, ST, starF);
                else blip(c.pen, x, y, 1.0f * k, ST, 0.25f);
            } else {
                float t1 = unit(h, 16);
                float col[3] = {0.65f + 0.35f * t1, 0.8f + 0.05f * t1, 1.0f - 0.45f * t1};
                float r = std::clamp(sp * 0.12f, 1.4f * k, 4.5f * k) * (0.4f + 0.6f * born);
                float f = (0.4f + 0.6f * born) * fade;
                if (f < 0.02f) continue;
                if (sp > 22.0f * k) ellipse(c.pen, x, y, r * 1.5f, r, col, f);
                blip(c.pen, x, y, r * 0.8f, col, f);
            }
        }
    }
    if (stage == 3) {
        const float LG[3] = {1.0f, 0.9f, 0.55f}, LG2[3] = {0.7f, 0.85f, 1.0f};
        c.pen.circle(cx, cy, 3.6f * k, LG, 12);
        blip(c.pen, cx, cy, 2.0f * k, LG);
        blip(c.pen, cx + 8.0f * k, cy - 4.0f * k, 1.6f * k, LG2);
        blip(c.pen, cx - 5.0f * k, cy + 7.0f * k, 1.3f * k, LG2);
        txt(c, c.a > 2.0 ? "LOCAL GROUP - STAYS BOUND" : "MILKY WAY", cx + 14.0f * k, cy + 6.0f * k, scl(c, 1.2f), LG);
    }
    char buf[96];
    if (c.a < 0.1) {
        std::snprintf(buf, sizeof buf, "THE GRID IS TO SCALE - NOW ONLY %.3g%% OF ITS SIZE TODAY", c.a * 100.0);
        centered(c, buf, cx, gb - 16.0f * k, scl(c, 1.5f), WHITE);
        float rr = std::max(2.5f * k, sp * 14.0f);
        const float HOT[3] = {1.0f, 0.9f, 0.7f};
        c.pen.circle(cx, cy, rr, HOT, 16);
    }
}

void caption(Ctx& c) {
    const ExEra& e = exEras()[c.era];
    float k = c.k;
    float cx = 0.5f * c.W;
    float y = c.H - 258.0f * k;
    const float TIT[3] = {1.0f, 0.85f, 0.45f};
    centered(c, e.title, cx, y, scl(c, 2.3f), TIT);
    centered(c, e.sub1, cx, y + 26.0f * k, scl(c, 1.55f), WHITE);
    centered(c, e.sub2, cx, y + 45.0f * k, scl(c, 1.55f), DIM);
}

}

void emitExpansion(AppContext& app, OverlayVertex* out, uint32_t& used) {
    ExpansionState& S = exState();
    if (!S.ready) return;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float k = H / 720.0f;
    LgPen pen{out, used, W, H, {}};
    ExTexts tx;
    Ctx c{pen, tx, W, H, k, S.tSec, cosmo::scaleAt(S.tSec), exEraAt(S.tSec), S.clock};
    txt(c, "EXPANSION OF THE UNIVERSE", 16.0f * k, 12.0f * k, scl(c, 2.3f), GOLD);
    txt(c, "FLAT LCDM: H0 67.7  OMEGA-M 0.31  OMEGA-L 0.69", 16.0f * k, 40.0f * k, scl(c, 1.35f), DIM);
    txt(c, "A GRID OF GALAXIES: SPACING = SCALE FACTOR A(T)", 16.0f * k, 58.0f * k, scl(c, 1.35f), DIM);
    txt(c, "LEFT/RIGHT SCRUB  UP/DOWN ERA  SPACE PLAY  N NOW  ESC EXIT", 16.0f * k, 78.0f * k, scl(c, 1.2f), WHITE);
    readouts(c);
    graph(c);
    grid(c);
    caption(c);
    exEmitBar(pen, tx, W, H, k, S.tSec);
    uint32_t lineVerts = used;
    tx.flush(out, used, W, H);
    app.overlayLineVerts[app.currentFrame] = lineVerts;
    app.overlayPointVerts[app.currentFrame] = used - lineVerts;
}
