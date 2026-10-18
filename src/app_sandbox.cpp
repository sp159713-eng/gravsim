#include "vk_app.h"
#include "massctl.h"
#include <algorithm>
#include <cmath>

bool g_sandboxDemo = false;
bool g_sandboxSolar = false;
int g_sandboxPreset = 0;

namespace {

const double GM_SUN = 10000.0;
const double SOFT2 = 1.0;
const double TIME_RATE = 2.0;
const double KILL_R = 5000.0;
const double VEL_PER_UNIT = 0.5;
const size_t MAX_BODIES = 120;

struct TypeInfo {
    double mass, radius;
    float col[3];
};

const TypeInfo TYPES[4] = {
    {1.0, 1.8, {0.45f, 0.65f, 1.0f}},
    {40.0, 3.6, {1.0f, 0.68f, 0.38f}},
    {2000.0, 5.8, {1.0f, 0.93f, 0.55f}},
    {4000.0, 3.0, {0.0f, 0.0f, 0.0f}},
};

float rnd(SandboxState& S) {
    S.rng = S.rng * 1664525u + 1013904223u;
    return (float)(S.rng >> 8) / 16777216.0f;
}

template <class T>
void accel(const std::vector<T>& b, std::vector<double>& ax, std::vector<double>& ay, std::vector<double>& az) {
    size_t n = b.size();
    ax.assign(n, 0.0);
    ay.assign(n, 0.0);
    az.assign(n, 0.0);
    for (size_t i = 0; i < n; i++)
        for (size_t j = i + 1; j < n; j++) {
            double dx = b[j].x - b[i].x, dy = b[j].y - b[i].y, dz = b[j].z - b[i].z;
            double d2 = dx * dx + dy * dy + dz * dz + SOFT2;
            double inv = 1.0 / (d2 * std::sqrt(d2));
            ax[i] += b[j].m * dx * inv;
            ay[i] += b[j].m * dy * inv;
            az[i] += b[j].m * dz * inv;
            ax[j] -= b[i].m * dx * inv;
            ay[j] -= b[i].m * dy * inv;
            az[j] -= b[i].m * dz * inv;
        }
}

template <class T>
void step(std::vector<T>& b, double h, std::vector<double>& ax, std::vector<double>& ay, std::vector<double>& az) {
    accel(b, ax, ay, az);
    for (size_t i = 0; i < b.size(); i++) {
        b[i].vx += 0.5 * h * ax[i];
        b[i].vy += 0.5 * h * ay[i];
        b[i].vz += 0.5 * h * az[i];
        b[i].x += h * b[i].vx;
        b[i].y += h * b[i].vy;
        b[i].z += h * b[i].vz;
    }
    accel(b, ax, ay, az);
    for (size_t i = 0; i < b.size(); i++) {
        b[i].vx += 0.5 * h * ax[i];
        b[i].vy += 0.5 * h * ay[i];
        b[i].vz += 0.5 * h * az[i];
    }
}

void pushFlash(SandboxState& S, const SbPt& p, float r, const float col[3]) {
    S.flashes.push_back({p.x, p.y, p.z, r, 0.0f, {col[0], col[1], col[2]}});
}

void mergeAll(SandboxState& S) {
    std::vector<SbBody>& b = S.bodies;
    bool again = true;
    while (again) {
        again = false;
        for (size_t i = 0; i < b.size() && !again; i++) {
            for (size_t j = i + 1; j < b.size(); j++) {
                double dx = b[j].x - b[i].x, dy = b[j].y - b[i].y, dz = b[j].z - b[i].z;
                double lim = 0.85 * (b[i].r + b[j].r);
                if (dx * dx + dy * dy + dz * dz >= lim * lim) continue;
                size_t big = b[i].m >= b[j].m ? i : j, small = big == i ? j : i;
                SbBody& A = b[big];
                const SbBody& B = b[small];
                double m = A.m + B.m;
                A.vx = (A.m * A.vx + B.m * B.vx) / m;
                A.vy = (A.m * A.vy + B.m * B.vy) / m;
                A.vz = (A.m * A.vz + B.m * B.vz) / m;
                A.x = (A.m * A.x + B.m * B.x) / m;
                A.y = (A.m * A.y + B.m * B.y) / m;
                A.z = (A.m * A.z + B.m * B.z) / m;
                float w = (float)(B.m / m);
                for (int c = 0; c < 3; c++) A.col[c] = A.col[c] * (1.0f - w) + B.col[c] * w;
                A.r = std::cbrt(A.r * A.r * A.r + B.r * B.r * B.r);
                A.m = m;
                A.sun = A.sun || B.sun;
                const float hot[3] = {1.0f, 0.85f, 0.6f};
                pushFlash(S, A, (float)A.r, A.type == 3 || A.sun ? hot : A.col);
                b.erase(b.begin() + (long)small);
                again = true;
                break;
            }
        }
    }
}

void spawn(SandboxState& S, int type, const double p[3], const double v[3]) {
    if (S.bodies.size() >= MAX_BODIES) {
        S.full = true;
        return;
    }
    const TypeInfo& T = TYPES[type];
    SbBody nb;
    nb.x = p[0]; nb.y = p[1]; nb.z = p[2];
    nb.vx = v[0]; nb.vy = v[1]; nb.vz = v[2];
    nb.m = T.mass * massMult(S.mk);
    nb.r = T.radius * massSize(S.mk);
    nb.type = type;
    for (int c = 0; c < 3; c++) nb.col[c] = T.col[c];
    if (type == 0) {
        float k = rnd(S);
        if (k < 0.35f) { nb.col[0] = 0.85f; nb.col[1] = 0.5f; nb.col[2] = 0.35f; }
        else if (k < 0.65f) { nb.col[0] = 0.6f; nb.col[1] = 0.9f; nb.col[2] = 0.6f; }
        else if (k < 0.8f) { nb.col[0] = 0.8f; nb.col[1] = 0.8f; nb.col[2] = 0.78f; }
    }
    if (type == 1) {
        nb.col[1] += 0.2f * (rnd(S) - 0.5f);
        nb.col[2] += 0.2f * (rnd(S) - 0.5f);
    }
    S.bodies.push_back(nb);
}

void addSun(SandboxState& S) {
    S.bodies.insert(S.bodies.begin(), SbBody{});
    SbBody& s = S.bodies[0];
    s.m = GM_SUN;
    s.r = 8.0;
    s.type = 2;
    s.sun = true;
    s.col[0] = 1.0f; s.col[1] = 0.82f; s.col[2] = 0.35f;
}

bool hasSun(const SandboxState& S) {
    for (const SbBody& b : S.bodies) if (b.sun) return true;
    return false;
}

const SbBody* heaviest(const SandboxState& S) {
    const SbBody* c = nullptr;
    for (const SbBody& b : S.bodies) if (!c || b.m > c->m) c = &b;
    return c;
}

void circularVelocity(const SandboxState& S, const double p[3], double v[3]) {
    v[0] = v[1] = v[2] = 0.0;
    const SbBody* c = heaviest(S);
    if (!c) return;
    double dx = p[0] - c->x, dy = p[1] - c->y, dz = p[2] - c->z;
    double r = std::sqrt(dx * dx + dy * dy + dz * dz), h = std::sqrt(dx * dx + dz * dz);
    if (r < 2.0 || h < 1e-6) return;
    double sp = std::sqrt(c->m / r);
    v[0] = c->vx - dz / h * sp;
    v[1] = c->vy;
    v[2] = c->vz + dx / h * sp;
}

void throwVelocity(const SandboxState& S, bool orbit, double v[3]) {
    if (orbit) {
        double p[3] = {S.sx, S.sy, S.sz};
        circularVelocity(S, p, v);
    } else {
        v[0] = (S.ex - S.sx) * VEL_PER_UNIT;
        v[1] = (S.ey - S.sy) * VEL_PER_UNIT;
        v[2] = (S.ez - S.sz) * VEL_PER_UNIT;
    }
}

void buildPreview(SandboxState& S, const double v[3]) {
    S.preview.clear();
    std::vector<SbPt> pts;
    for (const SbBody& b : S.bodies) pts.push_back(b);
    SbPt t;
    t.x = S.sx; t.y = S.sy; t.z = S.sz; t.vx = v[0]; t.vy = v[1]; t.vz = v[2];
    t.m = TYPES[S.type].mass * massMult(S.mk);
    t.r = TYPES[S.type].radius * massSize(S.mk);
    pts.push_back(t);
    size_t me = pts.size() - 1;
    double horizon = 30.0;
    const SbBody* c = heaviest(S);
    if (c) {
        double dx = t.x - c->x, dy = t.y - c->y, dz = t.z - c->z;
        double r = std::sqrt(dx * dx + dy * dy + dz * dz) + 1e-6;
        double rvx = v[0] - c->vx, rvy = v[1] - c->vy, rvz = v[2] - c->vz;
        double e = 0.5 * (rvx * rvx + rvy * rvy + rvz * rvz) - c->m / r;
        if (e < 0.0) {
            double a = -c->m / (2.0 * e);
            horizon = std::min(90.0, 6.2831853 * std::sqrt(a * a * a / c->m) * 1.02);
        }
    }
    double n = (double)pts.size();
    double steps = std::max(300.0, std::min(3600.0, 4e6 / (n * n)));
    double h = horizon / steps;
    std::vector<double> ax, ay, az;
    S.preview.push_back({(float)t.x, (float)t.y, (float)t.z});
    for (int i = 1; i <= (int)steps; i++) {
        step(pts, h, ax, ay, az);
        bool hit = false;
        for (size_t j = 0; j < me; j++) {
            double dx = pts[j].x - pts[me].x, dy = pts[j].y - pts[me].y, dz = pts[j].z - pts[me].z, lim = 0.85 * (pts[j].r + pts[me].r);
            if (dx * dx + dy * dy + dz * dz < lim * lim) hit = true;
        }
        if (i % 4 == 0 || hit) S.preview.push_back({(float)pts[me].x, (float)pts[me].y, (float)pts[me].z});
        if (hit || std::fabs(pts[me].x) + std::fabs(pts[me].y) + std::fabs(pts[me].z) > 40000.0) break;
    }
}

struct DemoEvent {
    float t;
    int type;
    double r, phi, incl, node, vfac;
};

const DemoEvent DEMO[] = {
    {0.3f, 0, 60, 0.0, 0.0, 0.0, 1.0},
    {0.6f, 0, 85, 2.0, 0.55, 0.4, 1.0},
    {0.9f, 1, 120, 3.5, -0.35, 1.0, 1.0},
    {1.2f, 0, 56, 0.8, 0.95, 2.0, 0.75},
    {1.5f, 0, 75, 4.4, 1.25, 3.5, 1.0},
    {1.8f, 3, 125, 1.6, 0.3, 5.0, 1.0},
    {2.1f, 1, 95, 5.4, -0.8, 0.2, 1.0},
};
const int DEMO_COUNT = 7;
const float DEMO_DRAG_ON = 4.0f, DEMO_DRAG_OFF = 8.0f, DEMO_STAR = 8.6f;

void tilt(double p[3], double incl, double node) {
    double y = p[1] * std::cos(incl) - p[2] * std::sin(incl), z = p[1] * std::sin(incl) + p[2] * std::cos(incl);
    double x0 = p[0];
    p[0] = x0 * std::cos(node) + z * std::sin(node); p[1] = y; p[2] = -x0 * std::sin(node) + z * std::cos(node);
}

void runDemo(SandboxState& S) {
    if (S.demoStep < DEMO_COUNT && S.clock >= DEMO[S.demoStep].t) {
        const DemoEvent& e = DEMO[S.demoStep++];
        double p[3] = {e.r * std::cos(e.phi), 0.0, e.r * std::sin(e.phi)};
        double sp = std::sqrt(GM_SUN / e.r) * e.vfac;
        double v[3] = {-std::sin(e.phi) * sp, 0.0, std::cos(e.phi) * sp};
        tilt(p, e.incl, e.node);
        tilt(v, e.incl, e.node);
        spawn(S, e.type, p, v);
    }
    if (S.clock >= DEMO_DRAG_ON && S.clock < DEMO_DRAG_OFF) {
        S.dragging = true;
        S.vertical = true;
        S.type = 1;
        S.sx = -65; S.sy = 0; S.sz = -40;
        S.ex = -52; S.ey = 14; S.ez = -50;
    } else if (S.dragging && S.clock >= DEMO_DRAG_OFF) {
        S.dragging = false;
        S.vertical = false;
        double p[3] = {S.sx, S.sy, S.sz}, v[3];
        throwVelocity(S, false, v);
        spawn(S, 1, p, v);
        S.type = 0;
    }
    if (S.clock >= DEMO_STAR && S.demoStep == DEMO_COUNT) {
        S.demoStep++;
        double p[3] = {-34, 8, -34}, v[3] = {0, 0, 0};
        spawn(S, 2, p, v);
    }
}

void recordTrails(SandboxState& S, double thr) {
    for (SbBody& b : S.bodies) {
        if (!b.trail.empty()) {
            double dx = b.x - b.trail.back()[0], dy = b.y - b.trail.back()[1], dz = b.z - b.trail.back()[2];
            if (dx * dx + dy * dy + dz * dz < thr * thr) continue;
        }
        b.trail.push_back({(float)b.x, (float)b.y, (float)b.z});
        if (b.trail.size() > S.trailCap) b.trail.erase(b.trail.begin());
    }
}

}

