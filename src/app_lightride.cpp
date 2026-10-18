#include "vk_app.h"
#include "ephem.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

double g_lrStartAt = -1.0;

namespace {

const double PI = 3.14159265358979323846;
const double SUN_R_AU = 696340.0 / LR_KM_AU;
const double REAL_AU[8] = {0.387, 0.723, 1.0, 1.524, 5.203, 9.537, 19.19, 30.07};
const char* NAMES[8] = {"MERCURY", "VENUS", "EARTH", "MARS", "JUPITER", "SATURN", "URANUS", "NEPTUNE"};
const char* MEAN[8] = {"3.2 MIN", "6 MIN", "8 MIN 20 S", "12.7 MIN", "43 MIN", "1 H 20 MIN", "2 H 40 MIN", "4 H 10 MIN"};
const double CAPTION_S = 7.0;
const double DWELL_WARP = 30.0;

double wrapAngle(double a) {
    while (a > PI) a -= 2.0 * PI;
    while (a < -PI) a += 2.0 * PI;
    return a;
}

bool edge(bool now, bool& prev) {
    bool r = now && !prev;
    prev = now;
    return r;
}

}

void lrFormatDuration(double s, char* buf, size_t n) {
    if (s < 60.0) std::snprintf(buf, n, "%.0f S", s);
    else if (s < 3600.0) {
        int m = (int)(s / 60.0), r = (int)std::lround(s - m * 60.0);
        if (r == 60) { m++; r = 0; }
        if (r == 0) std::snprintf(buf, n, "%d MIN", m);
        else std::snprintf(buf, n, "%d MIN %d S", m, r);
    } else if (s < 86400.0) {
        int h = (int)(s / 3600.0), m = (int)std::lround((s - h * 3600.0) / 60.0);
        if (m == 60) { h++; m = 0; }
        std::snprintf(buf, n, "%d H %d MIN", h, m);
    } else if (s < 2.0 * LR_YEAR_S) std::snprintf(buf, n, "%.0f DAYS", s / 86400.0);
    else std::snprintf(buf, n, "%.2f YEARS", s / LR_YEAR_S);
}

void lrFormatClock(double s, char* buf, size_t n) {
    if (s < 3600.0) {
        int sec = (int)s;
        std::snprintf(buf, n, "%02d:%02d", sec / 60, sec % 60);
    } else if (s < 360000.0) {
        int mins = (int)(s / 60.0);
        std::snprintf(buf, n, "%02d:%02d", mins / 60, mins % 60);
    } else if (s < 2.0 * LR_YEAR_S) std::snprintf(buf, n, "%.1f DAYS", s / 86400.0);
    else std::snprintf(buf, n, "%.2f YEARS", s / LR_YEAR_S);
}

void lrFormatKm(double km, char* buf, size_t n) {
    if (km < 1e12) {
        long long v = (long long)std::llround(km);
        char tmp[40];
        std::snprintf(tmp, sizeof tmp, "%lld", v);
        std::string s = tmp, o;
        int c = 0;
        for (int i = (int)s.size() - 1; i >= 0; i--) {
            o.insert(o.begin(), s[i]);
            if (++c % 3 == 0 && i > 0) o.insert(o.begin(), ',');
        }
        std::snprintf(buf, n, "%s", o.c_str());
    } else std::snprintf(buf, n, "%.3E", km);
}

void lrPlanetPos(const LightRideState& S, int i, double t, double out[3]) {
    OrbitState o = ephemPlanet(i, S.jd0 + t / 86400.0, REAL_AU[i], 1.0);
    out[0] = o.pos[0];
    out[1] = o.pos[1];
    out[2] = o.pos[2];
}

