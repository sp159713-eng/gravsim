#include "vk_app.h"
#include "realstars.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

const char* HOME_NAMES[BODIES] = {"SUN", "MERCURY", "VENUS", "EARTH", "MARS",
                                  "JUPITER", "SATURN", "URANUS", "NEPTUNE",
                                  "MOON", "IO", "EUROPA", "GANYMEDE", "CALLISTO", "HALLEY'S COMET"};
const int HOME_PARENT[BODIES] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, 3, 5, 5, 5, 5, -1};
const float HOME_COLORS[BODIES][3] = {
    {1.0f, 0.85f, 0.3f}, {0.9f, 0.4f, 0.3f}, {0.95f, 0.8f, 0.5f}, {0.3f, 0.6f, 1.0f},
    {0.9f, 0.4f, 0.3f}, {0.9f, 0.7f, 0.5f}, {0.95f, 0.85f, 0.6f}, {0.5f, 0.85f, 0.9f},
    {0.4f, 0.55f, 1.0f}, {0.8f, 0.8f, 0.8f}, {0.8f, 0.8f, 0.8f}, {0.8f, 0.8f, 0.8f},
    {0.8f, 0.8f, 0.8f}, {0.8f, 0.8f, 0.8f}, {0.5f, 0.85f, 1.0f}};
const char* CLASS_COLOR[6] = {"BLUE", "BLUE-WHITE", "YELLOW-WHITE", "YELLOW", "ORANGE", "RED"};
const float DEFAULT_POS[3] = {31.2f, 23.4f, 45.6f};
const float MIN_PICK_LY = 0.05f;
const float PICK_PX = 8.0f;

double unitsToLy(const AppContext& app) {
    return (double)LY_PER_AU_INV / solarUnitsPerAu(app.trueScale);
}

float length3(const float v[3]) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

float originUnits(const AppContext& app) {
    float eye[3], sun[3];
    cameraEye(app.camera, eye);
    sunPosition(app, sun);
    float d[3] = {eye[0] - sun[0], eye[1] - sun[1], eye[2] - sun[2]};
    return length3(d);
}

int classOf(const GalaxyPoint& p) {
    int best = 0;
    float bestD = 1e9f;
    for (int c = 0; c < 6; c++) {
        float d = 0.0f;
        for (int a = 0; a < 3; a++) d += (p.color[a] - SPECTRAL[c].rgb[a]) * (p.color[a] - SPECTRAL[c].rgb[a]);
        if (d < bestD) { bestD = d; best = c; }
    }
    return best;
}

std::string fmtFloat(float v, int decimals) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

std::string fmtLy(float ly) {
    if (ly < 10.0f) return fmtFloat(ly, 1);
    std::string s = std::to_string((long long)std::llround(ly));
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ",");
    return s;
}

void restoreDim(AppContext& app) {
    VisitState& v = app.visit;
    if (v.dimmed >= 0 && app.galaxyMapped && (size_t)v.dimmed < app.galaxyPts.size())
        app.galaxyMapped[v.dimmed].bright = app.galaxyPts[v.dimmed].bright;
    v.dimmed = -1;
}

int pickStar(const AppContext& app, double fx, double fy) {
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float vp[16];
    galaxyViewProj(app, W / H, vp);
    float best = PICK_PX * PICK_PX;
    int idx = -1;
    float sc[3];
    sgrCentre(sc);
    float sr = sgrRadiusLy(app);
    for (size_t i = 0; i < app.galaxyPts.size(); i++) {
        if (app.visit.active && i == app.visit.star) continue;
        const float* p = app.galaxyPts[i].pos;
        float qx = p[0] - sc[0], qy = p[1] - sc[1], qz = p[2] - sc[2];
        if (qx * qx + qy * qy + qz * qz < sr * sr) continue;
        float w = vp[3] * p[0] + vp[7] * p[1] + vp[11] * p[2] + vp[15];
        if (w < 1e-3f) continue;
        float sx = ((vp[0] * p[0] + vp[4] * p[1] + vp[8] * p[2] + vp[12]) / w + 1.0f) * 0.5f * W - (float)fx;
        float sy = ((vp[1] * p[0] + vp[5] * p[1] + vp[9] * p[2] + vp[13]) / w + 1.0f) * 0.5f * H - (float)fy;
        float d2 = sx * sx + sy * sy;
        if (d2 < best) { best = d2; idx = (int)i; }
    }
    return idx;
}

