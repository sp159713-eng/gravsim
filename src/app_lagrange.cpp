#include "vk_app.h"
#include <algorithm>
#include <cmath>

bool g_lgJupiter = false;
bool g_lgDemo = false;

void lgSetMode(AppContext& app, bool jupiter) {
    LagrangeState& S = app.lg;
    S.jupiter = jupiter;
    S.mu = jupiter ? lg::MU_JUPITER : lg::MU_EARTH;
    S.t = 0.0;
    S.acc = 0.0;
    S.hover = S.selected = -1;
    S.rng = 12345u;
    lg::solvePoints(S.mu, S.pt);
    lg::buildContours(S);
    lg::populate(S);
}

void lgDemoDrop(AppContext& app) {
    LagrangeState& S = app.lg;
    lg::dropTest(S, S.pt[3][0] + 0.045, S.pt[3][1] + 0.03);
    lg::dropTest(S, S.pt[1][0] + 0.008, 0.004);
    lg::dropTest(S, S.pt[0][0] - 0.004, -0.003);
    S.selected = 1;
}

void lgEnter(AppContext& app) {
    LagrangeState& S = app.lg;
    S.ready = true;
    lgSetMode(app, g_lgJupiter);
    Camera& c = app.camera;
    c = Camera{};
    c.free = false;
    c.pitch = 0.2f;
    c.dist = 11.7f;
    c.minDist = 1.0f;
    c.nearPlane = 0.1f;
    if (g_lgDemo) lgDemoDrop(app);
    g_lgJupiter = false;
    g_lgDemo = false;
}

void lgRestart(AppContext& app) {
    lgSetMode(app, app.lg.jupiter);
}

void lgUpdate(AppContext& app, float dt, bool mouseNow, bool pressUi) {
    LagrangeState& S = app.lg;
    app.camera.zoomImpulse = 0.0f;
    app.camera.dist = 11.7f;
    dt = std::min(dt, 0.05f);
    GLFWwindow* w = app.window;
    bool tab = glfwGetKey(w, GLFW_KEY_TAB) == GLFW_PRESS;
    if (tab && !S.tabPrev) lgSetMode(app, !S.jupiter);
    S.tabPrev = tab;
    bool clr = glfwGetKey(w, GLFW_KEY_C) == GLFW_PRESS;
    if (clr && !S.clearPrev) S.tests.clear();
    S.clearPrev = clr;
    if (!S.paused) lg::advance(S, (double)dt * (double)S.speed * lg::RATE);

    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    LgView v = lgView(W, H);
    double cx = app.cursorFb[0], cy = app.cursorFb[1];
    S.hover = -1;
    if (app.cursorOk) {
        float best = 20.0f * v.k;
        for (int i = 0; i < 5; i++) {
            float px, py;
            v.to(S.pt[i][0], S.pt[i][1], px, py);
            float d = std::hypot(px - (float)cx, py - (float)cy);
            if (d < best) {
                best = d;
                S.hover = i;
            }
        }
    }
    if (mouseNow && !S.leftPrev && !pressUi && app.cursorOk && !app.showHelp) {
        LgRect r = lgInsetRect(W, H);
        if (!(!S.jupiter && r.has(cx, cy))) {
            double wx, wy;
            v.from(cx, cy, wx, wy);
            if (std::fabs(wx) <= lg::GX && std::fabs(wy) <= lg::GY) {
                lg::dropTest(S, wx, wy);
                S.selected = S.hover;
            }
        }
    }
    S.leftPrev = mouseNow;
}

void drawLagrange(AppContext& app, VkCommandBuffer cmd, float aspect) {
    float vp[16];
    cameraViewProj(app.camera, aspect, vp);
    if (app.showSky) drawSky(app, cmd, vp);
}
