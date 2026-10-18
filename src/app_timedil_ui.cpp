#include "vk_app.h"
#include "timedil.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

const float PI2 = 6.2831853f;

struct Pen {
    OverlayVertex* out;
    uint32_t& used;
    float W, H;

    void line(float x0, float y0, float x1, float y1, const float c[3]) {
        if (used + 2 > MAX_VERTS - 12000) return;
        out[used++] = {-1.0f + 2.0f * x0 / W, -1.0f + 2.0f * y0 / H, c[0], c[1], c[2]};
        out[used++] = {-1.0f + 2.0f * x1 / W, -1.0f + 2.0f * y1 / H, c[0], c[1], c[2]};
    }

    void circle(float cx, float cy, float r, const float c[3], int seg = 40) {
        for (int i = 0; i < seg; i++) {
            float a0 = PI2 * (float)i / (float)seg, a1 = PI2 * (float)(i + 1) / (float)seg;
            line(cx + r * std::cos(a0), cy + r * std::sin(a0), cx + r * std::cos(a1), cy + r * std::sin(a1), c);
        }
    }

    void hand(float cx, float cy, float frac, float len, const float c[3], float thick) {
        float a = PI2 * frac - 1.5707963f;
        float dx = std::cos(a), dy = std::sin(a);
        float nx = -dy, ny = dx;
        for (float o = -thick; o <= thick; o += 0.5f)
            line(cx + nx * o, cy + ny * o, cx + dx * len + nx * o, cy + dy * len + ny * o, c);
    }
};

void fmtDuration(double s, char* buf, size_t n) {
    double days = s / 86400.0, years = days / 365.25;
    if (s < 60.0) std::snprintf(buf, n, "%.1f SEC", s);
    else if (s < 3600.0) std::snprintf(buf, n, "%d MIN %02d SEC", (int)(s / 60.0), (int)std::fmod(s, 60.0));
    else if (s < 86400.0) std::snprintf(buf, n, "%d HR %02d MIN", (int)(s / 3600.0), (int)(std::fmod(s, 3600.0) / 60.0));
    else if (days < 365.25) std::snprintf(buf, n, "%d DAYS %d HR", (int)days, (int)(std::fmod(s, 86400.0) / 3600.0));
    else if (years < 1000.0) std::snprintf(buf, n, "%d YR %d DAYS", (int)years, (int)(std::fmod(days, 365.25)));
    else if (years < 1.0e6) std::snprintf(buf, n, "%.0f YEARS", years);
    else if (years < 1.0e9) std::snprintf(buf, n, "%.1f MILLION YR", years / 1.0e6);
    else std::snprintf(buf, n, "%.1f BILLION YR", years / 1.0e9);
}

void fmtHourEquals(double factor, char* buf, size_t n) {
    double s = 3600.0 * factor;
    if (s < 100.0 * 3600.0) std::snprintf(buf, n, "%.2f HOURS", s / 3600.0);
    else if (s < 100.0 * 86400.0) std::snprintf(buf, n, "%.1f DAYS", s / 86400.0);
    else {
        double y = s / 86400.0 / 365.25;
        if (y < 100.0) {
            char t[32];
            std::snprintf(t, sizeof(t), "%.1f", y);
            size_t l = std::strlen(t);
            if (l > 2 && t[l - 1] == '0' && t[l - 2] == '.') t[l - 2] = 0;
            std::snprintf(buf, n, "%s YEARS", t);
        } else std::snprintf(buf, n, "%.0f YEARS", y);
    }
}

void fmtCount(double v, char* buf, size_t n) {
    if (v < 100.0) {
        std::snprintf(buf, n, "%.2f", v);
        return;
    }
    char t[32];
    std::snprintf(t, sizeof(t), "%.0f", v);
    int len = (int)std::strlen(t), o = 0;
    for (int i = 0; i < len && o + 2 < (int)n; i++) {
        if (i > 0 && (len - i) % 3 == 0) buf[o++] = ',';
        buf[o++] = t[i];
    }
    buf[o] = 0;
}