void lrBuildPath(LightRideState& S) {
    LrPath& P = S.path;
    P.x.clear();
    P.y.clear();
    P.z.clear();
    double E[3], T[3];
    double te = 500.0;
    for (int it = 0; it < 4; it++) {
        lrPlanetPos(S, 2, te, E);
        te = (std::hypot(E[0], E[2]) - SUN_R_AU) / LR_C_AU_S;
    }
    S.tEarth = te;
    lrPlanetPos(S, 7, 14900.0, T);
    double el = std::hypot(E[0], E[2]);
    double ux = E[0] / el, uz = E[2] / el;
    double sx = ux * SUN_R_AU * 1.003, sz = uz * SUN_R_AU * 1.003;
    double ex = E[0] - uz * 0.0013, ez = E[2] + ux * 0.0013;
    double tx = T[0], tz = T[2];
    double tl = std::hypot(tx - ex, tz - ez);
    tx += -(tz - ez) / tl * 0.006;
    tz += (tx - ex) / tl * 0.006;
    double theta = std::atan2(ez - sz, ex - sx);
    double sE = std::hypot(ex - sx, ez - sz);
    double px = sx, pz = sz, s = 0.0;
    const double omega = 0.5;
    double bump0 = sE, bump1 = 0.0;
    bool steering = false;
    while (true) {
        if (!steering && s >= sE) {
            steering = true;
            bump0 = s;
            bump1 = s + std::hypot(tx - px, tz - pz) * 1.03;
        }
        double yy = 0.0;
        if (steering && s < bump1) {
            double u = (s - bump0) / (bump1 - bump0);
            double w = std::sin(PI * u);
            yy = 3.0 * w * w;
        }
        P.x.push_back(px);
        P.y.push_back(yy);
        P.z.push_back(pz);
        if (steering) {
            double want = std::atan2(tz - pz, tx - px);
            double d = wrapAngle(want - theta);
            double lim = omega * LR_PATH_DS;
            theta += std::max(-lim, std::min(lim, d));
        }
        px += std::cos(theta) * LR_PATH_DS;
        pz += std::sin(theta) * LR_PATH_DS;
        s += LR_PATH_DS;
        if (steering && std::hypot(tx - px, tz - pz) < 0.05) break;
        if (s > 80.0) break;
    }
    P.endS = s;
    P.x.push_back(px);
    P.y.push_back(0.0);
    P.z.push_back(pz);
    P.x.push_back(px + std::cos(theta) * LR_PATH_DS);
    P.y.push_back(0.0);
    P.z.push_back(pz + std::sin(theta) * LR_PATH_DS);
}

void lrPose(LightRideState& S, double t) {
    const LrPath& P = S.path;
    double s = t * LR_C_AU_S;
    int last = (int)P.x.size() - 2;
    double fi = s / LR_PATH_DS;
    int i = (int)std::floor(fi);
    auto dirAt = [&](int k, double d[3]) {
        int a = std::max(0, k - 1), b = std::min((int)P.x.size() - 1, k + 1);
        d[0] = P.x[b] - P.x[a];
        d[1] = P.y[b] - P.y[a];
        d[2] = P.z[b] - P.z[a];
        double l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (l < 1e-12) l = 1.0;
        d[0] /= l; d[1] /= l; d[2] /= l;
    };
    if (i >= last) {
        double d[3];
        dirAt(last, d);
        double extra = s - last * LR_PATH_DS;
        S.pos[0] = P.x[last] + d[0] * extra;
        S.pos[1] = P.y[last] + d[1] * extra;
        S.pos[2] = P.z[last] + d[2] * extra;
        for (int a = 0; a < 3; a++) S.fwd[a] = d[a];
        return;
    }
    double f = fi - i;
    S.pos[0] = P.x[i] + (P.x[i + 1] - P.x[i]) * f;
    S.pos[1] = P.y[i] + (P.y[i + 1] - P.y[i]) * f;
    S.pos[2] = P.z[i] + (P.z[i + 1] - P.z[i]) * f;
    double d0[3], d1[3];
    dirAt(i, d0);
    dirAt(i + 1, d1);
    double l = 0.0;
    for (int a = 0; a < 3; a++) {
        S.fwd[a] = d0[a] + (d1[a] - d0[a]) * f;
        l += S.fwd[a] * S.fwd[a];
    }
    l = std::sqrt(l);
    for (int a = 0; a < 3; a++) S.fwd[a] /= l;
}

