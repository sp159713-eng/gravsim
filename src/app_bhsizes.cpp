#include "vk_app.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

struct Hole {
    const char* name;
    const char* mass;
    double suns;
};

const Hole HOLES[] = {
    {"SGR A*", "4.3 MILLION SUNS", 4.3e6},
    {"M87*", "6.5 BILLION SUNS", 6.5e9},
    {"HOLMBERG 15A", "40 BILLION SUNS", 40e9},
    {"TON 618", "66 BILLION SUNS", 66e9},
    {"PHOENIX A (EST., DISPUTED)", "100 BILLION SUNS", 100e9},
};
const int HOLE_COUNT = 5;

struct Orbit {
    const char* name;
    float au;
    float c[3];
};

const Orbit ORBITS[] = {
    {"MERCURY", 0.387f, {0.75f, 0.68f, 0.6f}},
    {"EARTH", 1.0f, {0.35f, 0.62f, 1.0f}},
    {"JUPITER", 5.2f, {1.0f, 0.7f, 0.4f}},
    {"NEPTUNE", 30.07f, {0.4f, 0.55f, 1.0f}},
    {"PLUTO", 39.5f, {0.8f, 0.7f, 0.9f}},
};
const float OORT_AU = 2000.0f;
const float SUN_RADIUS_AU = 696000.0f / 149.6e6f;
const double KM_PER_AU = 149.6e6;
const double RS_KM_PER_SUN = 2.95;
const double EXTENT = 8.2;
const double SHADOW = 2.6;
const float NEAR_DIST_AU = 2.7f;
const float MIN_DIST_AU = 0.15f;
const float TAN30 = 0.57735027f;
const float SUN_X_MIN_AU = -45.0f;
const float PITCH = 0.9f;

double rsAu(int i) {
    return HOLES[i].suns * RS_KM_PER_SUN / KM_PER_AU;
}

const double* holeX() {
    static double x[HOLE_COUNT];
    static bool ready = false;
    if (!ready) {
        x[0] = 2.0;
        x[1] = 3600.0;
        for (int i = 2; i < HOLE_COUNT; i++) x[i] = x[i - 1] + 1.12 * EXTENT * (rsAu(i - 1) + rsAu(i));
        ready = true;
    }
    return x;
}

float unitsPerAu() {
    return solarUnitsPerAu(true);
}

void framing(const AppContext& app, float& dFar, float& farCentre) {
    float aspect = (float)app.swapchainExtent.width / (float)std::max(1u, app.swapchainExtent.height);
    double xMax = holeX()[HOLE_COUNT - 1] + EXTENT * rsAu(HOLE_COUNT - 1);
    double width = xMax - SUN_X_MIN_AU;
    dFar = (float)(1.06 * width / (2.0 * TAN30 * aspect)) * unitsPerAu();
    farCentre = (float)(0.5 * (xMax + SUN_X_MIN_AU)) * unitsPerAu();
}

void placeCamera(AppContext& app) {
    Camera& c = app.camera;
    float dFar, farCentre;
    framing(app, dFar, farCentre);
    float d = std::exp(app.bhLogDist);
    float s = std::min(1.0f, std::max(0.0f, (d / dFar - 0.3f) / 0.7f));
    s = s * s * (3.0f - 2.0f * s);
    float t1 = 0.4f * d;
    float tx = t1 + (farCentre - t1) * s;
    float cp = std::cos(c.pitch), sp = std::sin(c.pitch), sy = std::sin(c.yaw), cy = std::cos(c.yaw);
    c.free = true;
    float formula[3] = {tx, 0.0f, 0.0f};
    BhTour& tour = app.bhTour;
    for (int a = 0; a < 3; a++) {
        if (tour.mode != 0) tour.off[a] = tour.target[a] - formula[a];
        c.target[a] = formula[a] + tour.off[a];
    }
    c.dist = d;
    c.pos[0] = c.target[0] + d * cp * sy;
    c.pos[1] = c.target[1] + d * sp;
    c.pos[2] = c.target[2] + d * cp * cy;
    c.nearPlane = 0.005f * d;
}

