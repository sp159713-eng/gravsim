#include "closeup_view.h"
#include "closeup_data.h"
#include "nebulae.h"
#include "universe.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

std::string g_closeupName;

namespace {

struct LabelBox {
    float x0, y0, x1, y1;
    int kind, obj;
};

std::vector<LabelBox> g_boxes;

float smooth01(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

void toSim(const Binary& b, float out[3]) {
    float l = b.l * 3.14159265f / 180.0f, bb = b.b * 3.14159265f / 180.0f;
    float g[3] = {b.distLy * std::cos(bb) * std::cos(l), b.distLy * std::cos(bb) * std::sin(l), b.distLy * std::sin(bb)};
    galaxySunToSim(g, out);
}

void targetPos(int kind, int obj, float pos[3], float& standoff) {
    if (kind == 0) {
        const NebulaInfo& n = nebulaInfos()[obj];
        for (int a = 0; a < 3; a++) pos[a] = n.pos[a];
        standoff = 3.0f * n.diameterLy * 0.5f;
    } else if (kind == 1) {
        sgrCentre(pos);
        standoff = 40.0f;
    } else {
        toSim(binaryAt(obj), pos);
        standoff = 2.0f;
    }
}

float bhGain(const AppContext& app) {
    float d = galaxyDistanceLy(app);
    return std::clamp((d - 60.0f) / 120.0f, 0.0f, 1.0f) * galaxyLayerFade(app);
}

bool sgrVisible(const AppContext& app) {
    return galaxyDistanceLy(app) >= GALAXY_LABEL_LY && galaxyLayerFade(app) >= 0.3f;
}

bool projectLy(const AppContext& app, const float p[3], float& sx, float& sy) {
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float vp[16];
    galaxyViewProj(app, W / H, vp);
    float c[4];
    for (int row = 0; row < 4; row++) c[row] = vp[row] * p[0] + vp[row + 4] * p[1] + vp[row + 8] * p[2] + vp[row + 12];
    if (c[3] <= 1e-9f) return false;
    sx = (c[0] / c[3] + 1.0f) * 0.5f * W;
    sy = (c[1] / c[3] + 1.0f) * 0.5f * H;
    return true;
}

void beginFlight(AppContext& app, int kind, int obj) {
    float pos[3], standoff;
    targetPos(kind, obj, pos, standoff);
    CloseupState& c = app.cu;
    c = CloseupState{};
    c.kind = kind;
    c.obj = obj;
    c.saved = app.camera;
    c.savedFollow = app.follow;
    c.savedRate = app.tm.rate;
    float e[3];
    galaxyEye(app, e);
    double d[3] = {e[0] - pos[0], e[1] - pos[1], e[2] - pos[2]};
    double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len < 1e-3) {
        d[0] = 0.3; d[1] = 0.5; d[2] = 0.8;
        len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    }
    for (int a = 0; a < 3; a++) {
        c.targetLy[a] = pos[a];
        c.dir[a] = (float)(d[a] / len);
    }
    c.d0 = len;
    c.d1 = std::min((double)standoff, len);
    c.mode = 1;
    c.rate = kind == 2 ? binaryAt(obj).defRate : 1.0f;
    c.zoom = 1.0f;
    if (kind == 1) { c.vyaw = 0.55f; c.vpitch = 0.5f; }
    if (kind == 2) { c.vyaw = 0.4f; c.vpitch = 0.55f; }
    app.follow = -1;
}

void placeCamera(AppContext& app, double dist) {
    CloseupState& c = app.cu;
    float sun[3];
    sunPosition(app, sun);
    float k = LY_PER_AU_INV / solarUnitsPerAu(app.trueScale);
    Camera& cam = app.camera;
    for (int a = 0; a < 3; a++) {
        float rel = c.targetLy[a] + c.dir[a] * (float)dist - app.visit.offLy[a];
        cam.pos[a] = sun[a] + rel / k;
    }
    cam.yaw = std::atan2(c.dir[0], c.dir[2]);
    cam.pitch = std::asin(std::clamp(c.dir[1], -1.0f, 1.0f));
    cam.free = true;
    app.zoomValid = false;
}

void beginExit(AppContext& app) {
    CloseupState& c = app.cu;
    if (c.mode == 0 || c.mode == 3) return;
    app.camera = c.saved;
    app.follow = c.savedFollow;
    app.zoomValid = false;
    c.mode = 3;
}

int findByName(const char* name, int& kind) {
    auto up = [](std::string s) { for (char& ch : s) ch = (char)std::toupper((unsigned char)ch); return s; };
    std::string want = up(name);
    const std::vector<NebulaInfo>& infos = nebulaInfos();
    for (size_t i = 0; i < infos.size(); i++)
        if (up(infos[i].name).find(want) != std::string::npos) { kind = 0; return (int)i; }
    if (want.find("SGR") == 0) { kind = 1; return 0; }
    for (int i = 0; i < binaryCount(); i++)
        if (up(binaryAt(i).name).find(want) != std::string::npos) { kind = 2; return i; }
    return -1;
}

}