bool projectTo(const float m[16], const float p[3], float& x, float& y) {
    float clip[4];
    for (int row = 0; row < 4; row++)
        clip[row] = m[row] * p[0] + m[row + 4] * p[1] + m[row + 8] * p[2] + m[row + 12];
    if (clip[3] <= 1e-9f) return false;
    x = clip[0] / clip[3];
    y = clip[1] / clip[3];
    return true;
}

void lineTo(OverlayVertex* out, uint32_t& used, float x0, float y0, float x1, float y1, const float c[3]) {
    if (used + 2 > MAX_VERTS) return;
    out[used++] = {x0, y0, c[0], c[1], c[2]};
    out[used++] = {x1, y1, c[0], c[1], c[2]};
}

void ringAt(const AppContext& app, OverlayVertex* out, uint32_t& used, float cx, float cy, float rpx, int segs, const float c[3]) {
    float px = 2.0f / (float)app.swapchainExtent.width, py = 2.0f / (float)app.swapchainExtent.height;
    for (int i = 0; i < segs; i++) {
        float a0 = 6.2831853f * i / segs, a1 = 6.2831853f * (i + 1) / segs;
        lineTo(out, used, cx + rpx * px * std::cos(a0), cy + rpx * py * std::sin(a0),
               cx + rpx * px * std::cos(a1), cy + rpx * py * std::sin(a1), c);
    }
}

void setBodyDefaults(AppContext& app) {
    for (uint32_t i = 0; i < BODIES; i++) {
        app.bodyNames[i] = HOME_NAMES[i];
        app.bodyNotes[i].clear();
        app.bodyParent[i] = HOME_PARENT[i];
        app.bodyActive[i] = true;
        for (int a = 0; a < 3; a++) app.bodyColors[i][a] = HOME_COLORS[i][a];
    }
}

void setVisitBodies(AppContext& app) {
    const StarSystem& s = app.visit.sys;
    std::string base = std::to_string(app.visit.star);
    app.bodyNames[0] = s.real >= 0 ? realStarAt(s.real).name : "STAR #" + base;
    app.bodyNames[14] = "COMET";
    for (int a = 0; a < 3; a++) app.bodyColors[0][a] = s.color[a];
    for (int i = 0; i < 8; i++) {
        int b = i + 1;
        app.bodyActive[b] = i < s.planetCount;
        if (!app.bodyActive[b]) continue;
        const PlanetSpec& p = s.planets[i];
        app.bodyNames[b] = s.real >= 0 ? std::string(realStarAt(s.real).name) + " " + p.name : base + "-" + (char)('B' + i);
        if (p.habitable) app.bodyNotes[b] = "POSSIBLY HABITABLE";
        for (int a = 0; a < 3; a++) app.bodyColors[b][a] = 0.35f + 0.6f * p.tint[a];
    }
    const char* roman[5] = {"I", "II", "III", "IV", "V"};
    int perParent[9] = {0};
    for (int k = 0; k < 5; k++) {
        int b = 9 + k;
        app.bodyActive[b] = k < s.moonCount;
        if (!app.bodyActive[b]) continue;
        const MoonSpec& m = s.moons[k];
        app.bodyParent[b] = m.parent;
        app.bodyNames[b] = base + "-" + (char)('B' + m.parent - 1) + " " + roman[perParent[m.parent]++];
    }
}

void arrivalCamera(AppContext& app) {
    float d = arrivalDistance(app.visit.sys, app.trueScale);
    float dir[3] = {0.6f, 0.5f, 0.62f};
    float len = length3(dir);
    Camera& c = app.camera;
    for (int a = 0; a < 3; a++) c.pos[a] = dir[a] / len * d;
    c.yaw = std::atan2(dir[0], dir[2]);
    c.pitch = std::asin(dir[1] / len);
    c.free = true;
}

