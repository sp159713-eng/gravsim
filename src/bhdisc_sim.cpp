#include "vk_app.h"
#include "bhdisc.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

float g_bdAtSec = -1.0f;
int g_bdScen = 0;

namespace {

BdState& S = bdState();

struct RatePt { double t, r; };
const RatePt RATE[] = {{0, 260}, {5, 260}, {6.2, 90}, {12, 90}, {15, 220}, {19, 280}, {1000, 280}};
std::vector<double> g_phi;
const double PHI_STEP = 0.01;

double rateAt(double t) {
    const int n = (int)(sizeof(RATE) / sizeof(RATE[0]));
    if (t <= RATE[0].t) return RATE[0].r;
    for (int i = 1; i < n; i++)
        if (t <= RATE[i].t) return RATE[i - 1].r + (RATE[i].r - RATE[i - 1].r) * (t - RATE[i - 1].t) / (RATE[i].t - RATE[i - 1].t);
    return RATE[n - 1].r;
}

void buildPhi() {
    if (!g_phi.empty()) return;
    g_phi.push_back(0.0);
    for (int i = 1; i <= 40000; i++) {
        double t = i * PHI_STEP;
        g_phi.push_back(g_phi.back() + 0.5 * (rateAt(t) + rateAt(t - PHI_STEP)) * PHI_STEP);
    }
}

bool keyDown(AppContext& app, int k) { return glfwGetKey(app.window, k) == GLFW_PRESS; }

bool edge(int i, bool down) {
    bool fire = down && !S.keyPrev[i];
    S.keyPrev[i] = down;
    return fire;
}

int binOf(double ts) { return std::clamp((int)(ts / BD_BIN_S), 0, BD_BINS - 1); }

void pushInit() {
    BdOp op{};
    op.kind = 0;
    bdFillStep(op.push, S.com, S.scen, 2, 0, false);
    S.ops.push_back(op);
}

void restart(int scen) {
    S.scen = scen;
    S.epoch++;
    S.chunk = 0;
    S.ts = 0.0;
    S.tw = 0.0;
    S.seek = false;
    S.discNow = 0.0f;
    S.lumNow = 0.0f;
    S.jet = 0.0f;
    S.bins.clearFrom(0);
    for (BdSnap& s : S.snap) s.valid = false;
    S.ops.clear();
    bdComInit(S.com, scen);
    pushInit();
}

void restore(int idx) {
    S.epoch++;
    S.ops.push_back({2, idx, {}});
    S.chunk = (uint32_t)idx * BD_SNAP_EVERY;
    S.com = S.snap[idx].com;
    S.ts = bdPhiInv((double)S.chunk);
    S.bins.clearFrom(binOf(S.ts));
}

void consumeStats(int f) {
    BdSlot& sl = S.slot[f];
    uint32_t* m = S.statMap[f];
    if (!m) return;
    if (sl.pending && sl.epoch == S.epoch && sl.tsEnd < BD_BINS * BD_BIN_S) {
        int b = binOf(sl.tsEnd);
        S.bins.eaten[b] += (float)m[0];
        S.escaped += m[1];
        S.bins.shock[b] += (float)m[3] / 4000.0f;
        if (sl.hasLast) {
            S.discNow = (float)m[2];
            S.bins.disc[b] += (float)m[2];
            S.bins.lum[b] += (float)m[5] / 64.0f;
            S.bins.cnt[b] += 1.0f;
        }
    }
    sl.pending = false;
    std::memset(m, 0, 64);
}

void runChunks(uint32_t n) {
    for (uint32_t i = 0; i < n; i++) {
        BdOp op{};
        op.kind = 1;
        bdFillStep(op.push, S.com, S.scen, 1, S.chunk, i + 1 == n);
        S.ops.push_back(op);
        bdComStep(S.com, S.scen);
        S.chunk++;
        if (S.chunk % BD_SNAP_EVERY == 0) {
            int idx = (int)(S.chunk / BD_SNAP_EVERY);
            if (idx < BD_SNAPS) {
                S.snap[idx].valid = true;
                S.snap[idx].com = S.com;
                S.ops.push_back({3, idx, {}});
            }
        }
    }
}

void bdCamera(AppContext& app, float dt) {
    Camera& c = app.camera;
    float wheel = c.zoomImpulse;
    c.zoomImpulse = 0.0f;
    float key = (keyDown(app, GLFW_KEY_PAGE_UP) ? 1.0f : 0.0f) - (keyDown(app, GLFW_KEY_PAGE_DOWN) ? 1.0f : 0.0f);
    S.logUser = std::clamp(S.logUser - 0.12f * wheel - 1.2f * key * dt, -1.6f, 1.2f);
    double t = S.ts;
    float want = t < 6.0 ? 200.0f : t < 24.0 ? 185.0f : t < 60.0 ? 150.0f : t < 100.0 ? 125.0f : 105.0f;
    if (S.scen == 2) want = 140.0f;
    if (S.scen == 1) want = 190.0f;
    S.camDist += (want - S.camDist) * (1.0f - std::exp(-0.7f * dt));
    float d = S.camDist * std::exp(S.logUser);
    c.yaw += 0.035f * dt;
    c.free = true;
    c.dist = d;
    for (int a = 0; a < 3; a++) c.target[a] = 0.0f;
    float cp = std::cos(c.pitch), sp = std::sin(c.pitch), sy = std::sin(c.yaw), cy = std::cos(c.yaw);
    c.pos[0] = d * cp * sy;
    c.pos[1] = d * sp;
    c.pos[2] = d * cp * cy;
    c.nearPlane = 0.5f;
    c.zoomImpulse = 0.0f;
}

}

