#include "vk_app.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr unsigned GAL_STARS = 150000;
constexpr unsigned MW_SEED = 12345;
constexpr float OUT_MLY = 2.3f;
constexpr float FRAME_R = 2.2f;
constexpr float PICK_PX = 8.0f;

std::string g_mwLabel, g_title, g_bhLabel, g_hoverText;
int g_hoverIdx = -1;

double unitsToLy(const AppContext& app) {
    return (double)LY_PER_AU_INV / solarUnitsPerAu(app.trueScale);
}

double len3(const double v[3]) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

void normalize3(double v[3]) {
    double l = len3(v);
    if (l < 1e-18) { v[0] = 0.0; v[1] = 0.0; v[2] = 1.0; return; }
    for (int a = 0; a < 3; a++) v[a] /= l;
}

double smoothStep(double s) {
    s = std::min(1.0, std::max(0.0, s));
    return s * s * (3.0 - 2.0 * s);
}

void absEye(const AppContext& app, double out[3]) {
    float e[3];
    galaxyEye(app, e);
    for (int a = 0; a < 3; a++) out[a] = app.gal.originMly[a] + (double)e[a] * 1e-6;
}

void setCamera(AppContext& app, const double E[3], const double fwd[3]) {
    double k = unitsToLy(app);
    float sun[3];
    sunPosition(app, sun);
    Camera& c = app.camera;
    for (int a = 0; a < 3; a++) {
        double local = (E[a] - app.gal.originMly[a]) * 1e6 - (double)app.visit.offLy[a];
        c.pos[a] = sun[a] + (float)(local / k);
    }
    c.yaw = (float)std::atan2(-fwd[0], -fwd[2]);
    c.pitch = (float)std::max(-1.5, std::min(1.5, std::asin(std::max(-1.0, std::min(1.0, -fwd[1])))));
    c.free = true;
    app.follow = -1;
}

const char* typeWord(GalType t) {
    return t == GalType::SPIRAL ? "SPIRAL" : t == GalType::ELLIPTICAL ? "ELLIPTICAL" : t == GalType::IRREGULAR ? "IRREGULAR" : "LENTICULAR";
}

std::string commas(long long v) {
    std::string s = std::to_string(v);
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ",");
    return s;
}

std::string fmtMly(float mly) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), mly < 100.0f ? "%.1f" : "%.0f", mly);
    return buf;
}

std::string fmtStars(float billions) {
    char buf[40];
    if (billions < 1.0f) std::snprintf(buf, sizeof(buf), "~%.0f MILLION", billions * 1000.0f);
    else if (billions < 1000.0f) std::snprintf(buf, sizeof(buf), billions < 10.0f ? "~%.1f BILLION" : "~%.0f BILLION", billions);
    else std::snprintf(buf, sizeof(buf), "~%.1f TRILLION", billions / 1000.0f);
    return buf;
}

struct Lookback { std::vector<double> z, dc, tl; };

const Lookback& lookback() {
    static Lookback L = [] {
        Lookback r;
        const double H0 = 67.7, Om = 0.31, OL = 0.69;
        const double cH0 = 299792.458 / H0 * 3.26156;
        const double tH = 977.8 / H0;
        double dc = 0.0, tl = 0.0, z = 0.0;
        auto E = [&](double q) { return std::sqrt(Om * (1 + q) * (1 + q) * (1 + q) + OL); };
        r.z.push_back(0.0); r.dc.push_back(0.0); r.tl.push_back(0.0);
        for (int k = 1; z < 1100.0; k++) {
            double dz = 0.002 * (1.0 + z), zm = z + 0.5 * dz;
            dc += cH0 * dz / E(zm);
            tl += tH * dz / ((1 + zm) * E(zm));
            z += dz;
            if (k % 8 == 0) { r.z.push_back(z); r.dc.push_back(dc); r.tl.push_back(tl); }
        }
        return r;
    }();
    return L;
}

