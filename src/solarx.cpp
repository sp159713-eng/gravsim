#include "solarx.h"
#include "ephem.h"
#include "bodydata.h"
#include "belts.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

SolarX g_sx;
std::string g_sxDate;
bool g_sxNosunDemo = false;
std::string g_sxCamTo;
float g_sxCam[3] = {0.0f, 0.0f, 0.0f};
bool g_sxCamSet = false;
int g_sxRate = -1;

namespace {

long long daysFromCivil(long long y, int m, int d) {
    y -= m <= 2 ? 1 : 0;
    long long era = (y >= 0 ? y : y - 399) / 400;
    long long yoe = y - era * 400;
    long long doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

void civilFromDays(long long z, long long& y, int& m, int& d) {
    z += 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    long long doe = z - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = yoe + era * 400;
    long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long long mp = (5 * doy + 2) / 153;
    d = (int)(doy - (153 * mp + 2) / 5 + 1);
    m = (int)(mp < 10 ? mp + 3 : mp - 9);
    y += m <= 2 ? 1 : 0;
}

bool g_flagsDone = false;
int g_frames = 0;
bool g_nDown = false, g_escDown = false;
int g_demoWait = 0;

float earthLightSec(const AppContext& app) {
    if (!app.displayValid) return (float)SX_LIGHT_AU_SEC;
    const Particle& s = app.displayBodies[0];
    const Particle& e = app.displayBodies[3];
    double r = 0.0;
    for (int a = 0; a < 3; a++) r += ((double)e.pos[a] - s.pos[a]) * ((double)e.pos[a] - s.pos[a]);
    return (float)(std::sqrt(r) / solarUnitsPerAu(app.trueScale) * SX_LIGHT_AU_SEC);
}

void toggleSun(AppContext& app) {
    if (!g_sx.gone) {
        cometsSnapshot(app);
        g_sx.gone = true;
        g_sx.t0 = app.simTime;
        g_sx.snapT = app.simTime;
        g_sx.t0Pending = true;
        g_sx.zeroPending = true;
        return;
    }
    long long days = app.tm.launchDay + (long long)std::floor(app.simTime / SX_YEAR_UNITS * DAYS_PER_YEAR);
    app.tm.launchDay = days;
    app.tm.needReset = true;
    g_sx.gone = false;
}

void selectObj(AppContext& app, int k) {
    float p[3];
    if (!solarxObjPos(app, k, p)) return;
    float eye[3];
    cameraEye(app.camera, eye);
    float d[3] = {eye[0] - p[0], eye[1] - p[1], eye[2] - p[2]};
    float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len < 1e-6f) {
        d[2] = 1.0f;
        len = 1.0f;
    }
    Camera& c = app.camera;
    c.yaw = std::atan2(d[0], d[2]);
    c.pitch = std::max(-1.5f, std::min(1.5f, std::asin(std::max(-1.0f, std::min(1.0f, d[1] / len)))));
    for (int a = 0; a < 3; a++) c.target[a] = p[a];
    c.minDist = k >= BELT_SEL_BASE ? 0.002f : 0.3f;
    c.dist = std::min(len, k >= BELT_SEL_BASE ? (beltNamed(k - BELT_SEL_BASE).far ? 25.0f : 0.5f) : (k >= 2 ? 40.0f : 8.0f));
    c.free = false;
    app.follow = -1;
    g_sx.sel = k;
}

int beltCamTo(const std::string& name) {
    for (int j = 0; j < BELT_NAMED; j++) {
        std::string n = beltNamed(j).name;
        for (char& ch : n) ch = (char)std::tolower((unsigned char)ch);
        if (n == name) return j;
    }
    return -1;
}

void applyCamFlags(AppContext& app) {
    float p[3];
    if (g_sxCamTo == "halley" || g_sxCamTo == "sun") {
        int b = g_sxCamTo == "sun" ? 0 : 14;
        enterFollow(app, b, app.displayBodies[b].pos);
    } else if (g_sxCamTo == "hb") {
        selectObj(app, 0);
    } else if (g_sxCamTo == "encke") {
        selectObj(app, 1);
    } else if (g_sxCamTo == "v1") {
        selectObj(app, 2);
    } else if (g_sxCamTo == "v2") {
        selectObj(app, 3);
    } else if (beltCamTo(g_sxCamTo) >= 0) {
        selectObj(app, BELT_SEL_BASE + beltCamTo(g_sxCamTo));
    } else if (g_sxCamSet) {
        p[0] = p[1] = p[2] = 0.0f;
        for (int a = 0; a < 3; a++) app.camera.target[a] = p[a];
        app.camera.free = false;
        app.follow = -1;
    }
    if (g_sxCamSet) {
        app.camera.yaw = g_sxCam[0];
        app.camera.pitch = g_sxCam[1];
        app.camera.dist = g_sxCam[2];
    }
}

}

