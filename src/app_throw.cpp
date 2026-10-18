#include "vk_app.h"
#include "massctl.h"
#include "throwint.h"
#include <algorithm>
#include <cmath>
#include <cstring>

int g_throwDemo = 0;

namespace {

const double PI = 3.14159265358979;

bool galaxyScene(const AppContext& app) { return app.scene == Scene::GALAXY; }

float rnd(ThrowState& T) {
    T.rng = T.rng * 1664525u + 1013904223u;
    return (float)(T.rng >> 8) / 16777216.0f;
}

double killRadius(const AppContext& app) {
    return galaxyScene(app) ? 600.0 : 400.0 * solarUnitsPerAu(app.trueScale);
}

void advance(const AppContext& app, ThrowBody& b, double t, const Primary& P) {
    if (t <= 0.0) return;
    int n = std::max(1, std::min(60, (int)std::ceil(t / 0.02)));
    double h = t / n, G = app.simG, s2 = (double)app.simSoft * app.simSoft;
    for (int i = 0; i < n; i++) {
        double r[3], d2 = s2;
        for (int a = 0; a < 3; a++) {
            r[a] = b.p[a] - P.p[a];
            d2 += r[a] * r[a];
        }
        double k = -G * P.m / (d2 * std::sqrt(d2));
        for (int a = 0; a < 3; a++) {
            b.v[a] += 0.5 * h * k * r[a];
            b.p[a] += h * b.v[a];
        }
        d2 = s2;
        for (int a = 0; a < 3; a++) {
            r[a] = b.p[a] - P.p[a];
            d2 += r[a] * r[a];
        }
        k = -G * P.m / (d2 * std::sqrt(d2));
        for (int a = 0; a < 3; a++) b.v[a] += 0.5 * h * k * r[a];
    }
}

void pushFlash(ThrowState& T, const double p[3], float r, const float col[3]) {
    ThrowFlash f;
    for (int a = 0; a < 3; a++) {
        f.p[a] = p[a];
        f.col[a] = col[a];
    }
    f.r = r;
    f.age = 0.0f;
    T.flashes.push_back(f);
}

Particle makeParticle(const double p[3], const double v[3], double m) {
    Particle q{};
    for (int a = 0; a < 3; a++) {
        q.pos[a] = (float)p[a];
        q.vel[a] = (float)v[a];
    }
    q.pos[3] = (float)m;
    return q;
}

void restoreSlot(ThrowState& T, uint32_t slot) {
    if (slot >= T.initial.size()) return;
    T.writes.push_back({slot, {T.initial[slot]}});
}

void clearAll(AppContext& app) {
    ThrowState& T = app.th;
    for (ThrowBody& b : T.bodies) {
        pushFlash(T, b.p, 1.0f, b.col);
        restoreSlot(T, b.slot);
    }
    T.bodies.clear();
    if (T.starsUsed > 0) {
        uint32_t first = app.n - T.starsUsed;
        if (first + T.starsUsed <= T.initial.size()) {
            ThrowWrite w{first, {}};
            w.data.assign(T.initial.begin() + first, T.initial.begin() + first + T.starsUsed);
            T.writes.push_back(w);
        }
        T.starsUsed = 0;
    }
    T.full = false;
}

void launchVelocity(const AppContext& app, const double s[3], const double e[3], bool orbit, double v[3], double& vc, double& speed) {
    const ThrowState& T = app.th;
    Primary P = primaryOf(app);
    double rel[3] = {s[0] - P.p[0], s[1] - P.p[1], s[2] - P.p[2]};
    double r = std::sqrt(rel[0] * rel[0] + rel[1] * rel[1] + rel[2] * rel[2]);
    double h = std::sqrt(rel[0] * rel[0] + rel[2] * rel[2]);
    vc = std::sqrt(app.simG * P.m / std::max(r, 1e-9));
    double lref = 0.2 * distEye(app, s);
    if (T.forceV) {
        for (int a = 0; a < 3; a++) v[a] = T.fv[a];
    } else if (orbit) {
        v[0] = v[1] = v[2] = 0.0;
        if (h > 1e-9) {
            v[0] = P.v[0] - rel[2] / h * vc;
            v[1] = P.v[1];
            v[2] = P.v[2] + rel[0] / h * vc;
        }
    } else {
        double k = vc / std::max(lref, 1e-12);
        for (int a = 0; a < 3; a++) v[a] = (e[a] - s[a]) * k;
    }
    speed = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

void spawnDwarf(AppContext& app, ThrowBody& core, const double v[3]) {
    ThrowState& T = app.th;
    ThrowWrite w{app.n - T.starsUsed - DWARF_STARS, {}};
    double R = 2.5 * core.size, sum = (double)DWARF_STARS, tilt = 0.8;
    for (uint32_t i = 0; i < DWARF_STARS; i++) {
        double r = R * std::max(0.12, std::sqrt((double)rnd(T))), ph = 2.0 * PI * rnd(T);
        double e1[3] = {1, 0, 0}, e2[3] = {0, std::sin(tilt), std::cos(tilt)};
        double menc = core.m + sum * (r / R) * (r / R), vt = std::sqrt(app.simG * menc / r) * (0.9 + 0.2 * rnd(T));
        double p[3], u[3];
        for (int a = 0; a < 3; a++) {
            p[a] = core.p[a] + r * (std::cos(ph) * e1[a] + std::sin(ph) * e2[a]);
            u[a] = v[a] + vt * (-std::sin(ph) * e1[a] + std::cos(ph) * e2[a]);
        }
        p[1] += 0.25 * (rnd(T) - 0.5) * R * 0.3;
        w.data.push_back(makeParticle(p, u, 1.0));
    }
    T.starsUsed += DWARF_STARS;
    T.writes.push_back(w);
}

void spawn(AppContext& app, int type, const double p[3], const double v[3]) {
    ThrowState& T = app.th;
    bool gal = galaxyScene(app);
    bool used[THROW_SLOTS] = {};
    for (const ThrowBody& b : T.bodies) used[b.slot - throwBase(app)] = true;
    uint32_t slot = THROW_SLOTS;
    for (uint32_t i = 0; i < THROW_SLOTS; i++) if (!used[i]) { slot = i; break; }
    if (slot == THROW_SLOTS || (gal && type == 3 && T.starsUsed / DWARF_STARS >= DWARF_POOL)) {
        T.full = true;
        return;
    }
    if (T.initial.empty()) T.initial = makeScene(app.scene, app.n, 7, app.trueScale);
    ThrowBody b;
    for (int a = 0; a < 3; a++) {
        b.p[a] = p[a];
        b.v[a] = v[a];
    }
    b.kind = throwKind(gal, type);
    b.slot = throwBase(app) + slot;
    b.size = massSize(T.mk);
    b.m = throwDefMass(gal, type) * massMult(T.mk);
    static const float COL[6][3] = {{0.55f, 0.7f, 1.0f}, {1.0f, 0.68f, 0.38f}, {1.0f, 0.93f, 0.6f}, {0.0f, 0.0f, 0.0f}, {0.7f, 0.85f, 1.0f}, {0.0f, 0.0f, 0.0f}};
    for (int c = 0; c < 3; c++) b.col[c] = COL[b.kind][c];
    if (b.kind == 0) {
        float k = rnd(T);
        if (k < 0.4f) { b.col[0] = 0.85f; b.col[1] = 0.5f; b.col[2] = 0.35f; }
        else if (k < 0.7f) { b.col[0] = 0.6f; b.col[1] = 0.9f; b.col[2] = 0.6f; }
    }
    if (gal && b.kind == 2) { b.col[0] = 0.8f; b.col[1] = 0.9f; b.col[2] = 1.0f; }
    T.writes.push_back({b.slot, {makeParticle(p, v, b.m)}});
    if (gal && type == 3) spawnDwarf(app, b, v);
    pushFlash(T, p, 1.0f, b.col);
    T.bodies.push_back(b);
}

void syncBodies(AppContext& app) {
    ThrowState& T = app.th;
    int cf = app.currentFrame;
    double own = (double)app.batchSteps[cf] * app.batchDt[cf];
    double lag = own + (double)app.batchSteps[1 - cf] * app.batchDt[1 - cf];
    uint32_t base = throwBase(app);
    const Particle* rb = (app.readValid[cf] && T.recCount[cf] >= base + THROW_SLOTS) ? static_cast<const Particle*>(app.readMapped[cf]) + base : nullptr;
    Primary P = primaryOf(app);
    double thr = distEye(app, P.p) * 0.01, kill = killRadius(app);
    for (size_t i = 0; i < T.bodies.size();) {
        ThrowBody& b = T.bodies[i];
        bool dead = false;
        if (rb && b.age >= 2) {
            const Particle& q = rb[b.slot - base];
            if (q.pos[3] <= 0.0f) dead = true;
            else {
                for (int a = 0; a < 3; a++) {
                    b.p[a] = q.pos[a];
                    b.v[a] = q.vel[a];
                }
                b.m = q.pos[3];
                advance(app, b, lag, P);
            }
        } else {
            advance(app, b, own, P);
        }
        b.age++;
        double rr = std::sqrt((b.p[0] - P.p[0]) * (b.p[0] - P.p[0]) + (b.p[1] - P.p[1]) * (b.p[1] - P.p[1]) + (b.p[2] - P.p[2]) * (b.p[2] - P.p[2]));
        if (rr > kill) dead = true;
        if (dead) {
            pushFlash(T, b.p, 1.0f, b.col);
            restoreSlot(T, b.slot);
            T.bodies.erase(T.bodies.begin() + (long)i);
            continue;
        }
        if (own > 0.0) {
            bool add = b.trail.empty();
            if (!add) {
                double dx = b.p[0] - b.trail.back()[0], dy = b.p[1] - b.trail.back()[1], dz = b.p[2] - b.trail.back()[2];
                add = dx * dx + dy * dy + dz * dz > thr * thr;
            }
            if (add) b.trail.push_back({(float)b.p[0], (float)b.p[1], (float)b.p[2]});
            if (b.trail.size() > 140) b.trail.erase(b.trail.begin());
        }
        i++;
    }
}

struct DemoEvent {
    float t0, t1;
    int type, mk;
    double s[3], v[3];
};

const DemoEvent SOLAR_DEMO[] = {
    {1.5f, 4.5f, 2, 0, {-4.2, 0.0, 2.7}, {4.0, 0.0, -1.0}},
    {7.0f, 9.0f, 0, 9, {3.6, 0.0, -2.4}, {-1.6, 0.0, 1.5}},
};
const DemoEvent GALAXY_DEMO[] = {
    {1.5f, 4.0f, 2, 1, {-30.0, 2.0, 16.0}, {9.5, -0.3, -3.2}},
    {7.0f, 9.0f, 3, 0, {32.0, 0.0, -14.0}, {-7.0, 0.0, 3.0}},
};

}

bool throwOk(const AppContext& app) {
    if (app.scene != Scene::SOLAR && app.scene != Scene::GALAXY) return false;
    if (app.visit.active || app.visit.mode != 0 || app.gal.active || app.tour.mode != 0) return false;
    return app.n >= throwBase(app) + THROW_SLOTS + DWARF_STARS * DWARF_POOL + 8;
}

bool throwActive(const AppContext& app) {
    return app.th.on && throwOk(app);
}

uint32_t throwBase(const AppContext& app) {
    return app.scene == Scene::GALAXY ? 1u : BODIES;
}

uint32_t throwReadEnd(const AppContext& app) {
    if (!throwOk(app) || (!app.th.on && app.th.bodies.empty())) return 0;
    return throwBase(app) + THROW_SLOTS;
}

void throwReset(AppContext& app, const std::vector<Particle>& particles) {
    ThrowState& T = app.th;
    T.initial = particles;
    T.bodies.clear();
    T.flashes.clear();
    T.writes.clear();
    T.preview.clear();
    T.starsUsed = 0;
    T.dragging = false;
    T.full = false;
    T.recCount[0] = T.recCount[1] = 0;
    if (T.scene != (int)app.scene) T.on = false;
    T.scene = (int)app.scene;
}

void throwRecord(AppContext& app, VkCommandBuffer cmd) {
    ThrowState& T = app.th;
    if (T.writes.empty()) return;
    VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &b, 0, nullptr, 0, nullptr);
    for (const ThrowWrite& w : T.writes)
        for (int i = 0; i < 2; i++)
            vkCmdUpdateBuffer(cmd, app.particleBuffers[i], (VkDeviceSize)w.first * sizeof(Particle), (VkDeviceSize)w.data.size() * sizeof(Particle), w.data.data());
    T.writes.clear();
}