void lrBuildMilestones(LightRideState& S) {
    S.ms.clear();
    char dur[48], buf[200];
    double e[3];
    for (int i = 0; i < 8; i++) {
        lrPlanetPos(S, i, 0.0, e);
        double r = std::sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
        double t = r / LR_C_AU_S;
        if (i == 2) t = S.tEarth;
        if (i == 7) t = (S.path.endS + 0.05) / LR_C_AU_S;
        lrFormatDuration(t, dur, sizeof dur);
        std::snprintf(buf, sizeof buf, "%s - %s (MEAN %s)", NAMES[i], dur, MEAN[i]);
        std::string body = i == 2 ? "THE SUNLIGHT ON YOUR SKIN LEFT THE SUN 8 MINUTES AGO" : "";
        S.ms.push_back({buf, body, t, i});
    }
    double vgAu = 173.1 - 3.57 * (2461360.0 - S.jd0) / 365.25;
    S.ms.push_back({"VOYAGER 1 - ABOUT 23 HOURS", "THE FARTHEST HUMAN-MADE OBJECT IS ONLY A LIGHT-DAY AWAY", vgAu / LR_C_AU_S, -1});
    S.ms.push_back({"OORT CLOUD - ABOUT 1 YEAR", "THE ICY SHELL AT THE EDGE OF THE SUN'S GRAVITY", LR_YEAR_S, -1});
    S.ms.push_back({"PROXIMA CENTAURI - 4.24 YEARS", "THE NEAREST OTHER STAR. AT THIS SPEED THE TRIP TAKES 4.24 YEARS", 4.24 * LR_YEAR_S, -1});
    std::sort(S.ms.begin(), S.ms.end(), [](const LrMilestone& a, const LrMilestone& b) { return a.t < b.t; });
}

void lrMakeBasis(const LightRideState& S, LrBasis& b) {
    double f[3] = {S.fwd[0], S.fwd[1], S.fwd[2]};
    double sgn = S.backAmt;
    double yaw = std::atan2(-f[0], -f[2]);
    double pitch = -std::asin(std::max(-1.0, std::min(1.0, f[1])));
    yaw += S.yawOff + PI * sgn;
    pitch = pitch * (1.0 - 2.0 * sgn) + S.pitchOff;
    b.f[0] = -std::cos(pitch) * std::sin(yaw);
    b.f[1] = -std::sin(pitch);
    b.f[2] = -std::cos(pitch) * std::cos(yaw);
    double rl = std::sqrt(b.f[0] * b.f[0] + b.f[2] * b.f[2]);
    b.r[0] = -b.f[2] / rl;
    b.r[1] = 0.0;
    b.r[2] = b.f[0] / rl;
    b.u[0] = b.r[1] * b.f[2] - b.r[2] * b.f[1];
    b.u[1] = b.r[2] * b.f[0] - b.r[0] * b.f[2];
    b.u[2] = b.r[0] * b.f[1] - b.r[1] * b.f[0];
}

void lrEnter(AppContext& app) {
    LightRideState& S = app.lr;
    bool keepStart = g_lrStartAt >= 0.0;
    S = LightRideState{};
    S.ready = true;
    S.jd0 = ephemJd(app.tm.launchDay);
    lrBuildPath(S);
    lrBuildMilestones(S);
    S.t = 0.0;
    if (keepStart) {
        S.t = g_lrStartAt;
        S.frozen = true;
        S.backAmt = 0.0f;
        S.realT = g_lrStartAt > 0.0 ? 60.0 : 0.0;
        g_lrStartAt = -1.0;
        for (int i = 0; i < (int)S.ms.size(); i++)
            if (S.ms[i].t <= S.t + 2.0) {
                S.capIdx = i;
                S.capAge = 0.6;
            }
    }
    lrPose(S, S.t);
    Camera& c = app.camera;
    c = Camera{};
    c.free = true;
    c.pos[0] = c.pos[1] = c.pos[2] = 0.0f;
    c.nearPlane = 0.01f;
    c.minDist = 1.0f;
}

