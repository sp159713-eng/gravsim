#include "vk_app.h"
#include "lagrange_pen.h"
#include "bhdisc.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

const float GOLD[3] = {1.0f, 0.82f, 0.4f};
const float WHITE[3] = {0.88f, 0.92f, 1.0f};
const float DIM[3] = {0.6f, 0.66f, 0.8f};
const float CYAN[3] = {0.45f, 0.85f, 1.0f};
const float ORANGE[3] = {1.0f, 0.6f, 0.3f};
const float RED[3] = {1.0f, 0.45f, 0.35f};
const float GRID[3] = {0.3f, 0.34f, 0.45f};

struct Cap {
    float t0;
    const char* text;
};

const Cap CAP_TDE[] = {
    {0.0f, "A STAR WANDERS TOO CLOSE TO A SUPERMASSIVE BLACK HOLE"},
    {5.0f, "NEAR THE BLACK HOLE GRAVITY PULLS HARDER ON THE NEAR SIDE OF THE STAR THAN THE FAR SIDE"},
    {8.5f, "A STAR TORN APART - SEEN BY TELESCOPES AS A TIDAL DISRUPTION EVENT (TDE)"},
    {14.0f, "ABOUT HALF THE STAR IS FLUNG AWAY - THE REST FALLS BACK IN A LONG THIN STREAM"},
    {20.0f, "THE STREAM CRASHES INTO ITSELF - THE COLLISIONS HEAT IT AND IT SETTLES INTO A RING"},
    {30.0f, "FRICTION SPREADS THE RING INTO A DISC - GAS NEAR THE HOLE ORBITS FASTER THAN GAS FAR OUT"},
    {48.0f, "MATTER FALLING IN GETS HOTTER THAN THE SUN'S CORE - BLACK HOLES ARE THE BRIGHTEST ENGINES IN THE UNIVERSE"},
    {78.0f, "PAST THE LAST STABLE ORBIT GAS PLUNGES IN - THE DISC SLOWLY SHRINKS AS IT IS EATEN"},
    {125.0f, "THE FEAST FADES ALONG A T TO THE POWER MINUS 5/3 CURVE - BRIGHT FOR MONTHS, DIM FOR YEARS"},
};

const Cap CAP_CLOUD[] = {
    {0.0f, "A GAS CLOUD DRIFTS TOWARD THE BLACK HOLE"},
    {6.0f, "TIDES STRETCH THE CLOUD INTO LONG STREAMERS AS IT FALLS PAST THE HOLE"},
    {16.0f, "THE STREAMERS COLLIDE WITH THEMSELVES AND SETTLE INTO A SPINNING DISC"},
    {40.0f, "MATTER FALLING IN GETS HOTTER THAN THE SUN'S CORE - BLACK HOLES ARE THE BRIGHTEST ENGINES IN THE UNIVERSE"},
    {80.0f, "PAST THE LAST STABLE ORBIT GAS PLUNGES IN - THE DISC SLOWLY SHRINKS AS IT IS EATEN"},
};

const Cap CAP_STREAM[] = {
    {0.0f, "A COMPANION STAR SPILLS GAS TOWARD THE BLACK HOLE - LIKE CYGNUS X-1"},
    {14.0f, "THE GAS CANNOT FALL STRAIGHT IN - IT WRAPS AROUND AND BUILDS A DISC"},
    {40.0f, "MATTER FALLING IN GETS HOTTER THAN THE SUN'S CORE - BLACK HOLES ARE THE BRIGHTEST ENGINES IN THE UNIVERSE"},
    {80.0f, "A STEADY SUPPLY FROM THE COMPANION KEEPS THIS DISC ALIVE FOR MILLIONS OF YEARS"},
};

const char* caption(const BdState& S) {
    const Cap* c = S.scen == 0 ? CAP_TDE : S.scen == 1 ? CAP_CLOUD : CAP_STREAM;
    int n = S.scen == 0 ? (int)(sizeof(CAP_TDE) / sizeof(Cap)) : S.scen == 1 ? (int)(sizeof(CAP_CLOUD) / sizeof(Cap)) : (int)(sizeof(CAP_STREAM) / sizeof(Cap));
    const char* out = c[0].text;
    for (int i = 0; i < n; i++)
        if (S.ts >= c[i].t0) out = c[i].text;
    return out;
}

float discPeak(const BdState& S, double tref) {
    float pk = 0.0f;
    int b1 = std::clamp((int)(S.ts / BD_BIN_S), 0, BD_BINS - 1);
    for (int b = std::clamp((int)(tref / BD_BIN_S), 0, BD_BINS - 1); b <= b1; b++)
        if (S.bins.cnt[b] > 0.0f) pk = std::max(pk, S.bins.disc[b] / S.bins.cnt[b]);
    return pk;
}

float binLum(const BdState& S, int b) {
    float acc = S.bins.eaten[b] / BD_BIN_S;
    float shock = S.bins.shock[b] / BD_BIN_S;
    return acc + 0.02f * shock;
}