void throwDemoInit(AppContext& app) {
    ThrowState& T = app.th;
    T.demo = g_throwDemo;
    T.on = true;
    T.scene = (int)app.scene;
    Camera& c = app.camera;
    c.free = true;
    if (T.demo == 1) {
        c.pos[0] = 0.0f; c.pos[1] = 5.4f; c.pos[2] = 4.6f;
        c.yaw = 0.0f; c.pitch = 0.95f;
    } else {
        c.pos[0] = 0.0f; c.pos[1] = 45.0f; c.pos[2] = 38.0f;
        c.yaw = 0.0f; c.pitch = 0.85f;
    }
}

void throwUpdate(AppContext& app, float dt, bool mouseNow, bool pressUi) {
    ThrowState& T = app.th;
    Camera& cam = app.camera;
    if (app.scene != Scene::SOLAR && app.scene != Scene::GALAXY) return;
    if (!throwOk(app)) {
        T.dragging = false;
        T.leftPrev = mouseNow;
        cam.wheelHold = false;
        cam.lockLook = false;
        T.preview.clear();
        return;
    }
    dt = std::min(dt, 0.1f);
    T.clock += dt;
    if (T.msgT > 0.0f) T.msgT -= dt;
    GLFWwindow* w = app.window;
    double fx = app.cursorFb[0], fy = app.cursorFb[1];
    bool gal = galaxyScene(app);
    bool typing = timeTyping();
    auto key = [&](int idx, int k1, int k2) {
        bool now = !typing && (glfwGetKey(w, k1) == GLFW_PRESS || (k2 && glfwGetKey(w, k2) == GLFW_PRESS));
        bool edge = now && !T.keyPrev[idx];
        T.keyPrev[idx] = now;
        return edge;
    };
    if (key(0, GLFW_KEY_F, 0) && !T.demo) {
        T.on = !T.on;
        T.dragging = false;
    }
    if (key(1, GLFW_KEY_C, 0)) clearAll(app);
    for (int k = 0; k < 4; k++)
        if (key(2 + k, GLFW_KEY_1 + k, GLFW_KEY_KP_1 + k) && T.on) T.type = k;
    int ms = massSteps(w, cam, T.massPrev);
    if (T.on) T.mk = std::max(MASS_KMIN, std::min(MASS_KMAX, T.mk + ms));
    cam.lockLook = T.on;
    bool rnow = T.on && glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    if (rnow && T.rightPrev) {
        float sign = cam.free ? -1.0f : 1.0f;
        cam.yaw += sign * (float)(fx - T.panX) * 0.005f;
        cam.pitch -= sign * (float)(fy - T.panY) * 0.005f;
        cam.pitch = std::max(-1.5f, std::min(1.5f, cam.pitch));
    }
    T.panX = fx;
    T.panY = fy;
    T.rightPrev = rnow;

    bool orbit = false;
    double v[3] = {0, 0, 0};
    if (T.on && T.demo) {
        const DemoEvent* ev = T.demo == 1 ? SOLAR_DEMO : GALAXY_DEMO;
        int count = 2;
        T.forceV = false;
        for (int i = 0; i < count; i++) {
            const DemoEvent& e = ev[i];
            if (T.clock >= e.t0 && T.clock < e.t1) {
                if (!T.dragging) {
                    T.dragging = true;
                    T.type = e.type;
                    T.mk = e.mk;
                    T.sx = e.s[0]; T.sy = e.s[1]; T.sz = e.s[2];
                }
                Primary P = primaryOf(app);
                double rel = std::sqrt((e.s[0] - P.p[0]) * (e.s[0] - P.p[0]) + (e.s[1] - P.p[1]) * (e.s[1] - P.p[1]) + (e.s[2] - P.p[2]) * (e.s[2] - P.p[2]));
                double k = std::sqrt(app.simG * P.m / std::max(rel, 1e-9)) / std::max(0.2 * distEye(app, e.s), 1e-12);
                T.ex = e.s[0] + e.v[0] / k; T.ey = e.s[1] + e.v[1] / k; T.ez = e.s[2] + e.v[2] / k;
                T.forceV = true;
                for (int a = 0; a < 3; a++) T.fv[a] = e.v[a];
            } else if (T.dragging && T.demoStep == i && T.clock >= e.t1) {
                T.dragging = false;
                T.demoStep = i + 1;
                T.forceV = true;
                for (int a = 0; a < 3; a++) T.fv[a] = e.v[a];
                double p[3] = {e.s[0], e.s[1], e.s[2]}, ss[3] = {e.s[0], e.s[1], e.s[2]};
                double vv[3], vc, sp;
                launchVelocity(app, ss, ss, false, vv, vc, sp);
                spawn(app, e.type, p, vv);
                T.forceV = false;
            }
        }
    } else if (T.on) {
        double e[3], d[3];
        cursorRay(cam, app.swapchainExtent.width, app.swapchainExtent.height, fx, fy, e, d);
        if (mouseNow && !T.leftPrev && !pressUi) {
            int which = 0;
            if (throwBarHit(app, fx, fy, which)) T.type = which;
            else {
                double o[3] = {0, 0, 0}, ny[3] = {0, 1, 0}, hit[3];
                Primary P = primaryOf(app);
                for (int a = 0; a < 3; a++) o[a] = P.p[a];
                double dEye = distEye(app, o);
                bool got = rayPlane(e, d, o, ny, hit);
                if (got && distEye(app, hit) > 40.0 * std::max(dEye, 1e-9)) got = false;
                if (!got) {
                    double back[3], right[3], up[3];
                    rayBasis(cam, back, right, up);
                    for (int a = 0; a < 3; a++) hit[a] = e[a] + d[a] * dEye;
                }
                T.dragging = true;
                T.sx = hit[0]; T.sy = hit[1]; T.sz = hit[2];
                T.ex = T.sx; T.ey = T.sy; T.ez = T.sz;
                T.pressPx = fx;
                T.pressPy = fy;
            }
        }
        if (T.dragging) {
            T.vertical = glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
            double back[3], right[3], up[3], hit[3];
            rayBasis(cam, back, right, up);
            double p0[3] = {T.sx, T.sy, T.sz}, n[3] = {0, 1, 0};
            if (T.vertical) {
                double hl = std::sqrt(back[0] * back[0] + back[2] * back[2]) + 1e-9;
                n[0] = back[0] / hl; n[1] = 0.0; n[2] = back[2] / hl;
            }
            if (rayPlane(e, d, p0, n, hit)) {
                T.ex = hit[0]; T.ey = hit[1]; T.ez = hit[2];
            }
            double dpx = fx - T.pressPx, dpy = fy - T.pressPy;
            orbit = dpx * dpx + dpy * dpy < 36.0;
        }
        if (!mouseNow && T.leftPrev && T.dragging) {
            T.dragging = false;
            double p[3] = {T.sx, T.sy, T.sz}, s[3] = {T.sx, T.sy, T.sz}, en[3] = {T.ex, T.ey, T.ez}, vc, sp;
            launchVelocity(app, s, en, orbit, v, vc, sp);
            spawn(app, T.type, p, v);
        }
    } else {
        T.dragging = false;
    }
    T.leftPrev = mouseNow;
    cam.wheelHold = T.on && T.dragging && !T.demo;

    syncBodies(app);

    if (T.dragging) {
        double s[3] = {T.sx, T.sy, T.sz}, en[3] = {T.ex, T.ey, T.ez};
        bool useOrbit = orbit && !T.demo;
        launchVelocity(app, s, en, useOrbit, v, T.vc, T.speed);
        T.orbit = useOrbit;
        throwPreview(app, v);
    } else {
        T.preview.clear();
    }
    for (ThrowFlash& f : T.flashes) f.age += dt;
    T.flashes.erase(std::remove_if(T.flashes.begin(), T.flashes.end(), [](const ThrowFlash& f) { return f.age > 1.3f; }), T.flashes.end());
    T.recCount[app.currentFrame] = readbackCount(app);
    (void)gal;
}
