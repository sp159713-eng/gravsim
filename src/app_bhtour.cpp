#include "vk_app.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace {

struct Stop {
    const char* name;
    const char* mass;
    const char* fact;
    float yaw;
    float pitch;
};

const Stop STOPS[] = {
    {"THE SUN", "1 SOLAR MASS", "OUR STAR - 1.4 MILLION KM ACROSS", 0.0f, 0.45f},
    {"SGR A*", "4.3 MILLION SUNS", "OUR GALAXY'S BLACK HOLE - 18X THE SUN", 0.35f, 0.5f},
    {"M87*", "6.5 BILLION SUNS", "FIRST BLACK HOLE EVER PHOTOGRAPHED (2019)", -0.3f, 0.5f},
    {"HOLMBERG 15A", "40 BILLION SUNS", "ONE OF THE MOST RELIABLE GIANTS", 0.3f, 0.5f},
    {"TON 618", "66 BILLION SUNS", "PLUTO'S WHOLE ORBIT FITS INSIDE", -0.3f, 0.5f},
    {"PHOENIX A", "100 BILLION SUNS", "MAYBE THE BIGGEST - ESTIMATE DISPUTED", 0.3f, 0.5f},
    {"SIDE BY SIDE", "FROM 1 TO 100 BILLION SUNS", "THE SUN IS THE TINY DOT ON THE LEFT", 0.0f, 0.55f},
};
const int STOP_COUNT = 7;
const int FINALE = 6;
const int HOLE_COUNT = 5;
const float HOLD = 3.5f;
const float HOLD_FINALE = 8.0f;
const float CAPTION_FINALE = 4.2f;
const float TRANS = 5.0f;
const float TRANS_FINALE = 6.0f;
const float FADE = 0.7f;
const float TAN30 = 0.57735027f;
const float FILL = 0.42f;
const float LIFT = 0.1f;

float holdLen(int k) {
    return k == FINALE ? HOLD_FINALE : HOLD;
}

float captionLen(int k) {
    return k == FINALE ? CAPTION_FINALE : HOLD;
}

float transLen(int k) {
    return k + 1 == FINALE ? TRANS_FINALE : TRANS;
}

float totalLen() {
    float t = 0.0f;
    for (int k = 0; k < STOP_COUNT; k++) {
        t += holdLen(k);
        if (k + 1 < STOP_COUNT) t += transLen(k);
    }
    return t;
}

struct Where {
    int stop;
    bool hold;
    float local;
    float s;
};

Where locate(float t) {
    float at = 0.0f;
    for (int k = 0; k < STOP_COUNT; k++) {
        if (t < at + holdLen(k) || k == FINALE) return {k, true, std::min(t - at, holdLen(k)), 0.0f};
        at += holdLen(k);
        if (t < at + transLen(k)) return {k, false, t - at, (t - at) / transLen(k)};
        at += transLen(k);
    }
    return {FINALE, true, HOLD_FINALE, 0.0f};
}

struct Pose {
    float logd, yaw, pitch;
    float target[3];
};

Pose stopPose(const AppContext& app, int k) {
    Pose p;
    float d, x;
    if (k == FINALE) {
        float dFar, centre;
        bhFarFraming(app, dFar, centre);
        d = dFar;
        x = centre;
    } else {
        double sx, radius;
        bhStopGeom(app, k, sx, radius);
        d = (float)(radius / (FILL * TAN30));
        x = (float)sx;
    }
    p.logd = std::log(d);
    p.yaw = STOPS[k].yaw;
    p.pitch = STOPS[k].pitch;
    float up[3] = {-std::sin(p.pitch) * std::sin(p.yaw), std::cos(p.pitch), -std::sin(p.pitch) * std::cos(p.yaw)};
    float shift = LIFT * 2.0f * TAN30 * d;
    p.target[0] = x - up[0] * shift;
    p.target[1] = -up[1] * shift;
    p.target[2] = -up[2] * shift;
    return p;
}

Pose poseAt(const AppContext& app, float t) {
    Where w = locate(t);
    Pose a = stopPose(app, w.stop);
    if (w.hold) return a;
    Pose b = stopPose(app, w.stop + 1);
    float e = w.s * w.s * (3.0f - 2.0f * w.s);
    float aspect = (float)app.swapchainExtent.width / (float)std::max(1u, app.swapchainExtent.height);
    float bump = 0.0f;
    if (w.stop + 1 != FINALE) {
        float sep = std::fabs(b.target[0] - a.target[0]);
        float peak = 0.625f * sep / (TAN30 * aspect);
        bump = std::max(0.0f, std::log(std::max(peak, 1e-6f)) - 0.5f * (a.logd + b.logd));
    }
    Pose p;
    p.logd = a.logd + (b.logd - a.logd) * e + bump * std::sin(3.14159265f * e);
    p.yaw = a.yaw + (b.yaw - a.yaw) * e;
    p.pitch = a.pitch + (b.pitch - a.pitch) * e;
    for (int i = 0; i < 3; i++) p.target[i] = a.target[i] + (b.target[i] - a.target[i]) * e;
    return p;
}