bool project(const float m[16], const float p[3], float& x, float& y, float& w) {
    float clip[4];
    for (int row = 0; row < 4; row++)
        clip[row] = m[row] * p[0] + m[row + 4] * p[1] + m[row + 8] * p[2] + m[row + 12];
    w = clip[3];
    if (clip[3] <= 1e-9f) return false;
    x = clip[0] / clip[3];
    y = clip[1] / clip[3];
    return true;
}

struct Ctx {
    OverlayVertex* out;
    uint32_t& used;
    const float* vp;
    float W, H, px, py, pxPerUnit;
    float d;
};

void segment(Ctx& k, float x0, float y0, float x1, float y1, const float c[3]) {
    if (k.used + 2 > MAX_VERTS) return;
    k.out[k.used++] = {x0, y0, c[0], c[1], c[2]};
    k.out[k.used++] = {x1, y1, c[0], c[1], c[2]};
}

bool offSame(float a, float b) {
    return (a > 1.3f && b > 1.3f) || (a < -1.3f && b < -1.3f);
}

void circle(Ctx& k, float cx, float r, const float c[3], bool dashed) {
    float centre[3] = {cx, 0.0f, 0.0f};
    float x, y, w;
    project(k.vp, centre, x, y, w);
    w = std::max(w, 0.1f * k.d);
    float rpx = r * 0.5f * k.H / (TAN30 * w);
    if (rpx < 2.5f || rpx > 4e6f) return;
    int n = std::min(360, std::max(48, (int)(6.2831853f * std::sqrt(rpx))));
    float px0 = 0.0f, py0 = 0.0f;
    bool have = false;
    for (int i = 0; i <= n; i++) {
        float a = 6.2831853f * (float)i / (float)n;
        float p[3] = {cx + r * std::cos(a), 0.0f, r * std::sin(a)};
        float qx, qy, qw;
        bool ok = project(k.vp, p, qx, qy, qw);
        bool draw = have && ok && !offSame(px0, qx) && !offSame(py0, qy) && (!dashed || ((i / 2) % 2) == 0);
        if (draw) segment(k, px0, py0, qx, qy, c);
        px0 = qx; py0 = qy; have = ok;
    }
}

void disc(Ctx& k, float cx, float cy, float r, const float c[3]) {
    float step = std::max(1.0f, r / 600.0f);
    for (float dy = -r; dy <= r; dy += step) {
        float half = std::sqrt(std::max(0.0f, r * r - dy * dy));
        segment(k, cx - half * k.px, cy + dy * k.py, cx + half * k.px, cy + dy * k.py, c);
    }
}

std::string fmtKm(double km) {
    char b[48];
    if (km >= 1e9) std::snprintf(b, sizeof(b), "%.1f BILLION KM", km / 1e9);
    else std::snprintf(b, sizeof(b), "%.1f MILLION KM", km / 1e6);
    return b;
}

std::string fmtAu(double au) {
    char b[48];
    if (au < 1.0) std::snprintf(b, sizeof(b), "%.2f AU ACROSS", au);
    else std::snprintf(b, sizeof(b), "%.0f AU ACROSS", au);
    return b;
}

struct Label {
    std::vector<std::string> lines;
    float cx, cy, s, rs;
    bool above;
    bool sun;
    float colour[3];
};

struct Box {
    float x0, y0, x1, y1;
};

bool overlaps(const Box& a, const Box& b) {
    return a.x0 < b.x1 + 6.0f && a.x1 + 6.0f > b.x0 && a.y0 < b.y1 + 6.0f && a.y1 + 6.0f > b.y0;
}

}

