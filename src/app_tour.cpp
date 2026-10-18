#include "vk_app.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

struct Stop {
    const char* name;
    const char* size;
    const char* fact;
    float exp;
    int a;
    int b;
};

const Stop STOPS[] = {
    {"PLANCK LENGTH", "1.6 X 10^-35 M - 50 BILLION BILLION TIMES SMALLER THAN A PROTON", "WHERE SPACE ITSELF MAY BECOME FOAM - OUR PHYSICS STOPS HERE; THIS PICTURE IS A GUESS", -34.8f, 0, 0},
    {"QUARKS", "10^-18 M - A BILLIONTH OF A BILLIONTH OF A METRE", "THREE QUARKS BOUND BY GLUONS MAKE ONE PROTON - ILLUSTRATION, NOT A PHOTO", -18.0f, 1, 1},
    {"PROTONS AND NEUTRONS", "10^-15 M - A MILLIONTH OF A BILLIONTH OF A METRE", "ALMOST ALL OF AN ATOM'S MASS SITS IN THIS TINY NUCLEUS - ILLUSTRATION", -15.0f, 2, 2},
    {"ATOM", "10^-10 M - A TEN-BILLIONTH OF A METRE", "IF THE NUCLEUS WERE A MARBLE, THE ATOM WOULD BE A STADIUM", -10.0f, 3, 3},
    {"MOLECULES", "10^-9 M - A BILLIONTH OF A METRE", "WATER IS 0.3 NM ACROSS - A DNA HELIX IS 2 NM WIDE AND ABOUT 2 M LONG UNRAVELLED", -9.0f, 4, 5},
    {"CELL", "10^-5 M - TEN MICROMETRES", "YOU ARE MADE OF ABOUT 30 TRILLION CELLS - ILLUSTRATION", -5.0f, 6, 6},
    {"HUMAN", "1.7 M - ABOUT 10^0 METRES", "THE SCALE WE LIVE AT - THE BAR ON THE RIGHT IS ONE METRE", 0.23f, 7, 7},
    {"EARTH", "1.3 X 10^7 M - 12,742 KM ACROSS", "HOME - THE ONLY KNOWN PLACE WITH LIFE", 7.1f, -1, -1},
    {"EARTH AND MOON", "7.7 X 10^8 M - 384,400 KM APART", "LIGHT TAKES 1.3 SECONDS TO CROSS THE GAP", 8.9f, -1, -1},
    {"SOLAR SYSTEM", "9 X 10^12 M - 60 AU ACROSS TO NEPTUNE'S ORBIT", "LIGHT TAKES 4 HOURS TO REACH NEPTUNE", 12.95f, -1, -1},
    {"OORT CLOUD", "10^16 M - ABOUT 1 LIGHT-YEAR", "TRILLIONS OF ICY COMETS IN A SHELL AROUND THE SUN - ILLUSTRATION; TOO FAINT TO SEE FOR REAL", 16.0f, -1, -1},
    {"NEAREST STARS", "10^17 M - 10 LIGHT-YEARS", "PROXIMA CENTAURI, 4.2 LIGHT-YEARS AWAY, IS THE CLOSEST STAR", 17.0f, -1, -1},
    {"MILKY WAY", "10^21 M - 100,000 LIGHT-YEARS", "200 BILLION STARS CIRCLING A 4-MILLION-SUN BLACK HOLE, SGR A*", 21.0f, -1, -1},
    {"LOCAL GROUP", "10^23 M - 10 MILLION LIGHT-YEARS", "THE MILKY WAY, ANDROMEDA AND ABOUT 80 SMALLER GALAXIES", 23.0f, -1, -1},
    {"LANIAKEA", "5 X 10^24 M - 520 MILLION LIGHT-YEARS", "OUR HOME SUPERCLUSTER - 100,000 GALAXIES FLOWING TOWARD THE GREAT ATTRACTOR", 24.7f, -1, -1},
    {"BIGGEST CLUSTERS", "10^25 M - PHOENIX CLUSTER AND EL GORDO", "EACH WEIGHS MORE THAN A QUADRILLION SUNS - BILLIONS OF LIGHT-YEARS AWAY", 25.2f, -1, -1},
    {"TON 618", "66 BILLION SUNS - 10.8 BILLION LIGHT-YEARS AWAY", "ITS HORIZON IS 2,600 AU ACROSS - PLUTO'S ORBIT FITS INSIDE", 24.0f, -1, -1},
    {"PHOENIX A", "UP TO 100 BILLION SUNS", "MAYBE THE BIGGEST BLACK HOLE KNOWN - ESTIMATE DISPUTED", 24.0f, -1, -1},
    {"COSMIC WEB", "10^26 M - 10 BILLION LIGHT-YEARS", "GALAXIES TRACE A WEB OF FILAMENTS - THE HERCULES-CORONA BOREALIS WALL MAY SPAN 10 BILLION", 26.0f, -1, -1},
    {"HERCULES-CORONA BOREALIS GREAT WALL", "ABOUT 10 BILLION LIGHT-YEARS LONG", "THE BIGGEST KNOWN STRUCTURE - DISPUTED - A TENTH OF THE OBSERVABLE UNIVERSE", 26.0f, -1, -1},
    {"THE OBSERVABLE UNIVERSE", "8.8 X 10^26 M - THE COSMIC MICROWAVE BACKGROUND", "93 BILLION LIGHT-YEARS ACROSS - AND WE ARE THE TINY DOT IN THE MIDDLE", 26.95f, -1, -1},
    {"", "", "", 26.95f, -1, -1},
};
const int STOP_COUNT = 22;
const int FIRST_MACRO = 7;
const int TON = 16;
const int PHX = 17;
const int WEB = 18;
const int HCB = 19;
const int CMBSTOP = 20;
const int FINALE = 21;
const int ASIDE_FROM = -1;
const char* ASIDE = "TON 618'S HORIZON IS 2,600 AU - PLUTO'S ORBIT FITS INSIDE (SEE SCENE 4)";
const float HOLD = 4.0f;
const float HOLD_PHX = 8.5f;
const float HOLD_CMB = 5.5f;
const float HOLD_FINALE = 15.0f;
const float RISE = 4.5f;
const float TRANS = 5.0f;
const float FADE = 0.7f;
const float TAN30 = 0.57735027f;
const double LN5 = 1.6094379;