double bdPhi(double ts) {
    buildPhi();
    if (ts >= 399.99) return g_phi.back() + RATE[6].r * (ts - 400.0);
    double x = std::clamp(ts, 0.0, 399.99) / PHI_STEP;
    int i = (int)x;
    return g_phi[i] + (g_phi[i + 1] - g_phi[i]) * (x - i);
}

double bdPhiInv(double c) {
    buildPhi();
    if (c >= g_phi.back()) return 400.0 + (c - g_phi.back()) / RATE[6].r;
    int lo = 0, hi = (int)g_phi.size() - 1;
    while (hi - lo > 1) {
        int mid = (lo + hi) / 2;
        if (g_phi[mid] <= c) lo = mid;
        else hi = mid;
    }
    double f = (c - g_phi[lo]) / std::max(g_phi[lo + 1] - g_phi[lo], 1e-9);
    return (lo + std::clamp(f, 0.0, 1.0)) * PHI_STEP;
}

void bdComInit(BdCom& c, int scen) {
    c = BdCom();
    if (scen == 2) return;
    double rp = scen == 0 ? 16.0 : 20.0;
    double r0 = scen == 0 ? 130.0 : 120.0;
    double p = 2.0 * rp;
    double th = -std::acos(std::clamp(p / r0 - 1.0, -1.0, 1.0));
    double vr = std::sqrt(1.0 / p) * std::sin(th);
    double vt = std::sqrt(1.0 / p) * (1.0 + std::cos(th));
    double ph = -0.37;
    double x = r0 * std::cos(th), z = r0 * std::sin(th);
    double vx = vr * std::cos(th) - vt * std::sin(th), vz = vr * std::sin(th) + vt * std::cos(th);
    double cs = std::cos(ph), sn = std::sin(ph);
    double f = 0.997;
    c.x[0] = x * cs - z * sn;
    c.x[2] = x * sn + z * cs;
    c.v[0] = f * (vx * cs - vz * sn);
    c.v[2] = f * (vx * sn + vz * cs);
    c.k = scen == 0 ? 1.45e-4 : 0.0;
}

void bdComStep(BdCom& c, int scen) {
    if (scen == 2) return;
    const int sub = 16;
    double h = BD_DT / sub;
    auto acc = [](const double* x, double* a) {
        double r = std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
        double rr = std::max(r - 2.0, 0.35);
        for (int i = 0; i < 3; i++) a[i] = -x[i] / r / (rr * rr);
    };
    for (int s = 0; s < sub; s++) {
        double a[3];
        acc(c.x, a);
        for (int i = 0; i < 3; i++) c.v[i] += 0.5 * h * a[i];
        for (int i = 0; i < 3; i++) c.x[i] += h * c.v[i];
        acc(c.x, a);
        for (int i = 0; i < 3; i++) c.v[i] += 0.5 * h * a[i];
    }
    if (scen == 0) {
        double r = std::sqrt(c.x[0] * c.x[0] + c.x[1] * c.x[1] + c.x[2] * c.x[2]);
        if (!c.fading && r < 22.0) c.fading = true;
        if (c.fading) c.fade += BD_DT;
    }
}

void bdFillStep(BdStepPush& p, const BdCom& c, int scen, uint32_t mode, uint32_t chunk, bool last) {
    std::memset(&p, 0, sizeof(p));
    double k = scen == 0 && !c.fading ? 1.0 : 0.0;
    for (int i = 0; i < 3; i++) {
        p.com[i] = (float)c.x[i];
        p.comv[i] = (float)c.v[i];
    }
    p.com[3] = (float)k;
    p.src[0] = 70.0f;
    p.srcv[0] = -0.015f;
    p.srcv[2] = 0.078f;
    p.mode = mode;
    p.n = BD_N;
    p.chunk = chunk;
    p.scen = (uint32_t)scen;
    p.dt = (float)BD_DT;
    p.last = last ? 1.0f : 0.0f;
    p.spawnP = 0.0002f;
    double t0 = scen == 2 ? 300.0 : 2600.0;
    double ramp = std::clamp(((double)chunk - t0) / 1000.0, 0.0, 1.0);
    p.visc = (float)ramp;
}