std::vector<Particle> makeBhSizes(uint32_t n) {
    std::vector<Particle> p(n);
    for (auto& q : p) q = Particle{};
    for (int i = 0; i < HOLE_COUNT && (uint32_t)i < n; i++) {
        float a = 0.28f + 0.05f * (float)i, b = 1.3f * (float)i + 0.5f;
        p[i].pos[0] = (float)(holeX()[i] * unitsPerAu());
        p[i].pos[3] = (float)(SHADOW * rsAu(i) * unitsPerAu());
        p[i].vel[0] = std::sin(a) * std::cos(b);
        p[i].vel[1] = std::cos(a);
        p[i].vel[2] = std::sin(a) * std::sin(b);
    }
    return p;
}

uint32_t bhHoleCount(const AppContext& app) {
    return std::min<uint32_t>(HOLE_COUNT, app.n);
}

void bhResetCamera(AppContext& app) {
    app.camera.yaw = 0.0f;
    app.camera.pitch = PITCH;
    app.bhLogDist = std::log(NEAR_DIST_AU * unitsPerAu());
    app.bhTour = BhTour{};
    bhTourStart(app);
    placeCamera(app);
}

void bhStopGeom(const AppContext&, int stop, double& x, double& radius) {
    float u = unitsPerAu();
    if (stop == 0) {
        x = 0.0;
        radius = SUN_RADIUS_AU * u;
    } else {
        x = holeX()[stop - 1] * u;
        radius = 5.0 * rsAu(stop - 1) * u;
    }
}

void bhFarFraming(const AppContext& app, float& dFar, float& centre) {
    framing(app, dFar, centre);
}