float smoothLum(const BdState& S, int b, int hw) {
    float s = 0.0f;
    int n = 0;
    for (int i = std::max(0, b - hw); i <= std::min(BD_BINS - 1, b + hw); i++, n++) s += binLum(S, i);
    return n ? s / n : 0.0f;
}

void lightCurve(AppContext& app, LgPen& pen, float W, float H, float k, int sc) {
    const BdState& S = bdState();
    float x1 = W - 22.0f * k, x0 = x1 - 300.0f * k;
    float y1 = H - 185.0f * k, y0 = y1 - 175.0f * k;
    float px0 = x0 + 34.0f * k, py0 = y0 + 22.0f * k, px1 = x1 - 8.0f * k, py1 = y1 - 24.0f * k;
    pen.rect(x0, y0, x1, y1, GRID);
    pen.text("LIGHT CURVE", x0 + 6.0f * k, y0 + 5.0f * k, sc, CYAN);
    double t0 = S.scen == 0 ? 7.0 : S.scen == 1 ? 10.0 : 0.0;
    const double tLo = 1.0, tHi = 175.0;
    float lgLo = (float)std::log10(tLo), lgHi = (float)std::log10(tHi);
    auto X = [&](double t) { return px0 + (px1 - px0) * std::min(1.0f, ((float)std::log10(std::max(t - t0, tLo)) - lgLo) / (lgHi - lgLo)); };
    int bmax = std::clamp((int)(S.ts / BD_BIN_S), 0, BD_BINS - 1);
    float pk = 1e-6f;
    int bpk = 0;
    for (int b = 0; b <= bmax; b++) {
        float v = smoothLum(S, b, 3);
        if (v > pk) { pk = v; bpk = b; }
    }
    const float dLo = -3.0f, dHi = 0.25f;
    auto Y = [&](float v) {
        float d = std::clamp((float)std::log10(std::max(v / pk, 1e-6f)), dLo, dHi);
        return py1 - (py1 - py0) * (d - dLo) / (dHi - dLo);
    };
    for (int e = 0; e <= 3; e++) {
        float yy = Y(pk * std::pow(10.0f, -(float)e));
        pen.line(px0, yy, px1, yy, GRID, 0.4f);
    }
    for (double tt : {1.0, 10.0, 100.0}) {
        float xx = X(t0 + tt);
        pen.line(xx, py0, xx, py1, GRID, 0.4f);
        char b[16];
        std::snprintf(b, sizeof(b), "%d", (int)tt);
        pen.text(b, xx - 3.0f * k, py1 + 3.0f * k, 1, DIM);
    }
    if (S.scen != 2 && bmax > bpk) {
        double tpk = (bpk + 0.5) * BD_BIN_S - t0;
        if (tpk < 1.0) tpk = 1.0;
        float lx = 0, ly = 0;
        for (int i = 0; i <= 44; i++) {
            double tt = tpk * std::pow(tHi / tpk, i / 44.0);
            float v = pk * (float)std::pow(tt / tpk, -5.0 / 3.0);
            float xx = X(t0 + tt), yy = Y(v);
            if (i > 0 && (i & 1) == 0) pen.line(lx, ly, xx, yy, ORANGE, 0.8f);
            lx = xx; ly = yy;
        }
    }
    float lx = 0, ly = 0;
    bool have = false;
    for (int b = 0; b <= bmax; b++) {
        double t = (b + 0.5) * BD_BIN_S;
        if (t - t0 < tLo * 0.5 && S.scen != 2) continue;
        float v = smoothLum(S, b, 3);
        float xx = X(t), yy = Y(v);
        if (have) pen.line(lx, ly, xx, yy, WHITE, 1.0f);
        lx = xx; ly = yy;
        have = true;
    }
    if (have) pen.dot(lx, ly, 2.5f * k, GOLD);
    pen.text("BRIGHTNESS", x0 + 4.0f * k, y0 + 14.0f * k, 1, DIM);
    pen.text(S.scen == 2 ? "TIME (LOG)" : "TIME SINCE FLARE (LOG)", px0, y1 - 11.0f * k, 1, DIM);
    if (S.scen != 2) pen.text("DASHES: T TO THE MINUS 5/3 LAW", x0 + 6.0f * k, y1 + 5.0f * k, 1, ORANGE);
    (void)app;
}

}