void beginTravel(AppContext& app, int mode, double d0, double d1, const float dir0[3], const float dir1[3], float dur) {
    VisitState& v = app.visit;
    v.mode = mode;
    v.t = 0.0f;
    v.dur = dur;
    v.d0 = d0;
    v.d1 = d1;
    for (int a = 0; a < 3; a++) { v.dir0[a] = dir0[a]; v.dir1[a] = dir1[a]; }
    app.camera.free = true;
    app.follow = -1;
    visitStep(app, 0.0f);
}

void startVisit(AppContext& app, int idx) {
    VisitState& v = app.visit;
    const GalaxyPoint& pt = app.galaxyPts[idx];
    float eye[3];
    galaxyEye(app, eye);
    double k = unitsToLy(app);
    float rel[3];
    for (int a = 0; a < 3; a++) rel[a] = (float)((eye[a] - pt.pos[a]) / k);
    restoreDim(app);
    int ri = galaxyRealStar(app, idx);
    v.sys = ri >= 0 ? realSystem(ri) : generateSystem(galStarSeed(app, (uint32_t)idx), classOf(pt), pt.color);
    v.star = (uint32_t)idx;
    v.active = true;
    v.hover = -1;
    for (int a = 0; a < 3; a++) v.offLy[a] = pt.pos[a];
    resetSystem(app);
    double d0 = length3(rel);
    float dir0[3] = {rel[0] / (float)d0, rel[1] / (float)d0, rel[2] / (float)d0};
    float h = std::sqrt(dir0[0] * dir0[0] + dir0[2] * dir0[2]);
    float dir1[3] = {0.0f, 0.5f, 0.87f};
    if (h > 1e-4f) {
        dir1[0] = dir0[0] / h * 0.87f;
        dir1[2] = dir0[2] / h * 0.87f;
    }
    beginTravel(app, 1, d0, arrivalDistance(v.sys, app.trueScale), dir0, dir1, 4.5f);
}

void finishLeave(AppContext& app) {
    VisitState& v = app.visit;
    double k = unitsToLy(app);
    float pos[3];
    for (int a = 0; a < 3; a++) pos[a] = (float)(v.offLy[a] / k) + v.dir1[a] * (float)v.d1;
    visitClear(app);
    resetSystem(app);
    if (app.gal.active) {
        galStarReturn(app, pos);
        return;
    }
    Camera def;
    double d0 = length3(pos), d1 = length3(def.pos);
    float dir0[3] = {pos[0] / (float)d0, pos[1] / (float)d0, pos[2] / (float)d0};
    float dir1[3] = {def.pos[0] / (float)d1, def.pos[1] / (float)d1, def.pos[2] / (float)d1};
    beginTravel(app, 3, d0, d1, dir0, dir1, 5.0f);
}

}

void visitSync(AppContext& app) {
    bool v = app.visit.active && app.scene == Scene::SOLAR;
    float styles[9][8];
    setBodyDefaults(app);
    if (v) {
        systemStyles(app.visit.sys, styles);
        applyStyleRadii(styles);
        setVisitBodies(app);
    } else {
        homeStyles(styles);
    }
    if (app.styleMapped) std::memcpy(app.styleMapped, styles, sizeof(styles));
}

void visitClear(AppContext& app) {
    VisitState& v = app.visit;
    restoreDim(app);
    v.active = false;
    v.mode = 0;
    v.hover = -1;
    for (int a = 0; a < 3; a++) v.offLy[a] = 0.0f;
    clearBodyRadii();
}

void visitAfterReset(AppContext& app) {
    app.visit.mode = 0;
    if (app.visit.active) arrivalCamera(app);
}

void visitUpdate(AppContext& app) {
    VisitState& v = app.visit;
    bool wantDim = v.active && originUnits(app) < BODY_FAR_UNITS;
    if (wantDim && v.dimmed != (int)v.star && app.galaxyMapped) {
        restoreDim(app);
        v.dimmed = (int)v.star;
        app.galaxyMapped[v.star].bright = 0.0f;
    } else if (!wantDim && v.dimmed >= 0) {
        restoreDim(app);
    }
    v.hover = -1;
    if (app.cu.mode != 0 || v.mode != 0 || !galaxyActive(app) || galaxyDistanceLy(app) > 1.5e6f || !app.cursorOk || app.mouseHeld || app.showHelp) return;
    if (originUnits(app) * unitsToLy(app) < MIN_PICK_LY) return;
    float W = (float)app.swapchainExtent.width;
    if (app.cursorFb[0] > W - 70.0 && app.cursorFb[1] < 70.0) return;
    v.hover = pickStar(app, app.cursorFb[0], app.cursorFb[1]);
}

