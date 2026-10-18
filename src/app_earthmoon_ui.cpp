#include "vk_app.h"
#include "earthmoon_geom.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

struct Proj {
    float m[16];
    float eye[3];
    float W, H;

    bool at(const float p[3], float& x, float& y) const {
        float cx = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
        float cy = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
        float cw = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15];
        if (cw < 0.1f) return false;
        x = (cx / cw + 1.0f) * 0.5f * W;
        y = (cy / cw + 1.0f) * 0.5f * H;
        return true;
    }

    bool hiddenByEarth(const float p[3]) const {
        float d[3] = {p[0] - eye[0], p[1] - eye[1], p[2] - eye[2]};
        float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        for (float& v : d) v /= len;
        float b = eye[0] * d[0] + eye[1] * d[1] + eye[2] * d[2];
        float c = eye[0] * eye[0] + eye[1] * eye[1] + eye[2] * eye[2] - 1.0f;
        float h = b * b - c;
        if (h < 0.0f) return false;
        float t = -b - std::sqrt(h);
        return t > 0.0f && t < len - 0.02f;
    }
};

struct Pen {
    OverlayVertex* out;
    uint32_t& used;
    const Proj& pj;
    float px, py;

    void seg(const float a[3], const float b[3], const float col[3], bool occlude) {
        if (used + 2 > MAX_VERTS - 12000) return;
        float mid[3] = {0.5f * (a[0] + b[0]), 0.5f * (a[1] + b[1]), 0.5f * (a[2] + b[2])};
        if (occlude && (pj.hiddenByEarth(mid) || pj.hiddenByEarth(a) || pj.hiddenByEarth(b))) return;
        float x0, y0, x1, y1;
        if (!pj.at(a, x0, y0) || !pj.at(b, x1, y1)) return;
        out[used++] = {-1.0f + x0 * px, -1.0f + y0 * py, col[0], col[1], col[2]};
        out[used++] = {-1.0f + x1 * px, -1.0f + y1 * py, col[0], col[1], col[2]};
    }
};

const char* FACTS[3] = {
    "THE MOON IS DRIFTING AWAY 3.8 CM PER YEAR",
    "WE ALWAYS SEE THE SAME FACE - TIDAL LOCKING",
    "THE FAR SIDE IS NOT DARK - IT GETS AS MUCH SUN",
};

}