void lookbackOf(float mly, double& gyr, double& z) {
    const Lookback& L = lookback();
    double d = mly;
    size_t i = std::upper_bound(L.dc.begin(), L.dc.end(), d) - L.dc.begin();
    if (i >= L.dc.size()) i = L.dc.size() - 1;
    if (i == 0) i = 1;
    double f = (d - L.dc[i - 1]) / (L.dc[i] - L.dc[i - 1]);
    z = L.z[i - 1] + f * (L.z[i] - L.z[i - 1]);
    gyr = (L.tl[i - 1] + f * (L.tl[i] - L.tl[i - 1]));
}

std::string fmtAge(float mly) {
    char buf[64];
    if (mly < 100.0f) {
        std::snprintf(buf, sizeof(buf), mly < 10.0f ? "%.1f MILLION YEARS AGO" : "%.0f MILLION YEARS AGO", mly);
        return buf;
    }
    double gyr, z;
    lookbackOf(mly, gyr, z);
    if (gyr < 1.0) std::snprintf(buf, sizeof(buf), "%.0f MILLION YEARS AGO (REDSHIFT %.3f)", gyr * 1000.0, z);
    else std::snprintf(buf, sizeof(buf), "%.2f BILLION YEARS AGO (REDSHIFT %.2f)", gyr, z);
    return buf;
}

void applyHome(AppContext& app) {
    GalState& G = app.gal;
    std::vector<GalaxyPoint> pts = makeMilkyWay(GAL_STARS, MW_SEED);
    std::vector<GalaxyLabel> labels = milkyWayLabels();
    float n[3];
    galaxyNormal(n);
    galaxyUpload(app, pts, labels);
    sgrSet(app, labels[1].pos, n, SGR_MSUN);
    galFrameHome(app);
    universeStandIn({});
    universeHide(app, -1);
    G.uniPoint = -1;
    G.hidden = false;
    G.fadeIn = 0.0f;
}

void applyGalaxy(AppContext& app, const GalaxySpec& spec, int uniPoint) {
    GalState& G = app.gal;
    GalaxySpec sp = spec;
    std::vector<GalaxyPoint> pts = generateGalaxy(sp, GAL_STARS);
    g_bhLabel = sp.name + (sp.bhMsun > 0.0f ? " - CENTRAL BLACK HOLE" : " - CENTRE");
    GalaxyLabel lab;
    lab.text = g_bhLabel.c_str();
    for (int a = 0; a < 3; a++) lab.pos[a] = -sp.anchor[a];
    lab.color[0] = 1.0f; lab.color[1] = 0.75f; lab.color[2] = 0.45f;
    galaxyUpload(app, pts, {lab});
    sgrSet(app, lab.pos, sp.normal, sp.bhMsun);
    G.spec = sp;
    G.active = true;
    G.scale = sp.radiusLy / MW_RADIUS_LY;
    for (int a = 0; a < 3; a++) {
        G.centreMly[a] = sp.centreMly[a];
        G.originMly[a] = sp.centreMly[a] + (double)sp.anchor[a] * 1e-6;
    }
    float rM = sp.radiusLy * 1e-6f;
    std::vector<UniPoint> st;
    for (size_t i = 0; i < pts.size(); i += 25) {
        UniPoint u;
        for (int a = 0; a < 3; a++) u.pos[a] = (float)(sp.centreMly[a] + ((double)pts[i].pos[a] + sp.anchor[a]) * 1e-6);
        u.radius = 0.024f * rM;
        for (int a = 0; a < 3; a++) u.color[a] = pts[i].color[a];
        u.bright = 0.9f;
        u.fadeD = 40.0f;
        st.push_back(u);
    }
    UniPoint core;
    for (int a = 0; a < 3; a++) core.pos[a] = (float)sp.centreMly[a];
    core.radius = 0.3f * rM;
    core.color[0] = 1.0f; core.color[1] = 0.82f; core.color[2] = 0.55f;
    core.bright = 3.6f;
    core.fadeD = 40.0f;
    st.push_back(core);
    universeStandIn(st);
    universeHide(app, -1);
    G.uniPoint = uniPoint;
    G.hidden = false;
    g_title = sp.name + "  " + typeWord(sp.type);
    G.fadeIn = 0.0f;
}