void applyPose(AppContext& app) {
    BhTour& b = app.bhTour;
    Pose p = poseAt(app, b.t);
    app.bhLogDist = p.logd;
    app.camera.yaw = b.yaw = p.yaw;
    app.camera.pitch = b.pitch = p.pitch;
    for (int i = 0; i < 3; i++) b.target[i] = p.target[i];
}

}

void bigText(OverlayVertex* out, uint32_t& used, const char* text, float W, float y, float px, float py, int scale, float r, float g, float b) {
    float width = (float)(std::strlen(text) * 6 - 1) * (float)scale;
    float x = std::floor(0.5f * (W - width));
    int last = std::max(0, scale - 2);
    for (int oy = 0; oy <= last; oy += 2)
        for (int ox = 0; ox <= last; ox += 2) {
            int cy = oy == last - 1 ? last : oy, cx = ox == last - 1 ? last : ox;
            emitText(out, used, text, -1.0f + (x + (float)cx) * px, -1.0f + (y + (float)cy) * py, px, py, (float)scale, r, g, b);
        }
}

int fitScale(const char* text, float W, int want) {
    int fit = (int)std::floor(0.94f * W / (float)(std::strlen(text) * 6));
    return std::max(1, std::min(want, fit));
}

void bhTourStart(AppContext& app) {
    BhTour& b = app.bhTour;
    b.mode = 1;
    b.t = 0.0f;
    applyPose(app);
}

void bhTourToggle(AppContext& app) {
    BhTour& b = app.bhTour;
    if (b.mode == 1) b.mode = 2;
    else if (b.mode == 2) b.mode = 1;
}

void bhTourStep(AppContext& app, float dt, bool input) {
    BhTour& b = app.bhTour;
    Camera& c = app.camera;
    if (input || std::fabs(c.yaw - b.yaw) > 1e-4f || std::fabs(c.pitch - b.pitch) > 1e-4f) {
        b.mode = 0;
        return;
    }
    if (b.mode == 1) b.t += std::min(dt, 0.1f);
    float total = totalLen();
    if (b.t >= total) {
        b.t = total;
        applyPose(app);
        b.mode = 0;
        return;
    }
    applyPose(app);
}

int bhTourFocus(const AppContext& app) {
    const BhTour& b = app.bhTour;
    if (b.mode == 0) return -2;
    Where w = locate(b.t);
    if (!w.hold) return -1;
    if (w.stop == FINALE) return w.local < CAPTION_FINALE ? -1 : -2;
    return w.stop == 0 ? HOLE_COUNT : w.stop - 1;
}

void emitBhTour(AppContext& app, OverlayVertex* out, uint32_t& used, float px, float py) {
    const BhTour& b = app.bhTour;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float k = H / 720.0f;
    if (b.mode != 0) {
        Where w = locate(b.t);
        if (w.hold && w.local < captionLen(w.stop)) {
            float a = std::min(1.0f, std::min(w.local, captionLen(w.stop) - w.local) / FADE);
            a = std::max(0.0f, a);
            a = a * a * (3.0f - 2.0f * a);
            const Stop& st = STOPS[w.stop];
            int s1 = fitScale(st.name, W, std::max(2, (int)std::lround(6.0f * k)));
            int s2 = fitScale(st.mass, W, std::max(2, (int)std::lround(4.0f * k)));
            int s3 = fitScale(st.fact, W, std::max(1, (int)std::lround(3.0f * k)));
            float y = std::floor(0.72f * H);
            bigText(out, used, st.name, W, y, px, py, s1, 1.0f * a, 0.85f * a, 0.45f * a);
            y += 7.0f * (float)s1 + 14.0f * k;
            bigText(out, used, st.mass, W, y, px, py, s2, 0.85f * a, 0.9f * a, 1.0f * a);
            y += 7.0f * (float)s2 + 12.0f * k;
            bigText(out, used, st.fact, W, y, px, py, s3, 1.0f * a, 0.65f * a, 0.5f * a);
        }
    }
    const char* hint = b.mode == 1 ? "SPACE PAUSE  R REPLAY" : b.mode == 2 ? "PAUSED - SPACE RESUME  R REPLAY" : "R REPLAY TOUR";
    float len = (float)std::strlen(hint) * 6.0f;
    emitText(out, used, hint, -1.0f + (W - len - 14.0f) * px, -1.0f + (H - 20.0f) * py, px, py, 1.0f, 0.7f, 0.75f, 0.85f);
}
