#include "vk_app.h"
#include "lagrange_pen.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

const float GOLD[3] = {1.0f, 0.82f, 0.35f};
const float WHITE[3] = {0.92f, 0.94f, 1.0f};
const float DIM[3] = {0.6f, 0.66f, 0.8f};
const float TRACK[3] = {0.35f, 0.4f, 0.55f};

float sm(float x) {
    x = std::max(0.0f, std::min(1.0f, x));
    return x * x * (3.0f - 2.0f * x);
}

int fit(const LgPen& pen, const std::string& s, int sc) {
    while (sc > 1 && pen.width(s, sc) > 0.94f * pen.W) sc--;
    return sc;
}

void centred(LgPen& pen, const std::string& s, float y, int sc, const float c[3], float f) {
    sc = fit(pen, s, sc);
    float col[3] = {c[0] * f, c[1] * f, c[2] * f};
    pen.text(s, 0.5f * (pen.W - pen.width(s, sc)), y, sc, col);
}

}

void emitFutureSky(AppContext& app, OverlayVertex* out, uint32_t& used) {
    const FutureSkyState& S = app.fs;
    if (!S.ready) return;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float k = H / 720.0f;
    LgPen pen{out, used, W, H, {}};
    auto scl = [&](float base) { return std::max(1, (int)std::lround(base * k)); };
    FsParams P;
    fsEval(S.t, P);
    int stage = fsStage(S.t);

    pen.text("ANDROMEDA IN OUR FUTURE SKY", 16.0f * k, 14.0f * k, scl(2), DIM);
    char buf[64];
    if (S.t < 0.02f) std::snprintf(buf, sizeof buf, "NOW");
    else std::snprintf(buf, sizeof buf, "+%.2f BILLION YEARS", S.t);
    pen.text(buf, 16.0f * k, 38.0f * k, scl(4), GOLD);
    pen.text(S.playing ? "PLAYING" : "PAUSED", 16.0f * k + pen.width(buf, scl(4)) + 20.0f * k, 52.0f * k, scl(2), DIM);
    std::snprintf(buf, sizeof buf, "SUN BRIGHTNESS %d%% OF TODAY", (int)std::lround(P.lum * 100.0f));
    pen.text(buf, 16.0f * k, 76.0f * k, scl(2), WHITE);
    float sw[3] = {1.0f, 0.97f - 0.3f * P.sun, 0.85f - 0.6f * P.sun};
    float sx = 16.0f * k + pen.width(buf, scl(2)) + 14.0f * k;
    for (float i = 0.0f; i < 30.0f * k; i += 1.0f) pen.line(sx + i, 76.0f * k, sx + i, 91.0f * k, sw);
    pen.text("LEFT / RIGHT  SCRUB    SPACE  PLAY / PAUSE    SHIFT  FASTER    DRAG SLIDER    ESC  EXIT", 16.0f * k, 100.0f * k, scl(1.5f), DIM);

    float x0, x1, y;
    fsSlider(W, H, x0, x1, y);
    float xh = x0 + (x1 - x0) * S.t / FS_MAX_GYR;
    pen.line(x0, y, x1, y, TRACK);
    pen.line(x0, y + 1.0f, x1, y + 1.0f, TRACK);
    float lit[3] = {GOLD[0] * 0.8f, GOLD[1] * 0.8f, GOLD[2] * 0.8f};
    pen.line(x0, y, xh, y, lit);
    pen.line(x0, y + 1.0f, xh, y + 1.0f, lit);
    for (int i = 0; i < FS_STAGES; i++) {
        float tx = x0 + (x1 - x0) * FS_STAGE_T[i] / FS_MAX_GYR;
        pen.line(tx, y - 7.0f * k, tx, y + 8.0f * k, S.t >= FS_STAGE_T[i] - 1e-4f ? GOLD : TRACK);
    }
    for (float i = -4.0f * k; i <= 4.0f * k; i += 1.0f) pen.line(xh + i, y - 10.0f * k, xh + i, y + 11.0f * k, GOLD);
    struct Lab { int stage; const char* s; bool above; };
    const Lab labs[] = {{0, "NOW", false}, {1, "+2", false}, {2, "+3.75", true}, {4, "+4", false}, {5, "+5.1", true}, {6, "+7 GYR", false}};
    int ls = scl(1.5f);
    for (const Lab& l : labs) {
        float tx = x0 + (x1 - x0) * FS_STAGE_T[l.stage] / FS_MAX_GYR;
        pen.text(l.s, tx - 0.5f * pen.width(l.s, ls), l.above ? y - 30.0f * k : y + 12.0f * k, ls, DIM);
    }

    float t = S.t;
    float fade = stage == 0 ? 1.0f : sm(0.5f + (t - FS_STAGE_T[stage]) / 0.16f);
    centred(pen, fsStageHead(stage), H - 214.0f * k, scl(3), GOLD, fade);
    centred(pen, fsStageDetail(stage), H - 176.0f * k, scl(2), WHITE, fade);
    float starsF = sm((t - 3.6f) / 0.15f) * sm((5.8f - t) / 0.3f);
    if (starsF > 0.0f) centred(pen, "STARS ALMOST NEVER COLLIDE - THE GAPS ARE TOO HUGE", H - 144.0f * k, scl(2), WHITE, starsF);
    float sunF = sm((t - 1.0f) / 0.4f);
    if (sunF > 0.0f) centred(pen, "THE SUN WILL STILL BE SHINING - BUT EARTH WILL BE TOO HOT FOR OCEANS BY THEN", H - 118.0f * k, scl(2), sw, sunF);
    centred(pen, "NEW STUDIES GIVE ONLY ABOUT A 50% CHANCE THIS HAPPENS IN THE NEXT 10 BILLION YEARS", H - 92.0f * k, scl(2), DIM, 1.0f);

    uint32_t lineVerts = used;
    pen.flush();
    app.overlayLineVerts[app.currentFrame] = lineVerts;
    app.overlayPointVerts[app.currentFrame] = used - lineVerts;
}