bool eligible(const AppContext& app) {
    if (app.scene != Scene::SOLAR || !app.showUniverse || app.tour.mode != 0) return false;
    if (app.visit.mode != 0 || app.showHelp || app.n < 64) return false;
    return galaxyLayerFade(app) < 0.01f && universeFade(app) > 0.3f;
}

void beginFlight(AppContext& app, const GalaxySpec& spec, const double E0[3]) {
    GalState& G = app.gal;
    double C[3] = {spec.centreMly[0], spec.centreMly[1], spec.centreMly[2]};
    double rel[3] = {E0[0] - C[0], E0[1] - C[1], E0[2] - C[2]};
    G.d0 = std::max(len3(rel), 1e-6);
    for (int a = 0; a < 3; a++) { G.e0[a] = E0[a]; G.dir0[a] = rel[a] / G.d0; }
    double n[3] = {spec.normal[0], spec.normal[1], spec.normal[2]};
    double dn = G.dir0[0] * n[0] + G.dir0[1] * n[1] + G.dir0[2] * n[2];
    if (dn < 0.0) for (int a = 0; a < 3; a++) n[a] = -n[a];
    double side[3];
    for (int a = 0; a < 3; a++) side[a] = G.dir0[a] - n[a] * std::fabs(dn);
    if (len3(side) < 1e-3) for (int a = 0; a < 3; a++) side[a] = spec.e1[a];
    normalize3(side);
    for (int a = 0; a < 3; a++) G.dir1[a] = n[a] * 0.75 + side[a] * 0.66;
    normalize3(G.dir1);
    G.d1 = FRAME_R * spec.radiusLy * 1e-6;
    G.kind = 1;
    G.t = 0.0f;
    G.dur = 5.0f;
    app.visit.mode = 4;
    app.camera.free = true;
    app.follow = -1;
    galVisitStep(app, 0.0f);
}

void beginLeave(AppContext& app) {
    GalState& G = app.gal;
    double E0[3];
    absEye(app, E0);
    float cf[3];
    galaxyCentreLy(cf);
    double sunM[3] = {-(double)cf[0] * 1e-6, -(double)cf[1] * 1e-6, -(double)cf[2] * 1e-6};
    double toA[3] = {G.centreMly[0] - sunM[0], G.centreMly[1] - sunM[1], G.centreMly[2] - sunM[2]};
    normalize3(toA);
    double w[3] = {toA[2], 0.0, -toA[0]};
    if (len3(w) < 1e-3) { w[0] = 1.0; w[1] = 0.0; w[2] = 0.0; }
    normalize3(w);
    for (int a = 0; a < 3; a++) {
        G.e0[a] = E0[a];
        G.p1[a] = sunM[a] + w[a] * 3.5;
    }
    float cp = std::cos(app.camera.pitch);
    G.f0[0] = -cp * std::sin(app.camera.yaw);
    G.f0[1] = -std::sin(app.camera.pitch);
    G.f0[2] = -cp * std::cos(app.camera.yaw);
    G.kind = 2;
    G.t = 0.0f;
    G.dur = 4.5f;
    G.swapped = false;
    app.visit.mode = 5;
    app.camera.free = true;
    app.follow = -1;
    galVisitStep(app, 0.0f);
}

}

void galFrameHome(AppContext& app) {
    GalState& G = app.gal;
    float c[3];
    galaxyCentreLy(c);
    G.active = false;
    G.scale = 1.0f;
    for (int a = 0; a < 3; a++) {
        G.originMly[a] = -(double)c[a] * 1e-6;
        G.centreMly[a] = 0.0;
    }
}

const char* galMwLabel(const AppContext& app) {
    g_mwLabel = "MILKY WAY - HOME " + fmtMly(app.gal.spec.distMly) + " MLY";
    return g_mwLabel.c_str();
}

const char* galTitle(const AppContext& app) {
    (void)app;
    return g_title.c_str();
}

uint32_t galStarSeed(const AppContext& app, uint32_t idx) {
    if (!app.gal.active) return idx;
    return idx ^ ((app.gal.spec.index + 1u) * 0x9E3779B1u);
}