bool visitClick(AppContext& app, double fbx, double fby) {
    if (!galaxyActive(app) || galaxyDistanceLy(app) > 1.5e6f || app.n < 64 || app.visit.mode != 0) return false;
    if (originUnits(app) * unitsToLy(app) < MIN_PICK_LY) return false;
    int idx = pickStar(app, fbx, fby);
    if (idx < 0) {
        app.nebSel = nebulaPick(app, fbx, fby);
        return app.nebSel >= 0;
    }
    app.nebSel = -1;
    startVisit(app, idx);
    return true;
}

void visitFlyDefault(AppContext& app) {
    float sun[3];
    sunPosition(app, sun);
    float pos[3] = {app.camera.pos[0] - sun[0], app.camera.pos[1] - sun[1], app.camera.pos[2] - sun[2]};
    Camera def;
    double d0 = std::max(length3(pos), 1e-6f), d1 = length3(def.pos);
    float dir0[3] = {pos[0] / (float)d0, pos[1] / (float)d0, pos[2] / (float)d0};
    float dir1[3] = {def.pos[0] / (float)d1, def.pos[1] / (float)d1, def.pos[2] / (float)d1};
    beginTravel(app, 3, d0, d1, dir0, dir1, 5.0f);
}

void visitHome(AppContext& app) {
    if (closeupBack(app)) return;
    VisitState& v = app.visit;
    if (v.mode >= 4) return;
    if (!v.active) {
        galVisitBack(app);
        return;
    }
    if (v.mode == 2) return;
    if (!app.camera.free) enterFree(app);
    float sun[3];
    sunPosition(app, sun);
    float rel[3] = {app.camera.pos[0] - sun[0], app.camera.pos[1] - sun[1], app.camera.pos[2] - sun[2]};
    double d = length3(rel);
    float dir0[3] = {0.0f, 0.5f, 0.87f};
    if (d > 1e-6) for (int a = 0; a < 3; a++) dir0[a] = rel[a] / (float)d;
    else d = 1.0;
    double k = unitsToLy(app);
    float starU[3] = {(float)(v.offLy[0] / k), (float)(v.offLy[1] / k), (float)(v.offLy[2] / k)};
    float sd = length3(starU);
    float toSun[3] = {-starU[0] / sd, -starU[1] / sd, -starU[2] / sd};
    double d1 = std::max(d, 0.5 * sd);
    beginTravel(app, 2, d, d1, dir0, toSun, d1 > d ? 2.5f : 0.3f);
}

void visitStep(AppContext& app, float dt) {
    VisitState& v = app.visit;
    if (v.mode >= 4) {
        galVisitStep(app, dt);
        return;
    }
    v.t += dt;
    float s = std::min(1.0f, v.t / v.dur);
    float e = s * s * (3.0f - 2.0f * s);
    double d = v.d0 * std::pow(v.d1 / v.d0, (double)e);
    float dir[3], f[3];
    for (int a = 0; a < 3; a++) dir[a] = v.dir0[a] * (1.0f - e) + v.dir1[a] * e;
    float dl = length3(dir);
    if (dl < 1e-4f) { dir[0] = 0.0f; dir[1] = 1.0f; dir[2] = 0.0f; dl = 1.0f; }
    for (int a = 0; a < 3; a++) dir[a] /= dl;
    if (v.mode == 2) {
        for (int a = 0; a < 3; a++) f[a] = -v.dir0[a] * (1.0f - e) + v.dir1[a] * e;
        float fl = length3(f);
        if (fl < 1e-3f) { f[0] = 0.0f; f[1] = 0.0f; f[2] = -1.0f; fl = 1.0f; }
        for (int a = 0; a < 3; a++) f[a] /= fl;
    } else {
        for (int a = 0; a < 3; a++) f[a] = -dir[a];
    }
    float sun[3];
    sunPosition(app, sun);
    Camera& c = app.camera;
    c.free = true;
    app.follow = -1;
    for (int a = 0; a < 3; a++) c.pos[a] = sun[a] + dir[a] * (float)d;
    c.yaw = std::atan2(-f[0], -f[2]);
    c.pitch = std::max(-1.5f, std::min(1.5f, std::asin(std::max(-1.0f, std::min(1.0f, -f[1])))));
    if (s < 1.0f) return;
    int mode = v.mode;
    v.mode = 0;
    if (mode == 2) finishLeave(app);
}