void bhUpdateCamera(AppContext& app, float dt) {
    Camera& c = app.camera;
    GLFWwindow* w = app.window;
    float zoomDir = app.showHelp ? 0.0f : (glfwGetKey(w, GLFW_KEY_PAGE_UP) == GLFW_PRESS ? 1.0f : 0.0f)
        - (glfwGetKey(w, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS ? 1.0f : 0.0f);
    float wheel = c.zoomImpulse;
    if (app.bhTour.mode != 0) {
        bhTourStep(app, dt, zoomDir != 0.0f || wheel != 0.0f);
        if (app.bhTour.mode != 0) {
            c.zoomImpulse = 0.0f;
            placeCamera(app);
            return;
        }
    }
    c.zoomImpulse = 0.0f;
    for (int a = 0; a < 3; a++) app.bhTour.off[a] *= std::exp(-3.0f * std::min(dt, 0.1f));
    float rate = 3.0f * (glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 2.0f : 1.0f);
    float dFar, farCentre;
    framing(app, dFar, farCentre);
    float l = app.bhLogDist + zoomDir * rate * std::min(dt, 0.1f) - 0.35f * wheel;
    app.bhLogDist = std::min(std::log(1.5f * dFar), std::max(std::log(MIN_DIST_AU * unitsPerAu()), l));
    placeCamera(app);
}

void bhViewProj(const AppContext& app, float aspect, float out[16]) {
    float d = app.camera.dist;
    cameraViewProjEx(app.camera, aspect, nullptr, 0.005f * d, 200.0f * d, out);
}

void emitBhSizes(AppContext& app, OverlayVertex* out, uint32_t& used) {
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float vp[16];
    bhViewProj(app, W / H, vp);
    float u = unitsPerAu();
    float d = app.camera.dist;
    Ctx k{out, used, vp, W, H, 2.0f / W, 2.0f / H, 0.0f, d};
    auto toNdcX = [&](float X) { return -1.0f + X * k.px; };
    auto toNdcY = [&](float Y) { return -1.0f + Y * k.py; };

    for (const Orbit& o : ORBITS) circle(k, 0.0f, o.au * u, o.c, false);
    const float horizonC[3] = {1.0f, 0.3f, 0.28f};
    const float oortC[3] = {0.45f, 0.8f, 0.45f};
    std::vector<Label> labels;
    int focus = bhTourFocus(app);
    for (int i = 0; i < HOLE_COUNT; i++) {
        float cx = (float)(holeX()[i] * u);
        float rsU = (float)(rsAu(i) * u);
        if (i >= 1) {
            for (const Orbit& o : ORBITS) circle(k, cx, o.au * u, o.c, false);
            circle(k, cx, OORT_AU * u, oortC, true);
        }
        circle(k, cx, rsU, horizonC, false);
        float centre[3] = {cx, 0.0f, 0.0f};
        float x, y, w;
        if (!project(vp, centre, x, y, w)) continue;
        Label l;
        double diamKm = 2.0 * HOLES[i].suns * RS_KM_PER_SUN;
        l.lines = {HOLES[i].name, HOLES[i].mass, "HORIZON " + fmtKm(diamKm), fmtAu(diamKm / KM_PER_AU)};
        l.cx = (x + 1.0f) * 0.5f * W;
        l.cy = (y + 1.0f) * 0.5f * H;
        l.rs = std::max(2.0f, (float)(SHADOW * rsU) * 0.5f * H / (TAN30 * w));
        l.s = l.rs;
        l.above = false;
        l.sun = false;
        l.colour[0] = 1.0f; l.colour[1] = 0.8f; l.colour[2] = 0.45f;
        if (focus == -2 || focus == i) labels.push_back(l);
    }
    {
        float centre[3] = {0.0f, 0.0f, 0.0f};
        float x, y, w;
        if (project(vp, centre, x, y, w)) {
            float cx = (x + 1.0f) * 0.5f * W, cy = (y + 1.0f) * 0.5f * H;
            float rpx = std::max(3.0f, SUN_RADIUS_AU * u * 0.5f * H / (TAN30 * w));
            const float sunC[3] = {1.0f, 0.85f, 0.25f};
            disc(k, toNdcX(cx), toNdcY(cy), rpx, sunC);
            Label l;
            l.lines = {"THE SUN", "1 SOLAR MASS", "RADIUS 696,000 KM"};
            l.cx = cx; l.cy = cy; l.s = rpx; l.rs = rpx;
            l.above = false;
            l.sun = true;
            l.colour[0] = 1.0f; l.colour[1] = 0.9f; l.colour[2] = 0.4f;
            if (focus == -2 || focus == HOLE_COUNT) labels.push_back(l);
        }
    }
    const float scale = 2.0f;
    const float lineH = 18.0f;
    std::vector<size_t> order(labels.size());
    for (size_t i = 0; i < order.size(); i++) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return labels[a].rs > labels[b].rs; });
    struct Placed {
        size_t idx;
        float x0, y0, lw;
        bool leader;
        float fromY;
        bool up;
    };
    std::vector<Placed> placed;
    std::vector<Box> boxes;
    for (size_t idx : order) {
        const Label& l = labels[idx];
        size_t chars = 0;
        for (const std::string& s : l.lines) chars = std::max(chars, s.size());
        float lw = (float)chars * 6.0f * scale, lh = lineH * (float)l.lines.size();
        if (l.cx < -0.5f * lw || l.cx > W + 0.5f * lw || l.cy - l.s > H || l.cy + l.s < 0.0f) continue;
        if (focus >= 0) {
            placed.push_back({idx, W - lw - 16.0f, 64.0f, lw, false, 0.0f, false});
            continue;
        }
        float base = l.cy + (l.sun ? l.s : 2.4f * l.s) + 10.0f;
        float x0 = std::min(std::max(l.cx - 0.5f * lw, 8.0f), W - lw - 8.0f);
        float top = l.cy - (l.sun ? l.s : 2.4f * l.s) - 10.0f;
        for (int step = 0; step < 7; step++) {
            bool up = step >= 4;
            float y0 = up ? top - lh - (float)(step - 4) * (lh + 8.0f) : std::min(base + (float)step * (lh + 8.0f), H - lh - 8.0f);
            if (y0 < 90.0f) continue;
            Box b{x0, y0, x0 + lw, y0 + lh};
            bool clash = false;
            for (const Box& o : boxes) clash = clash || overlaps(b, o);
            if (clash) continue;
            boxes.push_back(b);
            bool leader = up ? true : (step > 0 && y0 > l.cy + l.s);
            placed.push_back({idx, x0, y0, lw, leader, up ? top + 8.0f : base - 8.0f, up});
            break;
        }
    }
    const float leaderC[3] = {0.4f, 0.4f, 0.5f};
    for (const Placed& p : placed) {
        if (!p.leader) continue;
        const Label& l = labels[p.idx];
        segment(k, toNdcX(l.cx), toNdcY(p.fromY), toNdcX(std::min(std::max(l.cx, p.x0), p.x0 + p.lw)), toNdcY(p.up ? p.y0 + (float)l.lines.size() * lineH : p.y0), leaderC);
    }
    uint32_t lineVerts = used;

    for (const Orbit& o : ORBITS) {
        float top[3] = {0.0f, 0.0f, -o.au * u};
        float x, y, w;
        if (!project(vp, top, x, y, w)) continue;
        float rpx = o.au * u * 0.5f * H / (TAN30 * w);
        float X = (x + 1.0f) * 0.5f * W, Y = (y + 1.0f) * 0.5f * H;
        float len = (float)std::string(o.name).size() * 6.0f;
        if (rpx < 40.0f || rpx > 3000.0f || X < 0.0f || X > W - len || Y < 14.0f || Y > H) continue;
        emitText(out, used, o.name, toNdcX(X - 0.5f * len), toNdcY(Y - 11.0f), k.px, k.py, 1.0f, o.c[0], o.c[1], o.c[2]);
    }
    for (const Placed& p : placed) {
        const Label& l = labels[p.idx];
        for (size_t i = 0; i < l.lines.size(); i++) {
            float cr = i == 0 ? l.colour[0] : (i == 1 ? 0.85f : 1.0f);
            float cg = i == 0 ? l.colour[1] : (i == 1 ? 0.9f : 0.55f);
            float cb = i == 0 ? l.colour[2] : (i == 1 ? 1.0f : 0.5f);
            emitText(out, used, l.lines[i].c_str(), toNdcX(p.x0), toNdcY(p.y0 + lineH * (float)i), k.px, k.py, scale, cr, cg, cb);
        }
    }

    float auPerPx = d / u * 2.0f * TAN30 / H;
    char buf[96];
    if (auPerPx >= 10.0f) std::snprintf(buf, sizeof(buf), "1 PX = %.0f AU", auPerPx);
    else if (auPerPx >= 1.0f) std::snprintf(buf, sizeof(buf), "1 PX = %.1f AU", auPerPx);
    else if (auPerPx >= 0.01f) std::snprintf(buf, sizeof(buf), "1 PX = %.2f AU", auPerPx);
    else std::snprintf(buf, sizeof(buf), "1 PX = %.4f AU", auPerPx);
    emitBhTour(app, out, used, k.px, k.py);
    emitText(out, used, buf, toNdcX(12.0f), toNdcY(12.0f), k.px, k.py, scale, 0.9f, 0.9f, 0.95f);
    emitText(out, used, "BLACK HOLE SIZES - TRUE SCALE", toNdcX(12.0f), toNdcY(34.0f), k.px, k.py, scale, 0.6f, 0.8f, 1.0f);
    emitText(out, used, "CIRCLES: ORBITS OF MERCURY, EARTH, JUPITER, NEPTUNE, PLUTO", toNdcX(12.0f), toNdcY(58.0f), k.px, k.py, 1.0f, 0.7f, 0.75f, 0.85f);
    emitText(out, used, "DASHED: OORT CLOUD INNER EDGE (2,000 AU)   RED RING: EVENT HORIZON", toNdcX(12.0f), toNdcY(69.0f), k.px, k.py, 1.0f, 0.7f, 0.75f, 0.85f);
    app.overlayLineVerts[app.currentFrame] = lineVerts;
    app.overlayPointVerts[app.currentFrame] = used - lineVerts;
}