void emitEarthMoon(AppContext& app, OverlayVertex* out, uint32_t& used) {
    const EarthMoonState& S = app.em;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float px = 2.0f / W, py = 2.0f / H, k = H / 720.0f;
    float aspect = W / H;
    Proj pj{};
    cameraViewProj(app.camera, aspect, pj.m);
    cameraEye(app.camera, pj.eye);
    pj.W = W;
    pj.H = H;
    Pen pen{out, used, pj, px, py};

    float phi = em::phiOf(S.days);
    em::V3 mp = em::moonPos(phi);
    float moon[3] = {mp.x, mp.y, mp.z};
    const float ring[3] = {0.28f, 0.34f, 0.5f};
    const float toMoon[3] = {0.35f, 0.35f, 0.2f};
    const float sunC[3] = {1.0f, 0.82f, 0.3f};
    float orbit[130][3];
    for (int i = 0; i <= 128; i++) {
        em::V3 q = em::moonPos(em::TWO_PI * (float)i / 128.0f);
        orbit[i][0] = q.x; orbit[i][1] = q.y; orbit[i][2] = q.z;
    }
    for (int i = 0; i < 128; i++) pen.seg(orbit[i], orbit[i + 1], ring, true);
    float origin[3] = {0.0f, 0.0f, 0.0f};
    float nearM[3] = {mp.x * 0.9f, mp.y * 0.9f, mp.z * 0.9f};
    pen.seg(origin, nearM, toMoon, true);

    const float ay[4] = {0.0f, 2.2f, -2.2f, 0.0f};
    const float az[4] = {0.0f, 0.0f, 0.0f, 2.2f};
    for (int i = 0; i < 4; i++) {
        float tail[3] = {13.0f, ay[i], az[i]}, tip[3] = {9.0f, ay[i], az[i]};
        pen.seg(tail, tip, sunC, false);
        float h1[3] = {tip[0] + 0.8f, tip[1] + 0.4f, tip[2]}, h2[3] = {tip[0] + 0.8f, tip[1] - 0.4f, tip[2]};
        float h3[3] = {tip[0] + 0.8f, tip[1], tip[2] + 0.4f}, h4[3] = {tip[0] + 0.8f, tip[1], tip[2] - 0.4f};
        pen.seg(tip, h1, sunC, false);
        pen.seg(tip, h2, sunC, false);
        pen.seg(tip, h3, sunC, false);
        pen.seg(tip, h4, sunC, false);
    }
    uint32_t lineVerts = used;

    int s1 = std::max(1, (int)std::lround(2.0f * k));
    int s0 = std::max(1, (int)std::lround(1.0f * k));
    float line = 8.0f * (float)s0 + 5.0f * k;
    auto text = [&](const char* t, float x, float y, int sc, float r, float g, float b) {
        emitText(out, used, t, -1.0f + x * px, -1.0f + y * py, px, py, (float)sc, r, g, b);
    };
    float x0 = 14.0f * k, y = 14.0f * k;
    text("EARTH AND MOON", x0, y, s1 + 1, 0.6f, 0.8f, 1.0f);
    y += 8.0f * (float)(s1 + 1) + 8.0f * k;
    char buf[160];
    std::snprintf(buf, sizeof(buf), "MOON ORBIT 29.5 DAYS   EARTH SPIN 1 DAY   %.1f DAYS PER SECOND   DAY %.1f", 2.0f * S.speed, S.days);
    text(buf, x0, y, s0, 0.75f, 0.8f, 0.9f);
    y += line;
    text("YELLOW ARROWS SUNLIGHT   BLUE SHELL OCEAN BULGE (EXAGGERATED)   RED DOT CITY", x0, y, s0, 0.75f, 0.8f, 0.9f);
    y += line + 6.0f * k;
    const char* tideNames[4] = {"LOW TIDE", "TIDE RISING", "HIGH TIDE", "TIDE FALLING"};
    const float tc[4][3] = {{0.55f, 0.7f, 0.95f}, {0.6f, 0.9f, 0.95f}, {0.4f, 1.0f, 0.6f}, {0.95f, 0.85f, 0.55f}};
    std::snprintf(buf, sizeof(buf), "CITY (RED DOT): %s", tideNames[S.tideState]);
    text(buf, x0, y, s1, tc[S.tideState][0], tc[S.tideState][1], tc[S.tideState][2]);
    y += 8.0f * (float)s1 + 6.0f * k;
    std::snprintf(buf, sizeof(buf), "HIGH TIDES SO FAR %d   TWO EVERY 24H 50M", S.highCount);
    text(buf, x0, y, s0, 0.75f, 0.8f, 0.9f);
    y += line;
    std::snprintf(buf, sizeof(buf), "TIDAL BULGE %d%% OF MAXIMUM", (int)std::lround(100.0f * em::bulgeFraction(phi)));
    text(buf, x0, y, s0, 0.75f, 0.8f, 0.9f);

    float sx, sy;
    if (pj.at(moon, sx, sy)) text("MOON", sx + 16.0f * k, sy - 26.0f * k, s0, 0.85f, 0.85f, 0.9f);
    float sunLbl[3] = {13.0f, 0.0f, 0.0f};
    if (pj.at(sunLbl, sx, sy)) text("SUNLIGHT", sx - 10.0f * k, sy - 22.0f * k, s0, 1.0f, 0.85f, 0.4f);
    float earthLbl[3] = {0.0f, -1.5f, 0.0f};
    if (pj.at(earthLbl, sx, sy)) text("EARTH", sx - 15.0f * k, sy, s0, 0.7f, 0.85f, 1.0f);
    em::V3 cn = em::cityNormal(em::TWO_PI * S.days);
    float cp[3] = {cn.x, cn.y, cn.z};
    float facing = cn.x * (pj.eye[0] - cn.x) + cn.y * (pj.eye[1] - cn.y) + cn.z * (pj.eye[2] - cn.z);
    if (facing > 0.0f && pj.at(cp, sx, sy)) text(tideNames[S.tideState], sx + 14.0f * k, sy - 5.0f * k, s0, tc[S.tideState][0], tc[S.tideState][1], tc[S.tideState][2]);

    float rad = 80.0f * k;
    float icx = 1.22f * rad + 20.0f * k, icy = H - 270.0f * k;
    const char* title = "MOON AS SEEN FROM EARTH";
    float tl = (float)std::strlen(title) * 6.0f * (float)s0;
    text(title, icx - 0.5f * tl, icy - 1.22f * rad - 16.0f * k, s0, 0.7f, 0.8f, 0.95f);
    const char* name = em::phaseName(phi);
    float nl = (float)std::strlen(name) * 6.0f * (float)s1;
    text(name, icx - 0.5f * nl, icy + 1.22f * rad + 10.0f * k, s1, 1.0f, 0.95f, 0.75f);
    std::snprintf(buf, sizeof(buf), "%d%% LIT   %s", (int)std::lround(100.0f * em::illum(phi)), em::waxing(phi) ? "WAXING" : "WANING");
    float bl = (float)std::strlen(buf) * 6.0f * (float)s0;
    text(buf, icx - 0.5f * bl, icy + 1.22f * rad + 14.0f * k + 8.0f * (float)s1, s0, 0.8f, 0.85f, 0.95f);

    float c = std::fabs(std::cos(phi));
    const char* cap;
    float cr = 0.6f, cg = 0.85f, cb = 1.0f;
    if (c > 0.86f) cap = "SPRING TIDES - SUN AND MOON PULL TOGETHER - BIGGEST BULGE";
    else if (c < 0.34f) cap = "NEAP TIDES - SUN AND MOON PULL AT RIGHT ANGLES - SMALLEST BULGE";
    else if (std::sin(2.0f * phi) < 0.0f) cap = "TIDES GROWING TOWARD SPRING TIDES";
    else cap = "TIDES EASING TOWARD NEAP TIDES";
    if (c > 0.86f) { cr = 1.0f; cg = 0.8f; cb = 0.4f; }
    int cs = fitScale(cap, W, std::max(2, (int)std::lround(3.0f * k)));
    bigText(out, used, cap, W, H - 112.0f * k, px, py, cs, cr, cg, cb);
    float ph = std::fmod(S.clock, 7.0f);
    float fa = std::min(1.0f, std::min(ph / 0.8f, (7.0f - ph) / 0.8f));
    const char* fact = FACTS[((int)(S.clock / 7.0f)) % 3];
    int fs = fitScale(fact, W, std::max(2, (int)std::lround(3.0f * k)));
    bigText(out, used, fact, W, H - 74.0f * k, px, py, fs, 0.95f * fa, 0.85f * fa, 0.7f * fa);
    const char* hint = S.paused ? "PAUSED - SPACE RESUME  R RESET  + - SPEED  DRAG ORBIT  SCROLL ZOOM" : "SPACE PAUSE  R RESET  + - SPEED  DRAG ORBIT  SCROLL ZOOM";
    float hl = (float)std::strlen(hint) * 6.0f * (float)s0;
    text(hint, 0.5f * (W - hl), H - 26.0f * k, s0, 0.65f, 0.7f, 0.8f);

    app.overlayLineVerts[app.currentFrame] = lineVerts;
    app.overlayPointVerts[app.currentFrame] = used - lineVerts;
}