float bdRate(double ts, float win) {
    int b1 = binOf(ts);
    int nb = std::max(1, (int)(win / BD_BIN_S));
    float sum = 0.0f;
    int cnt = 0;
    for (int b = std::max(0, b1 - nb); b < b1; b++, cnt++) sum += S.bins.eaten[b];
    return cnt ? sum / (cnt * BD_BIN_S) : 0.0f;
}

void bdEnter(AppContext& app) {
    bdCreateVk(app);
    S.active = true;
    S.clock = 0.0;
    S.paused = false;
    S.backScrub = false;
    S.logUser = 0.0f;
    S.camDist = 200.0f;
    for (bool& k : S.keyPrev) k = true;
    for (BdSlot& s : S.slot) s.pending = false;
    restart(g_bdScen);
    g_bdScen = 0;
    if (g_bdAtSec >= 0.0f) {
        S.tw = (double)g_bdAtSec;
        S.bigCap = true;
        S.atHold = g_bdAtSec > 0.5f;
        S.seek = true;
    }
    g_bdAtSec = -1.0f;
    app.showSky = true;
    app.camera.yaw = 0.35f;
    app.camera.pitch = 0.36f;
    app.camera.free = true;
    app.camera.zoomImpulse = 0.0f;
    app.camera.wheelHold = false;
}

void bdLeave(AppContext&) {
    S.active = false;
    S.ops.clear();
}

void bdUpdate(AppContext& app, float dtIn) {
    float dt = std::min(dtIn, 0.1f);
    if (!S.created) return;
    S.clock += dt;
    consumeStats(app.currentFrame);
    bool shift = keyDown(app, GLFW_KEY_LEFT_SHIFT) || keyDown(app, GLFW_KEY_RIGHT_SHIFT);
    if (edge(0, keyDown(app, GLFW_KEY_SPACE))) S.paused = !S.paused;
    if (edge(1, keyDown(app, GLFW_KEY_R))) restart(S.scen);
    for (int k = 0; k < 3; k++)
        if (edge(2 + k, keyDown(app, GLFW_KEY_1 + k) || keyDown(app, GLFW_KEY_KP_1 + k))) {
            restart(k);
            S.feedFlash = 3.0f;
        }
    S.feedFlash = std::max(0.0f, S.feedFlash - dt);
    double rate = 6.0 * (shift ? 3.0 : 1.0);
    bool left = keyDown(app, GLFW_KEY_LEFT), right = keyDown(app, GLFW_KEY_RIGHT);
    S.backScrub = left && !right;
    if (right && !left) {
        S.tw += rate * dt;
        S.seek = true;
    } else if (S.backScrub) {
        S.tw = std::max(0.0, S.tw - rate * dt);
        S.seek = true;
    } else if (!S.paused && !S.bigCap) {
        S.tw += dt;
        if (!S.seek) S.tw = std::min(S.tw, S.ts + 2.5);
    }
    uint32_t target = (uint32_t)std::floor(bdPhi(S.tw));
    if (S.backScrub) {
        if (target < S.chunk) {
            int idx = std::min((int)(target / BD_SNAP_EVERY), BD_SNAPS - 1);
            while (idx > 0 && !S.snap[idx].valid) idx--;
            if (idx == 0 && S.chunk != 0) {
                S.epoch++;
                S.chunk = 0;
                S.com = BdCom();
                bdComInit(S.com, S.scen);
                S.ts = 0.0;
                S.bins.clearFrom(0);
                pushInit();
            } else if (idx > 0 && (uint32_t)idx * BD_SNAP_EVERY != S.chunk) {
                restore(idx);
            }
        }
    } else if (target > S.chunk) {
        uint32_t n = std::min<uint32_t>(target - S.chunk, S.bigCap ? BD_CAP_SEEK : BD_CAP);
        runChunks(n);
        S.ts = bdPhiInv((double)S.chunk);
        BdSlot& sl = S.slot[app.currentFrame];
        sl.pending = true;
        sl.epoch = S.epoch;
        sl.tsEnd = (float)S.ts;
        sl.hasLast = true;
        if (S.chunk >= target) {
            S.seek = false;
            S.bigCap = false;
            if (S.atHold) S.paused = true;
            S.atHold = false;
        }
    } else {
        S.seek = false;
    }
    if (S.chunk != 0 || !S.ops.empty()) S.ts = bdPhiInv((double)S.chunk);
    float t = (float)S.ts;
    float sm = std::clamp((t - 40.0f) / 15.0f, 0.0f, 1.0f);
    float fall = 1.0f - std::clamp((t - 85.0f) / 30.0f, 0.0f, 1.0f);
    float want = S.scen == 2 ? std::clamp((t - 25.0f) / 15.0f, 0.0f, 1.0f) * 0.6f : sm * sm * (3.0f - 2.0f * sm) * fall * 0.7f;
    S.jet += (want - S.jet) * (1.0f - std::exp(-1.5f * dt));
    bdCamera(app, dt);
}