void shiftColour(float stretch, float c[3]) {
    float t = std::min(1.0f, std::log10(std::max(stretch, 1.0f)) / 1.2f);
    c[0] = 0.95f - 0.25f * t * t;
    c[1] = 0.95f - 0.8f * t;
    c[2] = 1.0f - 0.95f * std::sqrt(t);
    float dim = 1.0f - 0.45f * t * t;
    for (int i = 0; i < 3; i++) c[i] *= dim;
}

const char* FACTS[2] = {
    "GPS SATELLITE CLOCKS GAIN 38 MICROSECONDS PER DAY - ENGINEERS MUST CORRECT THEM",
    "ASTRONAUTS ON THE ISS AGE ABOUT 0.01 SECOND LESS EVERY 6 MONTHS",
};

}

void emitTimeDilation(AppContext& app, OverlayVertex* out, uint32_t& used) {
    const TimeDilState& S = app.td;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float px = 2.0f / W, py = 2.0f / H, k = H / 720.0f;
    Pen pen{out, used, W, H};
    int s0 = std::max(1, (int)std::lround(1.0f * k)), s1 = std::max(1, (int)std::lround(2.0f * k));
    float cx = 0.5f * W, cy = 0.5f * H;
    float rr = S.miller ? 1.01f : td::radiusOf(S.x);
    float stretch = S.miller ? td::MILLER_FACTOR : 1.0f / std::sqrt(1.0f - 1.0f / rr);
    float grav = td::gravFactor(S), spec = td::gammaOf(S.rapidity), total = grav * spec;
    float pc[3];
    shiftColour(stretch, pc);
    const float farC[3] = {0.7f, 0.85f, 1.0f};
    const float dimC[3] = {0.28f, 0.34f, 0.46f};
    const float shipC[3] = {0.85f, 0.92f, 1.0f};
    const float fillC[3] = {0.95f, 0.7f, 0.3f};

    float d = td::probeDistPx(S, H);
    float prx = cx - 0.8f * d, pry = cy - 0.6f * d;
    float dObs = td::shadowPx(H) * (1.3f + 1.45f * 3.0f);
    float obx = cx + 0.89f * dObs, oby = cy - 0.45f * dObs;

    float seg = 14.0f * k;
    float dx = obx - prx, dy = oby - pry, len = std::sqrt(dx * dx + dy * dy);
    for (float t = 0.0f; t + seg < len; t += 2.0f * seg)
        pen.line(prx + dx / len * (t + 12.0f * k), pry + dy / len * (t + 12.0f * k), prx + dx / len * (t + seg), pry + dy / len * (t + seg), pc);

    float h = 11.0f * k;
    pen.circle(prx, pry - 1.5f * h, h * 0.8f, pc, 14);
    pen.line(prx, pry - 0.7f * h, prx, pry + 1.6f * h, pc);
    pen.line(prx - 1.3f * h, pry + 0.2f * h, prx + 1.3f * h, pry + 0.2f * h, pc);
    pen.line(prx, pry + 1.6f * h, prx - 0.9f * h, pry + 3.0f * h, pc);
    pen.line(prx, pry + 1.6f * h, prx + 0.9f * h, pry + 3.0f * h, pc);
    pen.line(prx - 0.5f * h, pry - 0.9f * h, prx + 0.5f * h, pry - 0.9f * h, pc);

    pen.line(obx - 18.0f * k, oby + 5.0f * k, obx + 18.0f * k, oby, shipC);
    pen.line(obx - 18.0f * k, oby - 5.0f * k, obx + 18.0f * k, oby, shipC);
    pen.line(obx - 18.0f * k, oby - 5.0f * k, obx - 18.0f * k, oby + 5.0f * k, shipC);
    pen.line(obx - 18.0f * k, oby - 5.0f * k, obx - 26.0f * k, oby - 9.0f * k, shipC);
    pen.line(obx - 18.0f * k, oby + 5.0f * k, obx - 26.0f * k, oby + 9.0f * k, shipC);

    float R = 58.0f * k, ccy = H - 215.0f * k;
    float cxs[2] = {118.0f * k, W - 118.0f * k};
    double secs[2] = {S.tau, S.far};
    float blurRate = total * S.speed;
    for (int i = 0; i < 2; i++) {
        const float* col = i == 0 ? pc : farC;
        pen.circle(cxs[i], ccy, R, col, 48);
        pen.circle(cxs[i], ccy, R + 3.0f * k, dimC, 48);
        for (int m = 0; m < 12; m++) {
            float a = PI2 * (float)m / 12.0f;
            float in = R - (m % 3 == 0 ? 9.0f : 5.0f) * k;
            pen.line(cxs[i] + in * std::cos(a), ccy + in * std::sin(a), cxs[i] + R * std::cos(a), ccy + R * std::sin(a), col);
        }
        bool blur = i == 1 && blurRate > 90.0f;
        if (blur) {
            for (int m = 0; m < 36; m++) {
                float a = PI2 * (float)m / 36.0f;
                pen.line(cxs[i], ccy, cxs[i] + 0.8f * R * std::cos(a), ccy + 0.8f * R * std::sin(a), dimC);
            }
        } else {
            pen.hand(cxs[i], ccy, (float)std::fmod(secs[i] / 43200.0, 1.0), 0.5f * R, col, 1.0f * k);
            pen.hand(cxs[i], ccy, (float)std::fmod(secs[i] / 3600.0, 1.0), 0.82f * R, col, 0.5f * k);
        }
    }

    float bx0 = 0.5f * W - 170.0f * k, bx1 = 0.5f * W + 170.0f * k, by = H - 124.0f * k;
    const float barC[3] = {0.5f, 0.6f, 0.8f};
    pen.line(bx0, by - 6.0f * k, bx1, by - 6.0f * k, barC);
    pen.line(bx0, by + 6.0f * k, bx1, by + 6.0f * k, barC);
    pen.line(bx0, by - 6.0f * k, bx0, by + 6.0f * k, barC);
    pen.line(bx1, by - 6.0f * k, bx1, by + 6.0f * k, barC);
    float fill = S.rapidity / td::RAPID_MAX;
    for (float x = bx0 + 2.0f; x < bx0 + (bx1 - bx0) * fill; x += 2.0f)
        pen.line(x, by - 4.0f * k, x, by + 4.0f * k, fillC);
    uint32_t lineVerts = used;

    auto text = [&](const char* t, float x, float y, int sc, const float c[3]) {
        emitText(out, used, t, -1.0f + x * px, -1.0f + y * py, px, py, (float)sc, c[0], c[1], c[2]);
    };
    auto centred = [&](const char* t, float x, float y, int sc, const float c[3]) {
        float l = (float)std::strlen(t) * 6.0f * (float)sc;
        text(t, x - 0.5f * l, y, sc, c);
    };
    const float white[3] = {0.75f, 0.8f, 0.9f};
    const float title[3] = {0.6f, 0.8f, 1.0f};
    float line = 8.0f * (float)s0 + 5.0f * k;
    float x0 = 14.0f * k, y = 14.0f * k;
    text("TIME DILATION", x0, y, s1 + 1, title);
    y += 8.0f * (float)(s1 + 1) + 8.0f * k;
    text("IN STRONG GRAVITY OR AT HIGH SPEED, YOUR CLOCK TICKS SLOWER THAN A CLOCK FAR AWAY", x0, y, s0, white);
    y += line;
    char buf[200];
    std::snprintf(buf, sizeof(buf), "YOUR TIME RUNS 1 MINUTE PER SECOND   %.2gX SPEED   DRAG TO ORBIT", (double)S.speed);
    text(buf, x0, y, s0, white);

    if (S.miller) std::snprintf(buf, sizeof(buf), "YOU: R = 1.0000000027 RS");
    else std::snprintf(buf, sizeof(buf), "YOU: R = %.*f RS", rr < 10.0f ? 3 : 1, (double)rr);
    float lw = (float)std::strlen(buf) * 6.0f * (float)s0;
    text(buf, prx - 16.0f * k - lw, pry - 6.0f * k, s0, pc);
    if (S.rapidity > 0.01f) {
        std::snprintf(buf, sizeof(buf), "MOVING %.4f C", (double)td::speedOf(S.rapidity));
        text(buf, prx - 16.0f * k - (float)std::strlen(buf) * 6.0f * (float)s0, pry - 6.0f * k + line, s0, pc);
    }
    text("FAR AWAY OBSERVER", obx - 60.0f * k, oby + 16.0f * k, s0, shipC);
    text("1000 RS - FIXED", obx - 50.0f * k, oby + 16.0f * k + line, s0, shipC);

    const char* names[2] = {"YOUR CLOCK", "FAR AWAY"};
    for (int i = 0; i < 2; i++) {
        const float* col = i == 0 ? pc : farC;
        centred(names[i], cxs[i], ccy - R - 26.0f * k, s1, col);
        fmtDuration(secs[i], buf, sizeof(buf));
        int sc = fitScale(buf, 2.0f * R + 40.0f * k, s1);
        centred(buf, cxs[i], ccy + R + 12.0f * k, sc, col);
        if (i == 1 && blurRate > 90.0f) centred("TOO FAST TO SEE", cxs[i], ccy + R + 12.0f * k + 8.0f * (float)s1 + 6.0f * k, s0, dimC);
    }

    char hour[64];
    fmtHourEquals(total, hour, sizeof(hour));
    std::snprintf(buf, sizeof(buf), "1 HOUR HERE = %s FAR AWAY", hour);
    float cw = W - 2.0f * 250.0f * k;
    int hs = fitScale(buf, cw, std::max(2, (int)std::lround(4.0f * k)));
    centred(buf, 0.5f * W, H - 215.0f * k, hs, total > 1.5f ? fillC : farC);
    char num[48];
    fmtCount(stretch, num, sizeof(num));
    std::snprintf(buf, sizeof(buf), "LIGHT FROM YOU IS STRETCHED %s TIMES", num);
    centred(buf, 0.5f * W, H - 178.0f * k, fitScale(buf, cw, s1), pc);
    char g[32], v[32], t[32];
    fmtCount(grav, g, sizeof(g));
    fmtCount(spec, v, sizeof(v));
    fmtCount(total, t, sizeof(t));
    std::snprintf(buf, sizeof(buf), "GRAVITY SLOWS YOU %sX   SPEED %sX   TOGETHER %sX", g, v, t);
    centred(buf, 0.5f * W, H - 152.0f * k, fitScale(buf, cw, s1), white);
    float sp = td::speedOf(S.rapidity);
    std::snprintf(buf, sizeof(buf), sp < 0.99f ? "SPEED %.2f C" : "SPEED %.5f C", (double)sp);
    text(buf, bx0 - (float)std::strlen(buf) * 6.0f * (float)s0 - 10.0f * k, by - 4.0f * k, s0, fillC);
    text("LEFT / RIGHT", bx1 + 10.0f * k, by - 4.0f * k, s0, dimC);

    int cs = std::max(2, (int)std::lround(3.0f * k));
    if (S.miller) {
        const char* a = "MILLER'S PLANET (INTERSTELLAR): 1 HOUR = 7 YEARS, A FACTOR OF 61,320";
        const char* b = "NEEDS A FAST-SPINNING HOLE LIKE GARGANTUA - A NON-SPINNING HOLE HAS NO STABLE ORBIT THIS DEEP";
        bigText(out, used, a, W, H - 92.0f * k, px, py, fitScale(a, W, cs), 1.0f, 0.8f, 0.4f);
        bigText(out, used, b, W, H - 64.0f * k, px, py, fitScale(b, W, cs), 0.95f, 0.85f, 0.7f);
    } else {
        float ph = std::fmod(S.clock, 7.0f);
        float fa = std::min(1.0f, std::min(ph / 0.8f, (7.0f - ph) / 0.8f));
        const char* fact = FACTS[((int)(S.clock / 7.0f)) % 2];
        bigText(out, used, fact, W, H - 74.0f * k, px, py, fitScale(fact, W, cs), 0.95f * fa, 0.85f * fa, 0.7f * fa);
    }
    const char* hint = S.paused ? "PAUSED - SPACE RESUME  R RESET  UP / DOWN OR SCROLL MOVE  LEFT / RIGHT SPEED  I MILLER'S PLANET" : "SPACE PAUSE  R RESET  UP / DOWN OR SCROLL MOVE  LEFT / RIGHT SPEED  I MILLER'S PLANET";
    centred(hint, 0.5f * W, H - 26.0f * k, fitScale(hint, W, s0), dimC);

    app.overlayLineVerts[app.currentFrame] = lineVerts;
    app.overlayPointVerts[app.currentFrame] = used - lineVerts;
}