void emitVisitLines(AppContext& app, OverlayVertex* out, uint32_t& used, const float viewProj[16], bool bodiesFar) {
    const VisitState& v = app.visit;
    emitGalHoverLines(app, out, used);
    emitCloseupLines(app, out, used);
    if (!galaxyActive(app)) return;
    float aspect = (float)app.swapchainExtent.width / (float)app.swapchainExtent.height;
    float gvp[16];
    galaxyViewProj(app, aspect, gvp);
    float x, y;
    if (v.hover >= 0 && v.mode == 0 && tourOortAlpha(app) < 0.01f && projectTo(gvp, app.galaxyPts[v.hover].pos, x, y)) {
        const float hot[3] = {1.0f, 0.88f, 0.35f};
        ringAt(app, out, used, x, y, 11.0f, 24, hot);
    }
    if (!v.active) return;
    const float origin[3] = {0.0f, 0.0f, 0.0f};
    if (!app.gal.active && projectTo(gvp, origin, x, y)) {
        const float gold[3] = {1.0f, 0.85f, 0.3f};
        ringAt(app, out, used, x, y, 5.0f, 4, gold);
    }
    if (bodiesFar || !app.displayValid) return;
    const float* c = app.displayBodies[0].pos;
    for (int j = 0; j < 4; j++) {
        float au = v.sys.hzIn + (v.sys.hzOut - v.sys.hzIn) * j / 3.0f;
        float r = orbitUnits(au, app.trueScale, v.sys.au0);
        float k = (j == 0 || j == 3) ? 1.0f : 0.35f;
        const float col[3] = {0.1f * k, 0.55f * k, 0.22f * k};
        const int segs = 96;
        float px0 = 0.0f, py0 = 0.0f, px1, py1;
        bool prev = false;
        for (int i = 0; i <= segs; i++) {
            float a = 6.2831853f * i / segs;
            float p[3] = {c[0] + r * std::cos(a), c[1], c[2] + r * std::sin(a)};
            bool ok = projectTo(viewProj, p, px1, py1);
            if (ok && prev) lineTo(out, used, px0, py0, px1, py1, col);
            px0 = px1; py0 = py1; prev = ok;
        }
    }
}

void emitVisitText(AppContext& app, OverlayVertex* out, uint32_t& used, float px, float py, float scale) {
    const VisitState& v = app.visit;
    emitGalHoverText(app, out, used, px, py, scale);
    emitCloseupText(app, out, used);
    if (!galaxyActive(app)) return;
    float aspect = (float)app.swapchainExtent.width / (float)app.swapchainExtent.height;
    float gvp[16];
    galaxyViewProj(app, aspect, gvp);
    float x, y;
    if (v.hover >= 0 && v.mode == 0 && tourOortAlpha(app) < 0.01f && projectTo(gvp, app.galaxyPts[v.hover].pos, x, y)) {
        const GalaxyPoint& p = app.galaxyPts[v.hover];
        int cls = classOf(p);
        float q[3] = {p.pos[0], p.pos[1], p.pos[2]};
        if (app.gal.active) for (int a = 0; a < 3; a++) q[a] += app.gal.spec.anchor[a];
        int ri = galaxyRealStar(app, v.hover);
        std::string text = ri >= 0 ? std::string(realStarAt(ri).name) + "  " + fmtFloat(realStarAt(ri).distLy, realStarAt(ri).distLy < 100.0f ? 1 : 0) + " LY  REAL"
            : "STAR #" + std::to_string(v.hover) + "  " + SPECTRAL[cls].name + "  " + fmtLy(length3(q)) + " LY";
        emitText(out, used, text.c_str(), x + 16.0f * px, y - 4.0f * py, px, py, scale, 1.0f, 0.9f, 0.45f);
    }
    if (v.active && !app.gal.active) {
        const float origin[3] = {0.0f, 0.0f, 0.0f};
        if (projectTo(gvp, origin, x, y)) {
            std::string text = "SUN  " + fmtLy(length3(v.offLy)) + " LY";
            emitText(out, used, text.c_str(), x + 10.0f * px, y - 10.0f * py, px, py, scale, 1.0f, 0.85f, 0.3f);
        }
    }
}