bool bigHole(int k) { return k == TON || k == PHX; }
const char* holeLabel(int k) { return k == TON ? "TON 618" : "PHOENIX CLUSTER"; }
float holdLen(int k) { return k == FINALE ? HOLD_FINALE : bigHole(k) ? HOLD_PHX : k == CMBSTOP ? HOLD_CMB : HOLD; }
float captionLen(int k) { return k == FINALE ? 0.0f : holdLen(k); }
float transLen(int k) { return k == CMBSTOP ? RISE : TRANS; }

float totalLen() {
    float t = 0.0f;
    for (int k = 0; k < STOP_COUNT; k++) t += holdLen(k) + (k < FINALE ? transLen(k) : 0.0f);
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

float smooth(float x) {
    x = std::min(1.0f, std::max(0.0f, x));
    return x * x * (3.0f - 2.0f * x);
}

struct Pose {
    double T[3];
    double logd;
    double back[3];
    double rise;
    double dpitch;
};

void norm3(double v[3]) {
    double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l < 1e-30) { v[0] = 0.0; v[1] = 0.0; v[2] = 1.0; return; }
    for (int i = 0; i < 3; i++) v[i] /= l;
}

double lenOf(const double v[3]) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

void cross3(const double a[3], const double b[3], double o[3]) {
    o[0] = a[1] * b[2] - a[2] * b[1];
    o[1] = a[2] * b[0] - a[0] * b[2];
    o[2] = a[0] * b[1] - a[1] * b[0];
}