bool galVisitClick(AppContext& app, double fbx, double fby) {
    if (!eligible(app)) return false;
    float pos[3];
    int named = 0;
    int idx = universePick(app, fbx, fby, pos, named);
    if (idx < 0) return false;
    double E0[3];
    absEye(app, E0);
    double C[3] = {pos[0], pos[1], pos[2]};
    GalaxySpec spec = galaxySpec((uint32_t)idx, named, C);
    if (app.visit.active) {
        visitClear(app);
        resetSystem(app);
    }
    applyGalaxy(app, spec, universeGalaxyPoint(idx));
    app.gal.hover = -1;
    beginFlight(app, app.gal.spec, E0);
    return true;
}

void galVisitUpdate(AppContext& app) {
    GalState& G = app.gal;
    static double last = 0.0;
    double now = glfwGetTime();
    float dt = (float)std::min(0.1, now - last);
    last = now;
    if (G.fadeIn < 1.0f) G.fadeIn = std::min(1.0f, G.fadeIn + dt / 1.2f);
    if (G.active && G.uniPoint >= 0) {
        double E[3];
        absEye(app, E);
        double d[3] = {E[0] - G.centreMly[0], E[1] - G.centreMly[1], E[2] - G.centreMly[2]};
        bool near = len3(d) < 30.0;
        if (near != G.hidden) {
            universeHide(app, near ? G.uniPoint : -1);
            G.hidden = near;
        }
    }
    float W = (float)app.swapchainExtent.width;
    if (app.cu.mode != 0 || !eligible(app) || !app.cursorOk || app.mouseHeld || (app.cursorFb[0] > W - 70.0 && app.cursorFb[1] < 70.0)) {
        G.hover = -1;
        return;
    }
    if (++G.hoverFrame % 4 != 0) return;
    float pos[3];
    int named = 0;
    G.hover = universePick(app, app.cursorFb[0], app.cursorFb[1], pos, named);
    if (G.hover >= 0) {
        for (int a = 0; a < 3; a++) G.hoverPos[a] = pos[a];
        G.hoverNamed = named;
    }
}

void galVisitStep(AppContext& app, float dt) {
    GalState& G = app.gal;
    G.t += dt;
    float s = std::min(1.0f, G.t / G.dur);
    double e = smoothStep(s);
    double E[3], look[3], fwd[3];
    if (G.kind == 1) {
        double d = G.d0 * std::pow(G.d1 / G.d0, e);
        double dir[3];
        for (int a = 0; a < 3; a++) dir[a] = G.dir0[a] * (1.0 - e) + G.dir1[a] * e;
        normalize3(dir);
        for (int a = 0; a < 3; a++) {
            E[a] = G.spec.centreMly[a] + dir[a] * d;
            fwd[a] = -dir[a];
        }
    } else {
        for (int a = 0; a < 3; a++) E[a] = G.e0[a] + (G.p1[a] - G.e0[a]) * e;
        float cf[3];
        galaxyCentreLy(cf);
        double toSun[3];
        for (int a = 0; a < 3; a++) toSun[a] = -(double)cf[a] * 1e-6 - E[a];
        normalize3(toSun);
        double eb = smoothStep(s * 1.6);
        for (int a = 0; a < 3; a++) fwd[a] = G.f0[a] * (1.0 - eb) + toSun[a] * eb;
        normalize3(fwd);
        double dA[3] = {E[0] - G.centreMly[0], E[1] - G.centreMly[1], E[2] - G.centreMly[2]};
        if (!G.swapped && (len3(dA) * 1e6 / G.scale > OUT_MLY * 1e6 || s >= 1.0f)) {
            applyHome(app);
            G.swapped = true;
        }
    }
    for (int a = 0; a < 3; a++) look[a] = fwd[a];
    setCamera(app, E, look);
    if (s < 1.0f) return;
    int kind = G.kind;
    G.kind = 0;
    app.visit.mode = 0;
    if (kind == 2) visitFlyDefault(app);
}

bool galVisitBack(AppContext& app) {
    if (!app.gal.active || app.visit.mode != 0 || app.visit.active) return false;
    beginLeave(app);
    return true;
}

