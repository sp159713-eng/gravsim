#include "solarx.h"
#include "ephem.h"
#include "bodydata.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

struct Probe {
    const char* name;
    double raDeg, decDeg, au0, rate;
    const char* launched;
    const char* crossed;
    const char* fact;
    float col[3];
};

const Probe PR[2] = {
    {"VOYAGER 1", 258.25, 12.0, 165.0, 3.57, "1977-09-05", "2012", "THE MOST DISTANT HUMAN-MADE OBJECT. IT CARRIES THE GOLDEN RECORD OF EARTH.", {1.0f, 0.8f, 0.4f}},
    {"VOYAGER 2", 300.0, -60.0, 138.0, 3.2, "1977-08-20", "2018", "THE ONLY PROBE TO VISIT URANUS AND NEPTUNE. STILL SENDING DATA TODAY.", {0.6f, 0.9f, 1.0f}}
};

const double AU0_JD = 2461041.5;
const double HELIOPAUSE_AU = 120.0;
std::string g_label[2];
std::string g_hpLabel = "HELIOPAUSE - INTERSTELLAR SPACE BEGINS";

void direction(int v, float d[3]) {
    double ra = PR[v].raDeg * 3.14159265358979 / 180.0, de = PR[v].decDeg * 3.14159265358979 / 180.0;
    double x = std::cos(de) * std::cos(ra), y = std::cos(de) * std::sin(ra), z = std::sin(de);
    double eps = 23.4392911 * 3.14159265358979 / 180.0;
    double xe = x, ye = y * std::cos(eps) + z * std::sin(eps), ze = -y * std::sin(eps) + z * std::cos(eps);
    d[0] = (float)xe;
    d[1] = (float)-ze;
    d[2] = (float)ye;
}

double distAu(const AppContext& app, int v) {
    return std::max(0.0, PR[v].au0 + PR[v].rate * (solarxJd(app) - AU0_JD) / DAYS_PER_YEAR);
}

}

bool voyagerState(const AppContext& app, int v, float pos[3], double& au) {
    if (!app.displayValid || v < 0 || v > 1) return false;
    float d[3];
    direction(v, d);
    au = distAu(app, v);
    double upa = solarUnitsPerAu(true);
    const Particle& s = app.displayBodies[0];
    for (int a = 0; a < 3; a++) pos[a] = s.pos[a] + d[a] * (float)(au * upa);
    return true;
}

void farLines(AppContext& app, OverlayVertex* out, uint32_t& used, const float vp[16]) {
    if (!app.displayValid) return;
    double upa = solarUnitsPerAu(true);
    const Particle& s = app.displayBodies[0];
    float px = 2.0f / (float)app.swapchainExtent.width, py = 2.0f / (float)app.swapchainExtent.height;
    for (int v = 0; v < 2; v++) {
        float P[3], d[3];
        double au;
        if (!voyagerState(app, v, P, au)) continue;
        direction(v, d);
        float a0[3], x0, y0, x1, y1;
        for (int a = 0; a < 3; a++) a0[a] = s.pos[a] + d[a] * (float)(10.0 * upa);
        const float dim[3] = {PR[v].col[0] * 0.25f, PR[v].col[1] * 0.25f, PR[v].col[2] * 0.25f};
        if (sxProject(vp, a0, x0, y0) && sxProject(vp, P, x1, y1)) sxLine(out, used, x0, y0, x1, y1, dim, dim);
        if (sxProject(vp, P, x1, y1)) {
            const float* c = PR[v].col;
            float r = 5.0f;
            sxLine(out, used, x1 - r * px, y1, x1, y1 - r * py, c, c);
            sxLine(out, used, x1, y1 - r * py, x1 + r * px, y1, c, c);
            sxLine(out, used, x1 + r * px, y1, x1, y1 + r * py, c, c);
            sxLine(out, used, x1, y1 + r * py, x1 - r * px, y1, c, c);
        }
    }
    const float R = (float)(HELIOPAUSE_AU * upa);
    const float hc[3] = {0.12f, 0.32f, 0.5f};
    const int segs = 240;
    for (int ring = 0; ring < 11; ring++) {
        for (int j = 0; j < segs; j += 2) {
            float p0[3], p1[3];
            for (int e = 0; e < 2; e++) {
                float t = 6.2831853f * (float)(j + e) / segs;
                float* p = e ? p1 : p0;
                float ux, uy, uz;
                if (ring < 5) {
                    float lat = (-60.0f + 30.0f * ring) * 3.14159265f / 180.0f;
                    ux = std::cos(lat) * std::cos(t);
                    uy = std::sin(lat);
                    uz = std::cos(lat) * std::sin(t);
                } else {
                    float phi = 3.14159265f * (float)(ring - 5) / 6.0f;
                    ux = std::cos(t) * std::cos(phi);
                    uy = std::sin(t);
                    uz = std::cos(t) * std::sin(phi);
                }
                p[0] = s.pos[0] + ux * R;
                p[1] = s.pos[1] + uy * R;
                p[2] = s.pos[2] + uz * R;
            }
            float x0, y0, x1, y1;
            if (sxProject(vp, p0, x0, y0) && sxProject(vp, p1, x1, y1)) sxLine(out, used, x0, y0, x1, y1, hc, hc);
        }
    }
}