void sbRestart(AppContext& app) {
    SandboxState& S = app.sb;
    bool demo = S.demo || g_sandboxDemo;
    S = SandboxState{};
    S.demo = demo;
    S.ready = true;
    addSun(S);
    if (g_sandboxSolar) {
        g_sandboxSolar = false;
        sbPresetLoad(app, 0);
    }
    if (g_sandboxPreset > 0) {
        int n = g_sandboxPreset - 1;
        g_sandboxPreset = 0;
        sbPresetLoad(app, n);
    }
}

void sbEnter(AppContext& app) {
    if (!app.sb.ready) sbRestart(app);
    Camera& c = app.camera;
    const float* k = app.sb.cam;
    c.free = false;
    c.lockLook = true;
    c.yaw = k[0]; c.pitch = k[1]; c.dist = k[2];
    c.target[0] = k[3]; c.target[1] = k[4]; c.target[2] = k[5];
    c.minDist = 8.0f;
    c.nearPlane = 0.5f;
    app.showSky = false;
}

void sbToggleSun(AppContext& app) {
    SandboxState& S = app.sb;
    if (!S.ready) return;
    if (hasSun(S)) {
        for (size_t i = 0; i < S.bodies.size(); i++)
            if (S.bodies[i].sun) {
                pushFlash(S, S.bodies[i], (float)S.bodies[i].r, S.bodies[i].col);
                S.bodies.erase(S.bodies.begin() + (long)i);
                break;
            }
    } else {
        addSun(S);
        pushFlash(S, S.bodies[0], 8.0f, S.bodies[0].col);
    }
}