void galVisitReset(AppContext& app) {
    GalState& G = app.gal;
    G.hover = -1;
    G.kind = 0;
    if (!G.active) return;
    applyHome(app);
    G.fadeIn = 1.0f;
    app.visit.mode = 0;
}

void galStarReturn(AppContext& app, const float posUnits[3]) {
    double k = unitsToLy(app);
    double E0[3];
    for (int a = 0; a < 3; a++) E0[a] = app.gal.originMly[a] + (double)posUnits[a] * k * 1e-6;
    beginFlight(app, app.gal.spec, E0);
}

void emitGalHoverLines(AppContext& app, OverlayVertex* out, uint32_t& used) {
    const GalState& G = app.gal;
    if (G.hover < 0 || !eligible(app)) return;
    float x, y;
    if (!universeProject(app, G.hoverPos, x, y)) return;
    float px = 2.0f / (float)app.swapchainExtent.width, py = 2.0f / (float)app.swapchainExtent.height;
    const float hot[3] = {1.0f, 0.88f, 0.35f};
    const int segs = 24;
    for (int i = 0; i < segs && used + 2 <= MAX_VERTS; i++) {
        float a0 = 6.2831853f * i / segs, a1 = 6.2831853f * (i + 1) / segs;
        out[used++] = {x + 11.0f * px * std::cos(a0), y + 11.0f * py * std::sin(a0), hot[0], hot[1], hot[2]};
        out[used++] = {x + 11.0f * px * std::cos(a1), y + 11.0f * py * std::sin(a1), hot[0], hot[1], hot[2]};
    }
}

void emitGalHoverText(AppContext& app, OverlayVertex* out, uint32_t& used, float px, float py, float scale) {
    const GalState& G = app.gal;
    if (G.hover < 0 || !eligible(app)) return;
    float x, y;
    if (!universeProject(app, G.hoverPos, x, y)) return;
    if (g_hoverIdx != G.hover) {
        double C[3] = {G.hoverPos[0], G.hoverPos[1], G.hoverPos[2]};
        GalaxySpec s = galaxySpec((uint32_t)G.hover, G.hoverNamed, C);
        g_hoverText = (s.named ? s.name : "GALAXY #" + std::to_string(G.hover)) + "  " + typeWord(s.type) + "  " + fmtMly(s.distMly) + " MLY";
        g_hoverIdx = G.hover;
    }
    emitText(out, used, g_hoverText.c_str(), x + 16.0f * px, y - 4.0f * py, px, py, scale, 1.0f, 0.9f, 0.45f);
}

std::vector<CardLine> galaxyCard(const AppContext& app) {
    std::vector<CardLine> lines;
    const GalState& G = app.gal;
    if (!G.active || app.scene != Scene::SOLAR) return lines;
    const GalaxySpec& S = G.spec;
    const float white[3] = {1.0f, 1.0f, 1.0f}, text[3] = {0.85f, 0.87f, 0.92f}, hint[3] = {0.6f, 0.65f, 0.75f};
    const float kind[4][3] = {{0.6f, 0.8f, 1.0f}, {1.0f, 0.75f, 0.45f}, {0.7f, 1.0f, 0.8f}, {1.0f, 0.9f, 0.6f}};
    const float* kc = kind[(int)S.type];
    lines.push_back({S.name, {white[0], white[1], white[2]}});
    lines.push_back({"TYPE " + S.typeName, {kc[0], kc[1], kc[2]}});
    lines.push_back({"DIAMETER " + commas((long long)std::llround(S.radiusLy * 2.0f / 1000.0f) * 1000) + " LY", {text[0], text[1], text[2]}});
    lines.push_back({"STARS " + fmtStars(S.starsBillion), {text[0], text[1], text[2]}});
    lines.push_back({"BLACK HOLE " + fmtMass(S.bhMsun), {text[0], text[1], text[2]}});
    lines.push_back({"FROM MILKY WAY " + fmtMly(S.distMly) + " MLY", {text[0], text[1], text[2]}});
    lines.push_back({"LIGHT LEFT IT " + fmtAge(S.distMly), {text[0], text[1], text[2]}});
    lines.push_back({"BACKSPACE = HOME", {hint[0], hint[1], hint[2]}});
    return lines;
}