double kLyPerUnit(const AppContext& app) {
    return (double)LY_PER_AU_INV / solarUnitsPerAu(app.trueScale);
}

void bodyRel(const AppContext& app, int i, double out[3]) {
    double k = kLyPerUnit(app);
    for (int a = 0; a < 3; a++)
        out[a] = app.displayValid ? ((double)app.displayBodies[i].pos[a] - (double)app.displayBodies[0].pos[a]) * k : 0.0;
}

bool labelPos(const char* text, double out[3]) {
    static std::vector<UniLabel> labels = universeLabels();
    for (const UniLabel& l : labels)
        if (std::strcmp(l.text, text) == 0) {
            for (int a = 0; a < 3; a++) out[a] = l.pos[a];
            return true;
        }
    for (int a = 0; a < 3; a++) out[a] = 0.0;
    return false;
}

void uniToSun(const AppContext& app, const double p[3], double out[3]) {
    for (int a = 0; a < 3; a++) out[a] = (p[a] - app.gal.originMly[a]) * 1e6;
}

void setBack(Pose& p, double x, double y, double z) {
    p.back[0] = x; p.back[1] = y; p.back[2] = z;
    norm3(p.back);
}

Pose stopPose(const AppContext& app, int k) {
    Pose p{};
    double up[3] = {0.0, 1.0, 0.0};
    double ly = (double)LY_PER_AU_INV;
    if (k <= FIRST_MACRO || k == FIRST_MACRO + 1) {
        double E[3], M[3];
        bodyRel(app, 3, E);
        bodyRel(app, 9, M);
        double sun[3] = {-E[0], -E[1], -E[2]};
        norm3(sun);
        double perp[3];
        cross3(up, sun, perp);
        norm3(perp);
        if (k <= FIRST_MACRO) {
            for (int a = 0; a < 3; a++) p.T[a] = E[a];
            setBack(p, sun[0] * 0.75 + perp[0] * 0.5, sun[1] * 0.75 + perp[1] * 0.5 + 0.35, sun[2] * 0.75 + perp[2] * 0.5);
            p.logd = std::log(std::max(1e-20, (double)bodyRadius(app.trueScale, 3) * kLyPerUnit(app) * 4.6));
        } else {
            double d[3] = {M[0] - E[0], M[1] - E[1], M[2] - E[2]};
            for (int a = 0; a < 3; a++) p.T[a] = E[a] + 0.5 * d[a];
            setBack(p, sun[0] * 0.3 + perp[0] * 0.2, 0.85 + sun[1] * 0.3, sun[2] * 0.3 + perp[2] * 0.2);
            p.logd = std::log(std::max(1e-20, lenOf(d) * 0.8));
        }
        return p;
    }
    if (k == 9) { setBack(p, 0.25, 0.85, 0.45); p.logd = std::log(78.0 * ly); return p; }
    if (k == 10) { setBack(p, 0.35, 0.7, 0.6); p.logd = std::log(4.5); return p; }
    if (k == 11) { setBack(p, 0.35, 0.7, 0.6); p.logd = std::log(14.0); return p; }
    if (k == 12) {
        float c[3], n[3];
        galaxyCentreLy(c);
        galaxyNormal(n);
        double nn[3] = {n[0], n[1], n[2]}, side[3];
        cross3(nn, up, side);
        norm3(side);
        for (int a = 0; a < 3; a++) p.T[a] = 0.5 * c[a];
        setBack(p, nn[0] * 0.77 + side[0] * 0.64, nn[1] * 0.77 + side[1] * 0.64, nn[2] * 0.77 + side[2] * 0.64);
        p.logd = std::log(52000.0);
        return p;
    }
    double u[3], v[3];
    if (k == 13) {
        labelPos("LOCAL GROUP", u);
        setBack(p, 0.3, 0.8, 0.5);
        p.logd = std::log(3.0e6);
    } else if (k == 14) {
        labelPos("GREAT ATTRACTOR", u);
        setBack(p, 0.3, 0.8, 0.5);
        p.logd = std::log(7.5e8);
    } else if (k == 15) {
        labelPos("PHOENIX CLUSTER", u);
        labelPos("EL GORDO", v);
        double sep[3] = {u[0] - v[0], u[1] - v[1], u[2] - v[2]};
        for (int a = 0; a < 3; a++) u[a] = 0.5 * (u[a] + v[a]);
        setBack(p, -u[0] / std::max(1.0, lenOf(u)) * 0.8, 0.3, -u[2] / std::max(1.0, lenOf(u)) * 0.8);
        p.logd = std::log(std::max(1.6e9, 1.4e6 * lenOf(sep)));
    } else if (bigHole(k)) {
        labelPos(holeLabel(k), u);
        setBack(p, 0.35, 0.45, 0.8);
        p.logd = std::log(4.5e7);
    } else if (k == WEB) {
        labelPos("HERCULES-CORONA BOREALIS GREAT WALL", u);
        for (int a = 0; a < 3; a++) u[a] *= 0.5;
        setBack(p, -u[0] * 0.8, std::max(0.0, 0.3 * lenOf(u)), -u[2] * 0.8);
        p.logd = std::log(1.1e10);
    } else if (k == HCB) {
        labelPos("HERCULES-CORONA BOREALIS GREAT WALL", u);
        double l = std::max(1.0, lenOf(u));
        setBack(p, -u[0] / l + 0.25, -u[1] / l + 0.3, -u[2] / l + 0.25);
        p.logd = std::log(7.5e9);
    } else {
        for (int a = 0; a < 3; a++) u[a] = 0.0;
        setBack(p, 0.35, 0.5, 0.8);
        p.logd = std::log(1.15e11);
        if (k == FINALE) {
            p.rise = 1.25;
            p.dpitch = 0.9;
        }
    }
    uniToSun(app, u, p.T);
    return p;
}