void lrUpdate(AppContext& app, float dt, bool mouseNow, bool escHeld) {
    LightRideState& S = app.lr;
    GLFWwindow* w = app.window;
    app.camera.zoomImpulse = 0.0f;
    dt = std::min(dt, 0.1f);
    auto key = [&](int k) { return glfwGetKey(w, k) == GLFW_PRESS; };
    if (edge(escHeld, S.escPrev) && !app.showHelp) S.exitReq = true;
    bool plus = key(GLFW_KEY_EQUAL) || key(GLFW_KEY_KP_ADD);
    bool minus = key(GLFW_KEY_MINUS) || key(GLFW_KEY_KP_SUBTRACT);
    if (edge(plus, S.plusPrev) || edge(minus, S.minusPrev)) {
        if (S.autoWarp) S.wexp = std::max(0, std::min(7, (int)std::lround(std::log10(std::max(1.0, S.warp)))));
        S.autoWarp = false;
        S.frozen = false;
        S.wexp = std::max(0, std::min(7, S.wexp + (plus ? 1 : -1)));
    }
    if (edge(key(GLFW_KEY_A), S.autoPrev)) {
        S.autoWarp = true;
        S.frozen = false;
    }
    if (edge(key(GLFW_KEY_V), S.viewPrev)) {
        S.backTouched = true;
        S.back = !S.back;
    }
    if (edge(key(GLFW_KEY_R), S.restartPrev)) {
        lrEnter(app);
        return;
    }
    if (edge(key(GLFW_KEY_TAB), S.tabPrev)) {
        int next = -1;
        for (int i = 0; i < (int)S.ms.size(); i++)
            if (S.ms[i].t > S.t + 1.0) {
                next = i;
                break;
            }
        S.frozen = false;
        if (next < 0) {
            S.t = 0.0;
            S.realT = 0.0;
            S.capIdx = -1;
        } else {
            S.t = S.ms[next].t;
            S.capIdx = next;
            S.capAge = 0.0;
            S.realT = std::max(S.realT, 61.0);
        }
    }
    bool run = !S.paused && !S.frozen;
    if (run) S.realT += dt;
    if (S.autoWarp) S.warp = S.realT < 60.0 ? 1.0 : std::max(100.0, S.t / 8.0);
    else S.warp = std::pow(10.0, (double)S.wexp);
    S.warp = std::min(S.warp, 1e7);
    if (S.capIdx >= 0 && S.capAge < CAPTION_S && S.autoWarp && S.realT >= 60.0) S.warp = std::min(S.warp, DWELL_WARP);
    if (run) {
        double t0 = S.t;
        S.t += dt * S.warp;
        for (int i = 0; i < (int)S.ms.size(); i++)
            if (S.ms[i].t > t0 && S.ms[i].t <= S.t) {
                S.capIdx = i;
                S.capAge = 0.0;
            }
        S.capAge += dt;
    }
    lrPose(S, S.t);

    if (S.backTouched) {
        float goal = S.back ? 1.0f : 0.0f;
        S.backAmt += (goal - S.backAmt) * std::min(1.0f, dt * 4.0f);
    }

    if (mouseNow && S.dragPrev && app.cursorOk) {
        S.yawOff -= (float)(app.cursorFb[0] - S.lastX) * 0.004f;
        S.pitchOff += (float)(app.cursorFb[1] - S.lastY) * 0.004f;
        S.pitchOff = std::max(-1.4f, std::min(1.4f, S.pitchOff));
    }
    S.dragPrev = mouseNow && app.cursorOk;
    S.lastX = app.cursorFb[0];
    S.lastY = app.cursorFb[1];

    LrBasis b;
    lrMakeBasis(S, b);
    Camera& c = app.camera;
    c.free = true;
    c.pos[0] = c.pos[1] = c.pos[2] = 0.0f;
    c.yaw = (float)std::atan2(-b.f[0], -b.f[2]);
    c.pitch = (float)-std::asin(std::max(-1.0, std::min(1.0, b.f[1])));
}