void farLabels(AppContext& app, std::vector<ScreenLabel>& labels, const float vp[16]) {
    if (!app.displayValid) return;
    for (int v = 0; v < 2; v++) {
        float P[3], x, y;
        double au;
        if (!voyagerState(app, v, P, au) || !sxProject(vp, P, x, y)) continue;
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%s - %.0f AU - SIGNAL TAKES %.0f H", PR[v].name, au, au * SX_LIGHT_AU_SEC / 3600.0);
        g_label[v] = buf;
        ScreenLabel l{x, y, g_label[v].c_str(), {PR[v].col[0], PR[v].col[1], PR[v].col[2]}};
        l.prio = 0.4f + 0.01f * (float)v;
        l.oy = -2.0f;
        float widthPx = 12.0f * (float)g_label[v].size() + 24.0f;
        float pxN = 2.0f / (float)app.swapchainExtent.width;
        if (x + widthPx * pxN > 0.96f) l.x = x - widthPx * pxN;
        labels.push_back(l);
    }
    const Particle& s = app.displayBodies[0];
    float R = (float)(HELIOPAUSE_AU * solarUnitsPerAu(true));
    float eye[3];
    cameraEye(app.camera, eye);
    float de[3] = {eye[0] - s.pos[0], eye[1] - s.pos[1], eye[2] - s.pos[2]};
    if (std::sqrt(de[0] * de[0] + de[1] * de[1] + de[2] * de[2]) < 20.0f) return;
    float bestW = 1e30f, bx = 0, by = 0;
    bool have = false;
    for (int j = 0; j < 48; j++) {
        float t = 6.2831853f * (float)j / 48.0f;
        float p[3] = {s.pos[0] + R * std::cos(t) * 0.94f, s.pos[1] + R * 0.34f, s.pos[2] + R * std::sin(t) * 0.94f};
        float x, y;
        if (!sxProject(vp, p, x, y) || std::fabs(x) > 0.55f || std::fabs(y) > 0.8f) continue;
        float w = vp[3] * p[0] + vp[7] * p[1] + vp[11] * p[2] + vp[15];
        if (w < bestW) {
            bestW = w;
            bx = x;
            by = y;
            have = true;
        }
    }
    if (have) {
        ScreenLabel l{bx, by, g_hpLabel.c_str(), {0.45f, 0.8f, 1.0f}};
        l.prio = 0.3f;
        labels.push_back(l);
    }
}

std::vector<CardLine> voyagerCard(const AppContext& app, int v) {
    std::vector<CardLine> lines;
    const float white[3] = {1.0f, 1.0f, 1.0f}, text[3] = {0.85f, 0.87f, 0.92f}, green[3] = {0.4f, 1.0f, 0.55f};
    const float fact[3] = {0.55f, 0.8f, 1.0f}, hint[3] = {0.6f, 0.65f, 0.75f};
    float P[3];
    double au = 0.0;
    voyagerState(app, v, P, au);
    char buf[96];
    lines.push_back({PR[v].name, {white[0], white[1], white[2]}});
    lines.push_back({"SPACE PROBE", {PR[v].col[0], PR[v].col[1], PR[v].col[2]}});
    lines.push_back({std::string("LAUNCHED ") + PR[v].launched, {text[0], text[1], text[2]}});
    lines.push_back({std::string("LEFT THE HELIOSPHERE ") + PR[v].crossed, {text[0], text[1], text[2]}});
    std::snprintf(buf, sizeof(buf), "SUN DISTANCE %.1f AU", au);
    lines.push_back({buf, {green[0], green[1], green[2]}});
    std::snprintf(buf, sizeof(buf), "SPEED %.1f KM/S", PR[v].rate * KM_PER_AU / (DAYS_PER_YEAR * 86400.0));
    lines.push_back({buf, {green[0], green[1], green[2]}});
    std::snprintf(buf, sizeof(buf), "SIGNAL TAKES %.1f H", au * SX_LIGHT_AU_SEC / 3600.0);
    lines.push_back({buf, {green[0], green[1], green[2]}});
    std::string f = std::string("FACT: ") + PR[v].fact, cur;
    size_t pos = 0;
    while (pos < f.size()) {
        size_t sp = f.find(' ', pos);
        std::string word = f.substr(pos, sp == std::string::npos ? std::string::npos : sp - pos);
        pos = sp == std::string::npos ? f.size() : sp + 1;
        if (!cur.empty() && cur.size() + 1 + word.size() > 32) {
            lines.push_back({cur, {fact[0], fact[1], fact[2]}});
            cur.clear();
        }
        cur += (cur.empty() ? "" : " ") + word;
    }
    if (!cur.empty()) lines.push_back({cur, {fact[0], fact[1], fact[2]}});
    lines.push_back({"ESC = FREE-FLY", {hint[0], hint[1], hint[2]}});
    return lines;
}