bool cuAllowed(const AppContext& app) {
    return app.scene == Scene::SOLAR && galaxyActive(app) && !app.gal.active && !app.visit.active && app.visit.mode == 0 && app.tour.mode == 0;
}

void closeupStart(AppContext& app, const char* name) {
    (void)app;
    g_closeupName = name;
}

bool closeupCovers(const AppContext& app) {
    return app.cu.mode != 0 && app.cu.alpha > 0.999f;
}

bool closeupActive(const AppContext& app) {
    return app.cu.mode == 2;
}

bool closeupBack(AppContext& app) {
    if (app.cu.mode == 0) return false;
    beginExit(app);
    return true;
}

void closeupNoteLabels(const std::vector<ScreenLabel>& labels, float W, float H) {
    g_boxes.clear();
    const std::vector<NebulaInfo>& infos = nebulaInfos();
    for (const ScreenLabel& l : labels) {
        if (!l.text || l.ring > 0.0f || l.follow) continue;
        int kind = -1, obj = -1;
        if (std::strncmp(l.text, "SGR A*", 6) == 0) { kind = 1; obj = 0; }
        for (size_t i = 0; kind < 0 && i < infos.size(); i++)
            if (l.text == infos[i].label.c_str()) { kind = 0; obj = (int)i; }
        for (int i = 0; kind < 0 && i < binaryCount(); i++)
            if (std::strcmp(l.text, binaryAt(i).label) == 0) { kind = 2; obj = i; }
        if (kind < 0) continue;
        float cx = (l.x + 1.0f) * 0.5f * W, cy = (l.y + 1.0f) * 0.5f * H;
        g_boxes.push_back({cx, cy + l.oy - 3.0f, cx + 12.0f + 12.0f * (float)std::strlen(l.text), cy + l.oy + 17.0f, kind, obj});
    }
}

void closeupLabels(AppContext& app, std::vector<ScreenLabel>& out) {
    if (app.cu.mode == 2 || app.gal.active) return;
    float f = bhGain(app);
    if (f < 0.3f) return;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float vp[16];
    galaxyViewProj(app, W / H, vp);
    for (int i = 0; i < binaryCount(); i++) {
        const Binary& b = binaryAt(i);
        float p[3];
        toSim(b, p);
        float c[4];
        for (int row = 0; row < 4; row++) c[row] = vp[row] * p[0] + vp[row + 4] * p[1] + vp[row + 8] * p[2] + vp[row + 12];
        if (c[3] <= 1e-9f) continue;
        float nx = c[0] / c[3], ny = c[1] / c[3];
        ScreenLabel s{nx, ny, b.label, {0.9f * f, 0.55f * f, 1.0f * f}};
        s.prio = 2.3f;
        out.push_back(s);
        ScreenLabel ring{nx, ny, nullptr, {0.9f * f, 0.55f * f, 1.0f * f}};
        ring.ring = 6.0f;
        out.push_back(ring);
    }
}

bool closeupClick(AppContext& app, double fx, double fy) {
    if (app.cu.mode != 0) return true;
    if (!cuAllowed(app) || app.n < 64) return false;
    if (app.showLabels)
        for (const LabelBox& b : g_boxes)
            if (fx >= b.x0 && fx <= b.x1 && fy >= b.y0 && fy <= b.y1) {
                beginFlight(app, b.kind, b.obj);
                return true;
            }
    float sx, sy, p[3];
    if (sgrVisible(app)) {
        sgrCentre(p);
        if (projectLy(app, p, sx, sy) && std::hypot(sx - fx, sy - fy) < 14.0f) {
            beginFlight(app, 1, 0);
            return true;
        }
    }
    if (bhGain(app) > 0.3f)
        for (int i = 0; i < binaryCount(); i++) {
            toSim(binaryAt(i), p);
            if (projectLy(app, p, sx, sy) && std::hypot(sx - fx, sy - fy) < 14.0f) {
                beginFlight(app, 2, i);
                return true;
            }
        }
    int n = nebulaPick(app, fx, fy);
    if (n >= 0) {
        beginFlight(app, 0, n);
        return true;
    }
    return false;
}