void sbUpdate(AppContext& app, float dt, float speed, bool mouseNow, bool pressUi) {
    SandboxState& S = app.sb;
    if (!S.ready) sbRestart(app);
    Camera& cam = app.camera;
    cam.free = false;
    cam.lockLook = true;
    cam.zoomImpulse = 0.0f;
    cam.minDist = 8.0f;
    cam.nearPlane = 0.5f;
    dt = std::min(dt, 0.1f);
    S.clock += dt;
    S.capT = std::max(0.0f, S.capT - dt);
    S.paused = speed <= 0.0f;
    S.speed = speed;
    float H = (float)app.swapchainExtent.height;
    GLFWwindow* w = app.window;
    double fx = app.cursorFb[0], fy = app.cursorFb[1];

    bool rnow = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    bool mnow = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
    if (rnow && S.rightPrev) {
        cam.yaw += (float)(fx - S.panX) * 0.005f;
        cam.pitch -= (float)(fy - S.panY) * 0.005f;
        cam.pitch = std::max(-1.5f, std::min(1.5f, cam.pitch));
    }
    if (mnow && S.midPrev) {
        double back[3], right[3], up[3];
        rayBasis(cam, back, right, up);
        double hl = std::sqrt(back[0] * back[0] + back[2] * back[2]) + 1e-9;
        double k = 2.0 * cam.dist * std::tan(30.0 * 3.14159265358979 / 180.0) / H;
        double dx = (fx - S.panX) * k, dy = (fy - S.panY) * k;
        for (int a = 0; a < 3; a += 2) {
            cam.target[a] -= (float)(right[a] * dx);
            cam.target[a] += (float)(-back[a] / hl * dy);
        }
    }
    S.panX = fx;
    S.panY = fy;
    S.rightPrev = rnow;
    S.midPrev = mnow;
    if (S.demo) cam.yaw += dt * 0.07f;

    const int keys[6][2] = {{GLFW_KEY_1, GLFW_KEY_KP_1}, {GLFW_KEY_2, GLFW_KEY_KP_2}, {GLFW_KEY_3, GLFW_KEY_KP_3}, {GLFW_KEY_4, GLFW_KEY_KP_4}, {GLFW_KEY_C, GLFW_KEY_C}, {GLFW_KEY_X, GLFW_KEY_X}};
    for (int k = 0; k < 5; k++) {
        bool now = glfwGetKey(w, keys[k][0]) == GLFW_PRESS || glfwGetKey(w, keys[k][1]) == GLFW_PRESS;
        if (now && !S.keyPrev[k]) {
            if (k < 4) S.type = k;
            else {
                for (const SbBody& b : S.bodies) if (!b.sun) pushFlash(S, b, (float)b.r, b.col);
                S.bodies.erase(std::remove_if(S.bodies.begin(), S.bodies.end(), [](const SbBody& b) { return !b.sun; }), S.bodies.end());
                S.full = false;
            }
        }
        S.keyPrev[k] = now;
    }

    S.mk = std::max(MASS_KMIN, std::min(MASS_KMAX, S.mk + massSteps(w, cam, S.massPrev)));
    bool orbit = false;
    if (S.demo) {
        runDemo(S);
    } else {
        int which = 0;
        double e[3], d[3];
        cursorRay(cam, app.swapchainExtent.width, app.swapchainExtent.height, fx, fy, e, d);
        if (mouseNow && !S.leftPrev && !pressUi) {
            if (sbBarHit(app, fx, fy, which)) S.type = which;
            else {
                double o[3] = {0, 0, 0}, ny[3] = {0, 1, 0}, hit[3];
                double back[3], right[3], up[3];
                rayBasis(cam, back, right, up);
                if (!rayPlane(e, d, o, ny, hit)) {
                    double tg[3] = {cam.target[0], cam.target[1], cam.target[2]};
                    if (!rayPlane(e, d, tg, back, hit)) for (int a = 0; a < 3; a++) hit[a] = tg[a];
                }
                S.dragging = true;
                S.sx = hit[0]; S.sy = hit[1]; S.sz = hit[2];
                S.ex = S.sx; S.ey = S.sy; S.ez = S.sz;
                S.pressPx = fx;
                S.pressPy = fy;
            }
        }
        if (S.dragging) {
            S.vertical = glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
            double back[3], right[3], up[3], hit[3];
            rayBasis(cam, back, right, up);
            double p0[3] = {S.sx, S.sy, S.sz}, n[3] = {0, 1, 0};
            if (S.vertical) {
                double hl = std::sqrt(back[0] * back[0] + back[2] * back[2]) + 1e-9;
                n[0] = back[0] / hl; n[1] = 0.0; n[2] = back[2] / hl;
            }
            if (rayPlane(e, d, p0, n, hit)) {
                S.ex = hit[0]; S.ey = hit[1]; S.ez = hit[2];
            }
            double dpx = fx - S.pressPx, dpy = fy - S.pressPy;
            orbit = dpx * dpx + dpy * dpy < 36.0;
        }
        if (!mouseNow && S.leftPrev && S.dragging) {
            S.dragging = false;
            double p[3] = {S.sx, S.sy, S.sz}, v[3];
            throwVelocity(S, orbit, v);
            spawn(S, S.type, p, v);
        }
    }
    S.leftPrev = mouseNow;
    cam.wheelHold = S.dragging && !S.demo;

    if (S.dragging) {
        double v[3];
        throwVelocity(S, orbit, v);
        buildPreview(S, v);
    } else {
        S.preview.clear();
    }

    if (speed > 0.0f) {
        double sim = (double)dt * TIME_RATE * speed;
        int ns = std::max(1, std::min(600, (int)std::ceil(sim / 0.01)));
        double h = sim / ns;
        std::vector<double> ax, ay, az;
        double thr = cam.dist * 0.012;
        for (int k = 0; k < ns; k++) {
            step(S.bodies, h, ax, ay, az);
            mergeAll(S);
            sbPartStep(S, h);
            if (k % 6 == 5 || k == ns - 1) recordTrails(S, thr);
        }
        S.bodies.erase(std::remove_if(S.bodies.begin(), S.bodies.end(), [](const SbBody& b) {
            return std::fabs(b.x) > KILL_R || std::fabs(b.y) > KILL_R || std::fabs(b.z) > KILL_R;
        }), S.bodies.end());
    }
    for (SbFlash& f : S.flashes) f.age += dt;
    S.flashes.erase(std::remove_if(S.flashes.begin(), S.flashes.end(), [](const SbFlash& f) { return f.age > 1.3f; }), S.flashes.end());
    S.cam[0] = cam.yaw; S.cam[1] = cam.pitch; S.cam[2] = cam.dist;
    S.cam[3] = cam.target[0]; S.cam[4] = cam.target[1]; S.cam[5] = cam.target[2];
}