void emitBhDisc(AppContext& app, OverlayVertex* out, uint32_t& used) {
    BdState& S = bdState();
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float k = H / 720.0f;
    float px = 2.0f / W, py = 2.0f / H;
    LgPen pen{out, used, W, H, {}};
    int s1 = std::max(1, (int)std::lround(k));
    int s2 = std::max(2, (int)std::lround(2.0f * k));

    const char* title = S.scen == 0 ? "BLACK HOLE FEEDING - A STAR TORN APART"
        : S.scen == 1 ? "BLACK HOLE FEEDING - A GAS CLOUD" : "BLACK HOLE FEEDING - A COMPANION STAR";
    pen.text(title, 14.0f * k, 12.0f * k, s2, CYAN);
    char buf[200];
    std::snprintf(buf, sizeof(buf), "TIME %5.1f S%s", S.ts, S.paused ? " - PAUSED" : "");
    pen.text(buf, 14.0f * k, 12.0f * k + 9.0f * (float)s2 + 4.0f * k, s1, DIM);

    double tref = S.scen == 2 ? 10.0 : 22.0;
    float pk = discPeak(S, tref);
    float eatenTot = 0.0f;
    for (int b = 0; b < BD_BINS; b++) eatenTot += S.bins.eaten[b];
    float eatPct = 100.0f * eatenTot / (float)BD_N;
    float discPct = pk > 0.0f && S.ts >= tref ? std::clamp(100.0f * S.discNow / pk, 0.0f, 100.0f) : 100.0f;
    if (S.scen == 2) std::snprintf(buf, sizeof(buf), "MASS EATEN %.0f%% OF A STAR - DISC STEADY AT %.0f%%", eatPct, discPct);
    else std::snprintf(buf, sizeof(buf), "MASS EATEN %.0f%% - DISC REMAINING %.0f%%", eatPct, discPct);
    if (S.ts >= tref) pen.text(buf, 14.0f * k, 12.0f * k + 9.0f * (float)s2 + 18.0f * k, s2, GOLD);

    lightCurve(app, pen, W, H, k, s1);

    uint32_t lineVerts = used;
    pen.flush();
    const char* cap = caption(S);
    bool gone = S.ts > 100.0 && discPct < 12.0f && S.scen != 2;
    int want = std::max(2, (int)std::lround(3.0f * k));
    std::string c1 = cap, c2;
    size_t cut = c1.find(" - ");
    if (cut != std::string::npos && fitScale(cap, W, want) < want) {
        c2 = c1.substr(cut + 3);
        c1 = c1.substr(0, cut) + " -";
    }
    if (c2.empty()) {
        bigText(out, used, c1.c_str(), W, H - 112.0f * k, px, py, fitScale(cap, W, want), GOLD[0], GOLD[1], GOLD[2]);
    } else {
        int s = std::min(fitScale(c1.c_str(), W, want), fitScale(c2.c_str(), W, want));
        bigText(out, used, c1.c_str(), W, H - 140.0f * k, px, py, s, GOLD[0], GOLD[1], GOLD[2]);
        bigText(out, used, c2.c_str(), W, H - 140.0f * k + 9.0f * (float)s + 6.0f * k, px, py, s, GOLD[0], GOLD[1], GOLD[2]);
    }
    if (gone) {
        const char* a = "THE DISC IS NEARLY GONE - THE BLACK HOLE GOES DARK AGAIN";
        const char* b = "FEED IT AGAIN:  1 = ANOTHER STAR   2 = A GAS CLOUD   3 = A COMPANION STAR STREAM";
        int as = fitScale(a, W, std::max(2, (int)std::lround(3.0f * k)));
        int bs = fitScale(b, W, std::max(2, (int)std::lround(3.0f * k)));
        bigText(out, used, a, W, H - 78.0f * k, px, py, as, 1.0f, 0.55f, 0.4f);
        bigText(out, used, b, W, H - 48.0f * k, px, py, bs, 0.6f, 0.9f, 1.0f);
    } else {
        const char* hint = S.paused ? "PAUSED - SPACE RESUME  LEFT / RIGHT SCRUB  R RESTART  1 STAR  2 GAS CLOUD  3 COMPANION STREAM  DRAG ORBIT  SCROLL ZOOM"
                                    : "SPACE PAUSE  LEFT / RIGHT SCRUB  R RESTART  1 STAR  2 GAS CLOUD  3 COMPANION STREAM  DRAG ORBIT  SCROLL ZOOM";
        int hs = fitScale(hint, W, std::max(1, (int)std::lround(k)));
        float hl = (float)std::strlen(hint) * 6.0f * (float)hs;
        emitText(out, used, hint, -1.0f + 0.5f * (W - hl) * px, -1.0f + (H - 26.0f * k) * py, px, py, (float)hs, 0.65f, 0.7f, 0.8f);
    }
    if (S.feedFlash > 0.0f) {
        const char* f = S.scen == 0 ? "FEEDING: ANOTHER STAR" : S.scen == 1 ? "FEEDING: A GAS CLOUD" : "FEEDING: A STEADY STREAM FROM A COMPANION";
        bigText(out, used, f, W, 0.18f * H, px, py, fitScale(f, W, std::max(3, (int)std::lround(4.0f * k))), 1.0f, 0.9f, 0.5f);
    }
    app.overlayLineVerts[app.currentFrame] = lineVerts;
    app.overlayPointVerts[app.currentFrame] = used - lineVerts;
}