const double PHX_NEAR = 1.4e6;

double phxPush(const Where& w) {
    return smooth((w.local - 0.8f) / 3.0f);
}

Pose poseAt(const AppContext& app, const Where& w) {
    if (w.stop < FIRST_MACRO - 1 || (w.stop == FIRST_MACRO - 1 && w.hold)) return stopPose(app, FIRST_MACRO);
    float e = smooth(w.s);
    if (w.stop == FIRST_MACRO - 1) {
        Pose d = stopPose(app, FIRST_MACRO);
        d.logd += (double)(1.0f - e) * LN5;
        return d;
    }
    Pose a = stopPose(app, w.stop);
    if (bigHole(w.stop)) a.logd = w.hold ? a.logd + (std::log(PHX_NEAR) - a.logd) * phxPush(w) : std::log(PHX_NEAR);
    if (w.hold) return a;
    Pose b = stopPose(app, w.stop + 1);
    Pose p;
    p.rise = a.rise + (b.rise - a.rise) * e;
    p.dpitch = a.dpitch + (b.dpitch - a.dpitch) * e;
    p.logd = a.logd + (b.logd - a.logd) * e;
    for (int i = 0; i < 3; i++) {
        p.T[i] = a.T[i] + (b.T[i] - a.T[i]) * e;
        p.back[i] = a.back[i] + (b.back[i] - a.back[i]) * e;
    }
    norm3(p.back);
    return p;
}