bool closeupStep(AppContext& app, float dt) {
    CloseupState& c = app.cu;
    if (!g_closeupName.empty() && c.mode == 0 && app.displayValid && cuAllowed(app)) {
        if (g_closeupName == "PHOENIX" || g_closeupName == "TON") {
            const char* want = g_closeupName == "TON" ? "TON 618" : "PHOENIX CLUSTER";
            g_closeupName.clear();
            for (const UniLabel& l : universeLabels()) {
                if (std::strcmp(l.text, want) != 0) continue;
                float sun[3];
                sunPosition(app, sun);
                float k = LY_PER_AU_INV / solarUnitsPerAu(app.trueScale);
                const float dir[3] = {0.5f, 0.25f, 0.83f};
                float dl = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
                for (int a = 0; a < 3; a++)
                    app.camera.pos[a] = sun[a] + (float)(((double)l.pos[a] - app.gal.originMly[a]) * 1e6 + dir[a] / dl * 150000.0) / k;
                app.camera.yaw = std::atan2(dir[0], dir[2]);
                app.camera.pitch = std::asin(dir[1] / dl);
                app.camera.free = true;
                app.zoomValid = false;
            }
            return false;
        }
        int kind = -1;
        int obj = findByName(g_closeupName.c_str(), kind);
        g_closeupName.clear();
        if (obj >= 0) beginFlight(app, kind, obj);
    }
    if (c.mode == 0) return false;
    if (app.scene != Scene::SOLAR || app.visit.active) {
        c.mode = 0;
        return false;
    }
    dt = std::min(dt, 0.1f);
    c.clock += dt;
    GLFWwindow* w = app.window;
    bool esc = glfwGetKey(w, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (esc && !c.escDown && !c.helpWas) beginExit(app);
    c.escDown = esc;
    c.helpWas = app.showHelp;
    app.tm.rate = c.savedRate;
    if (c.mode == 3) {
        c.alpha -= dt / 0.9f;
        if (c.alpha <= 0.0f) {
            c.alpha = 0.0f;
            c.mode = 0;
        }
        return false;
    }
    Camera& cam = app.camera;
    if (c.mode == 1) {
        c.t += dt;
        float s = std::min(1.0f, c.t / c.dur);
        float e = smooth01(s);
        double dist = c.d0 * std::pow(c.d1 / c.d0, (double)e);
        placeCamera(app, dist);
        c.alpha = smooth01((e - 0.55f) / 0.45f);
        if (s >= 1.0f) {
            c.mode = 2;
            c.alpha = 1.0f;
            cam.yaw = 0.0f;
            cam.pitch = 0.0f;
            cam.zoomImpulse = 0.0f;
        }
        return true;
    }
    float zi = cam.zoomImpulse;
    cam.zoomImpulse = 0.0f;
    c.zoom = std::clamp(c.zoom * std::pow(0.9f, zi), 0.12f, 4.0f);
    if (c.kind == 0) {
        c.panX += cam.yaw * 0.4f * c.zoom;
        c.panY += cam.pitch * 0.4f * c.zoom;
    } else {
        c.vyaw -= cam.yaw;
        c.vpitch = std::clamp(c.vpitch - cam.pitch, -1.45f, 1.45f);
    }
    cam.yaw = 0.0f;
    cam.pitch = 0.0f;
    bool plus = glfwGetKey(w, GLFW_KEY_EQUAL) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_KP_ADD) == GLFW_PRESS;
    bool minus = glfwGetKey(w, GLFW_KEY_MINUS) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS;
    if (plus && !c.plusDown) c.rate = std::min(c.rate * 2.0f, 256.0f);
    if (minus && !c.minusDown) c.rate = std::max(c.rate * 0.5f, 1.0f / 32.0f);
    c.plusDown = plus;
    c.minusDown = minus;
    c.time += (double)dt * c.rate;
    return true;
}

std::vector<CardLine> closeupCard(const AppContext& app) {
    std::vector<CardLine> lines;
    const CloseupState& c = app.cu;
    if (c.mode != 2) return lines;
    const float white[3] = {1.0f, 1.0f, 1.0f}, text[3] = {0.85f, 0.87f, 0.92f}, fact[3] = {0.7f, 0.85f, 1.0f}, hint[3] = {0.6f, 0.65f, 0.75f};
    char buf[96];
    auto add = [&](const std::string& s, const float* col) { lines.push_back({s, {col[0], col[1], col[2]}}); };
    if (c.kind == 0) {
        const NebulaInfo& n = nebulaInfos()[c.obj];
        add(n.name, white);
        add(n.type, n.color);
        std::snprintf(buf, sizeof(buf), "DISTANCE %d LY", (int)n.distLy);
        add(buf, text);
        std::snprintf(buf, sizeof(buf), "SIZE ~%d LY ACROSS", (int)std::lround(n.diameterLy));
        add(buf, text);
        add(n.fact1, fact);
        add(n.fact2, fact);
        add("PROCEDURAL ARTIST VIEW", hint);
    } else if (c.kind == 1) {
        add("SGR A*", white);
        add("SUPERMASSIVE BLACK HOLE", S_STARS[0].color);
        add("MASS 4.15 MILLION SUNS", text);
        add("DISTANCE 26,000 LY", text);
        std::snprintf(buf, sizeof(buf), "HORIZON %.2f AU", horizonAu());
        add(buf, text);
        add("S2 ORBITS IT EVERY 16 YEARS", fact);
        add("AT 2.5% OF LIGHT SPEED", fact);
    } else {
        const Binary& b = binaryAt(c.obj);
        add(b.name, white);
        add(b.type, S_STARS[1].color);
        std::snprintf(buf, sizeof(buf), "DISTANCE %d LY", (int)b.distLy);
        add(buf, text);
        std::snprintf(buf, sizeof(buf), "BLACK HOLE %.1f SUNS", b.mBh);
        add(buf, text);
        std::snprintf(buf, sizeof(buf), "COMPANION %.1f SUNS", b.mStar);
        add(buf, text);
        add(b.fact1, fact);
        add(b.fact2, fact);
        add(b.fact3, fact);
    }
    add("ESC OR BACKSPACE = BACK", hint);
    return lines;
}