bool solarxHome(const AppContext& app) {
    return app.scene == Scene::SOLAR && !app.visit.active && !app.gal.active && app.tour.mode == 0 && app.visit.mode == 0 && app.displayValid;
}

std::string solarxDate(double jd) {
    long long y;
    int m, d;
    civilFromDays((long long)std::floor(jd - 2440587.5), y, m, d);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04lld-%02d-%02d", y, m, d);
    return buf;
}

double solarxJd(const AppContext& app) {
    return solarEpochJd() + app.simTime / SX_YEAR_UNITS * DAYS_PER_YEAR;
}

double solarxElapsed(const AppContext& app) {
    return g_sx.gone ? (app.simTime - g_sx.t0) / SX_YEAR_UNITS * DAYS_PER_YEAR * 86400.0 : -1.0;
}

double solarxUnits(const AppContext& app, double units, float frameDt) {
    if (!g_sx.gone || !solarxHome(app)) return units;
    if (g_sx.t0Pending) {
        g_sx.t0 = app.simTime;
        g_sx.t0Pending = false;
    }
    if (solarxElapsed(app) >= earthLightSec(app)) return units;
    return 60.0 * frameDt / 86400.0 / DAYS_PER_YEAR * SX_YEAR_UNITS;
}

float solarxPlanetSunT(const AppContext& app) {
    return g_sx.gone ? (float)solarxElapsed(app) + 1.0f : 0.0f;
}

void solarxCamNow(AppContext& app, const char* to, bool setCam, float yaw, float pitch, float dist) {
    std::string keepTo = g_sxCamTo;
    bool keepSet = g_sxCamSet;
    float keep[3] = {g_sxCam[0], g_sxCam[1], g_sxCam[2]};
    g_sxCamTo = to ? to : "";
    g_sxCamSet = setCam;
    g_sxCam[0] = yaw;
    g_sxCam[1] = pitch;
    g_sxCam[2] = dist;
    applyCamFlags(app);
    g_sxCamTo = keepTo;
    g_sxCamSet = keepSet;
    for (int a = 0; a < 3; a++) g_sxCam[a] = keep[a];
}

void solarxReset(AppContext& app) {
    (void)app;
    g_sx.gone = false;
    g_sx.zeroPending = false;
}

void solarxApplyFlags(AppContext& app) {
    if (g_sxRate >= 0) app.tm.rate = std::min(g_sxRate, 13);
    if (g_sxDate.empty()) return;
    int y = 0, m = 1, d = 1;
    std::sscanf(g_sxDate.c_str(), "%d-%d-%d", &y, &m, &d);
    app.tm.launchDay = daysFromCivil(y, m, d);
    solarSetEpochDay(app.tm.launchDay);
}