std::vector<CardLine> visitCard(const AppContext& app) {
    std::vector<CardLine> lines;
    const VisitState& v = app.visit;
    if (app.scene != Scene::SOLAR) return lines;
    std::vector<CardLine> body = bodyCard(app);
    if (!body.empty()) return body;
    if (app.cu.mode == 2) return closeupCard(app);
    if (!v.active && app.nebSel >= 0 && galaxyActive(app)) return nebulaCard(app);
    if (!v.active) return galaxyCard(app);
    const StarSystem& s = v.sys;
    const float white[3] = {1.0f, 1.0f, 1.0f}, text[3] = {0.85f, 0.87f, 0.92f}, hint[3] = {0.6f, 0.65f, 0.75f};
    const float green[3] = {0.4f, 1.0f, 0.55f};
    if (s.real >= 0) {
        lines.push_back({realStarAt(s.real).name, {white[0], white[1], white[2]}});
        lines.push_back({"REAL SYSTEM", {green[0], green[1], green[2]}});
    } else {
        lines.push_back({"STAR #" + std::to_string(v.star), {white[0], white[1], white[2]}});
    }
    lines.push_back({std::string("CLASS ") + SPECTRAL[s.cls].name + "  " + CLASS_COLOR[s.cls], {s.color[0], s.color[1], s.color[2]}});
    lines.push_back({"MASS " + fmtFloat(s.mass, 2) + " SUNS", {text[0], text[1], text[2]}});
    lines.push_back({"LUMINOSITY " + fmtFloat(s.lum, s.lum < 0.01f ? 5 : s.lum < 10.0f ? 2 : 0) + " SUNS", {text[0], text[1], text[2]}});
    lines.push_back({"AGE ~" + fmtFloat(s.ageGyr, s.ageGyr < 1.0f ? 2 : 1) + " GYR OF ~" + fmtFloat(s.lifeGyr, s.lifeGyr < 10.0f ? 1 : 0), {text[0], text[1], text[2]}});
    if (s.real >= 0 && s.planetCount == 0) lines.push_back({"NO CONFIRMED PLANETS", {text[0], text[1], text[2]}});
    else lines.push_back({"PLANETS " + std::to_string(s.planetCount) + "  MOONS " + std::to_string(s.moonCount), {text[0], text[1], text[2]}});
    lines.push_back({"HAB ZONE " + fmtFloat(s.hzIn, s.hzOut < 0.1f ? 3 : 2) + "-" + fmtFloat(s.hzOut, s.hzOut < 0.1f ? 3 : 2) + " AU", {green[0], green[1], green[2]}});
    int hab = 0;
    for (int i = 0; i < s.planetCount; i++) hab += s.planets[i].habitable ? 1 : 0;
    if (hab) lines.push_back({"POSSIBLY HABITABLE " + std::to_string(hab), {green[0], green[1], green[2]}});
    if (app.gal.active) {
        float q[3] = {v.offLy[0] + app.gal.spec.anchor[0], v.offLy[1] + app.gal.spec.anchor[1], v.offLy[2] + app.gal.spec.anchor[2]};
        lines.push_back({"IN " + app.gal.spec.name, {text[0], text[1], text[2]}});
        lines.push_back({"FROM CENTRE " + fmtLy(length3(q)) + " LY", {text[0], text[1], text[2]}});
        lines.push_back({"BACKSPACE = BACK TO GALAXY", {hint[0], hint[1], hint[2]}});
        return lines;
    }
    lines.push_back({"FROM SUN " + (s.real >= 0 ? fmtFloat(realStarAt(s.real).distLy, realStarAt(s.real).distLy < 100.0f ? 2 : 0) : fmtLy(length3(v.offLy))) + " LY", {text[0], text[1], text[2]}});
    lines.push_back({"BACKSPACE = HOME", {hint[0], hint[1], hint[2]}});
    return lines;
}