void applyCamera(AppContext& app, const Pose& p) {
    Camera& c = app.camera;
    float sun[3];
    sunPosition(app, sun);
    double k = kLyPerUnit(app), d = std::exp(p.logd);
    double up[3] = {0.0, 1.0, 0.0}, bu = p.back[1], cu[3];
    for (int a = 0; a < 3; a++) cu[a] = up[a] - p.back[a] * bu;
    norm3(cu);
    for (int a = 0; a < 3; a++) c.pos[a] = (float)((double)sun[a] + (p.T[a] - cu[a] * 0.12 * d + p.back[a] * d + (a == 1 ? p.rise * d : 0.0)) / k);
    c.yaw = (float)std::atan2(p.back[0], p.back[2]);
    c.pitch = (float)(std::asin(std::max(-1.0, std::min(1.0, p.back[1]))) - p.dpitch);
    c.free = true;
    app.follow = -1;
    app.zoomValid = false;
    app.zoomWheel = 0.0;
    TourState& t = app.tour;
    for (int a = 0; a < 3; a++) t.pos[a] = c.pos[a];
    t.yaw = c.yaw;
    t.pitch = c.pitch;
}

void applyMicro(TourState& t, const Where& w) {
    t.sceneA = t.sceneB = 0.0f;
    t.mix = 0.0f;
    t.zoomA = t.zoomB = 1.0f;
    t.alpha = 0.0f;
    float e = smooth(w.s);
    const float L = (float)LN5;
    if (w.hold) {
        if (w.stop >= FIRST_MACRO) return;
        t.alpha = 1.0f;
        t.sceneA = (float)STOPS[w.stop].a;
        if (STOPS[w.stop].a != STOPS[w.stop].b) {
            float m = smooth((w.local - 1.6f) / 1.1f);
            t.sceneB = (float)STOPS[w.stop].b;
            t.mix = m;
            t.zoomA = 1.0f - 0.25f * m;
            t.zoomB = 1.3f - 0.3f * m;
        }
        return;
    }
    if (w.stop >= FIRST_MACRO) return;
    t.sceneA = (float)STOPS[w.stop].b;
    if (w.stop + 1 < FIRST_MACRO) {
        t.alpha = 1.0f;
        t.sceneB = (float)STOPS[w.stop + 1].a;
        t.mix = e;
        t.zoomA = std::exp(-e * L);
        t.zoomB = std::exp((1.0f - e) * L);
    } else {
        t.alpha = 1.0f - smooth((w.s - 0.1f) / 0.75f);
        t.zoomA = std::exp(-e * L);
    }
}

void applyState(AppContext& app) {
    TourState& t = app.tour;
    Where w = locate(t.t);
    applyCamera(app, poseAt(app, w));
    applyMicro(t, w);
    float e = smooth(w.s);
    float r0 = STOPS[w.stop].exp;
    if (bigHole(w.stop)) r0 = w.hold ? r0 - 1.8f * (float)phxPush(w) : r0 - 1.8f;
    t.ruler = w.hold ? r0 : r0 + (STOPS[w.stop + 1].exp - r0) * e;
}

}

void tourStart(AppContext& app, float at) {
    TourState& t = app.tour;
    if (t.mode == 0) t.gridWas = app.showGrid;
    app.showGrid = false;
    app.showGalaxy = true;
    app.showUniverse = true;
    app.showSky = false;
    t.mode = 1;
    t.ran = true;
    t.t = at;
    applyState(app);
}

void tourToggle(AppContext& app) {
    TourState& t = app.tour;
    if (t.mode == 1) t.mode = 2;
    else if (t.mode == 2) t.mode = 1;
}

void tourStop(AppContext& app) {
    TourState& t = app.tour;
    if (t.mode != 0) app.showGrid = t.gridWas;
    t.mode = 0;
    t.alpha = 0.0f;
}

void tourStep(AppContext& app, float dt) {
    TourState& t = app.tour;
    t.clock += std::min(dt, 0.1f);
    if (t.mode == 0 || !app.displayValid) return;
    const Camera& c = app.camera;
    bool moved = !c.free || app.follow >= 0 || app.visit.mode != 0 || app.scene != Scene::SOLAR;
    for (int a = 0; a < 3; a++) moved = moved || c.pos[a] != t.pos[a];
    moved = moved || c.yaw != t.yaw || c.pitch != t.pitch;
    if (moved) {
        tourStop(app);
        return;
    }
    if (t.mode == 1) t.t += std::min(dt, 0.1f);
    float total = totalLen();
    if (t.t >= total) {
        t.t = total;
        applyState(app);
        tourStop(app);
        return;
    }
    applyState(app);
}