void solarxStep(AppContext& app, float dt) {
    (void)dt;
    g_frames++;
    beltsStep(app);
    if (!g_flagsDone && app.displayValid && g_frames > 8) {
        g_flagsDone = true;
        if (!g_sxCamTo.empty() || g_sxCamSet) applyCamFlags(app);
    }
    if (g_sxNosunDemo && g_flagsDone && !g_sx.gone && solarxHome(app) && g_demoWait++ > 25) {
        g_sxNosunDemo = false;
        toggleSun(app);
    }
    bool typing = timeTyping();
    bool nKey = !typing && glfwGetKey(app.window, GLFW_KEY_N) == GLFW_PRESS;
    if (nKey && !g_nDown && solarxHome(app) && app.tm.needReset == false) toggleSun(app);
    g_nDown = nKey;
    bool escKey = !typing && glfwGetKey(app.window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
    if (escKey && !g_escDown && g_sx.sel >= 0 && !app.showHelp) {
        enterFree(app);
        g_sx.sel = -1;
    }
    g_escDown = escKey;
    if (g_sx.sel >= 0) {
        float p[3];
        if (app.follow >= 0 || app.camera.free || !solarxHome(app) || !app.trueScale || !solarxObjPos(app, g_sx.sel, p)) {
            g_sx.sel = -1;
        } else {
            for (int a = 0; a < 3; a++) app.camera.target[a] = p[a];
        }
    }
}

void solarxRecord(AppContext& app, VkCommandBuffer cmd) {
    if (!g_sx.zeroPending) return;
    g_sx.zeroPending = false;
    float zero = 0.0f;
    for (int i = 0; i < 2; i++) vkCmdUpdateBuffer(cmd, app.particleBuffers[i], 12, 4, &zero);
}

bool solarxObjPos(const AppContext& app, int k, float pos[3]) {
    if (!app.displayValid) return false;
    float vel[3];
    if (k >= BELT_SEL_BASE) return beltsObjPos(app, k - BELT_SEL_BASE, pos);
    if (k < 2) return cometState(app, k + 1, pos, vel);
    double au;
    return voyagerState(app, k - 2, pos, au);
}

void solarxLines(AppContext& app, OverlayVertex* out, uint32_t& used, const float vp[16]) {
    cometsLines(app, out, used, vp, solarxHome(app));
    if (solarxHome(app) && app.trueScale) farLines(app, out, used, vp);
}

void solarxLabels(AppContext& app, std::vector<ScreenLabel>& labels, const float vp[16]) {
    if (!solarxHome(app) || !app.trueScale) return;
    cometsLabels(app, labels, vp);
    farLabels(app, labels, vp);
    beltsLabels(app, labels, vp);
}

bool solarxClick(AppContext& app, double fx, double fy) {
    if (!solarxHome(app) || !app.trueScale) return false;
    float aspect = (float)app.swapchainExtent.width / (float)app.swapchainExtent.height;
    float vp[16];
    cameraViewProj(app.camera, aspect, vp);
    int best = -1;
    double bestD = 18.0;
    for (int k = 0; k < BELT_SEL_BASE + BELT_NAMED; k++) {
        float p[3], x, y;
        if (k >= BELT_SEL_BASE && (!beltNamed(k - BELT_SEL_BASE).card || !beltsLabelShown(app, k - BELT_SEL_BASE))) continue;
        if (!solarxObjPos(app, k, p) || !sxProject(vp, p, x, y)) continue;
        double sx = (x + 1.0) * 0.5 * app.swapchainExtent.width, sy = (y + 1.0) * 0.5 * app.swapchainExtent.height;
        double d = std::hypot(sx - fx, sy - fy);
        if (d < bestD) {
            bestD = d;
            best = k;
        }
    }
    if (best < 0) return false;
    selectObj(app, best);
    return true;
}

std::vector<CardLine> solarxCard(const AppContext& app) {
    if (g_sx.sel < 0 || !solarxHome(app)) return {};
    if (g_sx.sel >= BELT_SEL_BASE) return beltsCard(app, g_sx.sel - BELT_SEL_BASE);
    if (g_sx.sel < 2) return cometCard(app, g_sx.sel + 1);
    return voyagerCard(app, g_sx.sel - 2);
}

void solarxHalleyCard(const AppContext& app, std::vector<CardLine>& lines) {
    std::string period, next;
    cometPeriodLines(app, 0, period, next);
    lines.push_back({"PERIOD " + period, {0.85f, 0.87f, 0.92f}});
    lines.push_back({"NEXT PERIHELION " + next, {0.4f, 1.0f, 0.55f}});
    lines.push_back({"TAILS: BLUE ION + YELLOW DUST", {0.55f, 0.8f, 1.0f}});
}

void solarxText(AppContext& app, OverlayVertex* out, uint32_t& used, float px, float py) {
    (void)px;
    (void)py;
    beltsText(app, out, used);
    if (!g_sx.gone || !solarxHome(app)) return;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    const char* l1 = "WITHOUT THE SUN'S GRAVITY EVERY PLANET FLIES OFF IN A STRAIGHT LINE";
    const char* l2 = "EARTH WOULD STILL SEE SUNLIGHT FOR 8 MORE MINUTES";
    float pxs = 2.0f / W, pys = 2.0f / H;
    int s1 = fitScale(l1, W, 2);
    float y = H - 118.0f;
    bigText(out, used, l1, W, y, pxs, pys, s1, 1.0f, 0.85f, 0.45f);
    y += 7.0f * s1 + 8.0f;
    bigText(out, used, l2, W, y, pxs, pys, s1, 1.0f, 0.85f, 0.45f);
    y += 7.0f * s1 + 12.0f;
    double remain = earthLightSec(app) - solarxElapsed(app);
    char buf[96];
    if (remain > 0.0) {
        int sec = (int)std::ceil(remain);
        std::snprintf(buf, sizeof(buf), "LIGHT STILL ARRIVING: %d:%02d", sec / 60, sec % 60);
        bigText(out, used, buf, W, y, pxs, pys, 3, 1.0f, 0.95f, 0.6f);
    } else {
        bigText(out, used, "LIGHT HAS STOPPED ARRIVING - EARTH IS NOW IN THE DARK", W, y, pxs, pys, fitScale("LIGHT HAS STOPPED ARRIVING - EARTH IS NOW IN THE DARK", W, 2), 0.6f, 0.75f, 1.0f);
    }
    y += 30.0f;
    bigText(out, used, "N = RESTORE THE SUN", W, y, pxs, pys, 1, 0.6f, 0.65f, 0.75f);
}