float tourMicroFade(const AppContext& app) {
    if (app.tour.mode == 0) return 0.0f;
    Where w = locate(app.tour.t);
    if (!w.hold || w.stop >= FIRST_MACRO || w.local >= HOLD) return 0.0f;
    return smooth(std::min(w.local, HOLD - w.local) / FADE);
}

float tourOortAlpha(const AppContext& app) {
    if (app.tour.mode == 0) return 0.0f;
    Where w = locate(app.tour.t);
    if (w.stop == 9 && !w.hold) return smooth((w.s - 0.4f) / 0.5f);
    if (w.stop == 10) return w.hold ? 1.0f : 1.0f - smooth(w.s / 0.5f);
    return 0.0f;
}

float tourWallGlow(const AppContext& app) {
    if (app.tour.mode == 0) return 0.0f;
    Where w = locate(app.tour.t);
    if (w.stop == WEB && !w.hold) return smooth((w.s - 0.55f) / 0.45f);
    if (w.stop == HCB) return w.hold ? 1.0f : 1.0f - smooth(w.s / 0.4f);
    return 0.0f;
}

bool tourHole(const AppContext& app, float pos[3], float& radius) {
    if (app.tour.mode == 0) return false;
    Where w = locate(app.tour.t);
    if (!bigHole(w.stop)) return false;
    if (std::exp(poseAt(app, w).logd) > 5.0e6) return false;
    double u[3];
    labelPos(holeLabel(w.stop), u);
    for (int a = 0; a < 3; a++) pos[a] = (float)u[a];
    radius = 0.03f;
    return true;
}

bool tourMicroCovers(const AppContext& app) {
    return app.tour.mode != 0 && app.tour.alpha > 0.5f;
}

void emitTourLines(AppContext& app, OverlayVertex* out, uint32_t& used) {
    const TourState& t = app.tour;
    if (t.mode == 0) return;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float px = 2.0f / W, py = 2.0f / H, k = std::max(1.0f, H / 720.0f);
    float x0 = 16.0f, span = 64.0f * 4.0f * k, y = H - 24.0f;
    auto seg = [&](float xa, float ya, float xb, float yb, float r, float g, float b) {
        out[used++] = {-1.0f + xa * px, -1.0f + ya * py, r, g, b};
        out[used++] = {-1.0f + xb * px, -1.0f + yb * py, r, g, b};
    };
    seg(x0, y, x0 + span, y, 0.45f, 0.55f, 0.75f);
    for (int e = -36; e <= 28; e++) {
        float x = x0 + (float)(e + 36) * span / 64.0f;
        float len = (e % 10 == 0) ? 9.0f : (e % 5 == 0 ? 6.0f : 3.0f);
        seg(x, y, x, y - len * k, 0.45f, 0.55f, 0.75f);
    }
    float mx = x0 + (t.ruler + 36.0f) * span / 64.0f;
    seg(mx, y + 5.0f, mx, y - 14.0f * k, 1.0f, 0.75f, 0.3f);
    seg(mx - 1.0f, y + 5.0f, mx - 1.0f, y - 14.0f * k, 1.0f, 0.75f, 0.3f);
    emitMicroLines(app, out, used);
    emitOortLines(app, out, used);
}

void emitTourText(AppContext& app, OverlayVertex* out, uint32_t& used) {
    const TourState& t = app.tour;
    if (t.mode == 0) return;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float px = 2.0f / W, py = 2.0f / H, k = H / 720.0f;
    Where w = locate(t.t);
    if (w.hold && w.local < captionLen(w.stop)) {
        float a = std::max(0.0f, std::min(1.0f, std::min(w.local, captionLen(w.stop) - w.local) / FADE));
        a = a * a * (3.0f - 2.0f * a);
        const Stop& st = STOPS[w.stop];
        int s1 = fitScale(st.name, W, std::max(2, (int)std::lround(6.0f * k)));
        int s2 = fitScale(st.size, W, std::max(2, (int)std::lround(4.0f * k)));
        int s3 = fitScale(st.fact, W, std::max(1, (int)std::lround(3.0f * k)));
        float y = std::floor(0.72f * H);
        bigText(out, used, st.name, W, y, px, py, s1, 1.0f * a, 0.85f * a, 0.45f * a);
        y += 7.0f * (float)s1 + 14.0f * k;
        bigText(out, used, st.size, W, y, px, py, s2, 0.85f * a, 0.9f * a, 1.0f * a);
        y += 7.0f * (float)s2 + 12.0f * k;
        bigText(out, used, st.fact, W, y, px, py, s3, 1.0f * a, 0.65f * a, 0.5f * a);
    }
    if (w.stop == FINALE && w.hold) {
        float f = w.local;
        float fo = smooth((HOLD_FINALE - f) / 0.7f);
        const char* L[5] = {"YOU ARE MADE OF STARDUST.",
                            "NOW THE STARS LOOK BACK THROUGH YOUR EYES.",
                            "EVERY ATOM OF YOU WAS FORGED INSIDE A STAR THAT DIED,",
                            "SO THAT ONE DAY, SOMETHING COULD LOOK UP - AND WONDER.",
                            "I AM THE UNIVERSE. AND SO ARE YOU."};
        const float T0[5] = {0.3f, 2.6f, 5.2f, 7.4f, 10.2f};
        const float SZ[5] = {12.0f, 5.0f, 3.6f, 3.6f, 6.0f};
        const float GAP[5] = {26.0f, 30.0f, 14.0f, 40.0f, 0.0f};
        const float C[5][3] = {{1.0f, 0.85f, 0.45f}, {0.85f, 0.9f, 1.0f}, {1.0f, 0.7f, 0.5f}, {1.0f, 0.7f, 0.5f}, {1.0f, 1.0f, 1.0f}};
        float y = std::floor(0.2f * H);
        for (int i = 0; i < 5; ++i) {
            float ai = smooth((f - T0[i]) / 1.1f) * fo;
            int si = fitScale(L[i], W, std::max(i == 0 ? 3 : 1, (int)std::lround(SZ[i] * k)));
            bigText(out, used, L[i], W, y, px, py, si, C[i][0] * ai, C[i][1] * ai, C[i][2] * ai);
            y += 7.0f * (float)si + GAP[i] * k;
        }
    }
    if (!w.hold && w.stop == ASIDE_FROM) {
        float a = std::min(smooth((w.s - 0.12f) / 0.15f), smooth((0.9f - w.s) / 0.15f));
        if (a > 0.01f) {
            int s = fitScale(ASIDE, W, std::max(2, (int)std::lround(4.0f * k)));
            bigText(out, used, ASIDE, W, std::floor(0.78f * H), px, py, s, 1.0f * a, 0.65f * a, 0.5f * a);
        }
    }
    emitMicroText(app, out, used);
    emitOortText(app, out, used);
    char buf[48];
    std::snprintf(buf, sizeof(buf), "SCALE  10^%d M", (int)std::lround(t.ruler));
    emitText(out, used, buf, -1.0f + 16.0f * px, -1.0f + (H - 52.0f * std::max(1.0f, k)) * py, px, py, 2.0f, 1.0f, 0.85f, 0.45f);
    const char* hint = t.mode == 1 ? "SPACE PAUSE  R RESTART  ESC EXIT" : "PAUSED - SPACE RESUME  R RESTART  ESC EXIT";
    float len = (float)std::strlen(hint) * 6.0f;
    emitText(out, used, hint, -1.0f + (W - len - 14.0f) * px, -1.0f + (H - 20.0f) * py, px, py, 1.0f, 0.7f, 0.75f, 0.85f);
}
