#include "belts.h"
#include "ephem.h"
#include "bodydata.h"
#include "solarx.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

const double PI_D = 3.14159265358979323846;
const float WHITE[3] = {1.0f, 1.0f, 1.0f}, TEXT[3] = {0.85f, 0.87f, 0.92f}, GREEN[3] = {0.4f, 1.0f, 0.55f};
const float FACT[3] = {0.55f, 0.8f, 1.0f}, HINT[3] = {0.6f, 0.65f, 0.75f};

struct Group {
    std::string text;
};

Group g_groups[12];

double upa() {
    return solarUnitsPerAu(true);
}

void sunOf(const AppContext& app, float s[3]) {
    for (int a = 0; a < 3; a++) s[a] = app.displayBodies[0].pos[a];
}

double eyeSunAu(const AppContext& app) {
    float e[3], s[3];
    cameraEye(app.camera, e);
    sunOf(app, s);
    return std::sqrt(std::pow(e[0] - s[0], 2) + std::pow(e[1] - s[1], 2) + std::pow(e[2] - s[2], 2)) / upa();
}

double bodyAngle(const AppContext& app, int i) {
    const Particle& p = app.displayBodies[i];
    const Particle& s = app.displayBodies[0];
    return std::atan2((double)p.pos[2] - s.pos[2], (double)p.pos[0] - s.pos[0]);
}

void ringPoint(const float s[3], double angle, double rAu, float out[3]) {
    out[0] = s[0] + (float)(std::cos(angle) * rAu * upa());
    out[1] = s[1];
    out[2] = s[2] + (float)(std::sin(angle) * rAu * upa());
}

bool bestOnRing(const float s[3], double rAu, const float vp[16], float x0, float x1, bool top, float& ox, float& oy) {
    bool have = false;
    float best = top ? -9.0f : 9.0f;
    for (int j = 0; j < 180; j++) {
        float p[3], x, y;
        ringPoint(s, 2.0 * PI_D * j / 180.0, rAu, p);
        if (!sxProject(vp, p, x, y) || x < x0 || x > x1 || std::fabs(y) > 0.8f) continue;
        if (top ? y > best : y < best) {
            best = y;
            ox = x;
            oy = y;
            have = true;
        }
    }
    return have;
}

void pushLabel(std::vector<ScreenLabel>& labels, int slot, const char* text, float x, float y, const float col[3], float prio) {
    g_groups[slot].text = text;
    ScreenLabel l{x, y, g_groups[slot].text.c_str(), {col[0], col[1], col[2]}};
    l.prio = prio;
    labels.push_back(l);
}

void wrapText(std::vector<CardLine>& lines, const std::string& text, size_t width, const float c[3]) {
    std::string cur;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t sp = text.find(' ', pos);
        std::string word = text.substr(pos, sp == std::string::npos ? std::string::npos : sp - pos);
        pos = sp == std::string::npos ? text.size() : sp + 1;
        if (!cur.empty() && cur.size() + 1 + word.size() > width) {
            lines.push_back({cur, {c[0], c[1], c[2]}});
            cur.clear();
        }
        cur += (cur.empty() ? "" : " ") + word;
    }
    if (!cur.empty()) lines.push_back({cur, {c[0], c[1], c[2]}});
}

double beltDistance(const AppContext& app) {
    float e[3], s[3];
    cameraEye(app.camera, e);
    sunOf(app, s);
    double u = upa();
    double r = std::hypot((e[0] - s[0]) / u, (e[2] - s[2]) / u), y = (e[1] - s[1]) / u;
    double dr = r < 2.1 ? 2.1 - r : (r > 3.3 ? r - 3.3 : 0.0);
    return std::hypot(dr, y);
}

}

bool beltsObjPos(const AppContext& app, int j, float pos[3]) {
    if (!beltsActive(app) || j < 0 || j >= BELT_NAMED) return false;
    const BeltNamed& n = beltNamed(j);
    double t = solarxJd(app) - 2451545.0;
    double nr = 0.01720209895 / std::pow(n.a, 1.5);
    double M = n.m0 * PI_D / 180.0 + nr * t;
    OrbitState o = ephemKeplerM(n.a, n.e, n.inc, n.node, n.arg, M, 1.0);
    float s[3];
    sunOf(app, s);
    for (int a = 0; a < 3; a++) pos[a] = s[a] + (float)(o.pos[a] * upa());
    return true;
}

bool beltsLabelShown(const AppContext& app, int j) {
    float p[3], e[3];
    if (!beltsObjPos(app, j, p)) return false;
    const BeltNamed& n = beltNamed(j);
    cameraEye(app.camera, e);
    double d = std::sqrt(std::pow(e[0] - p[0], 2) + std::pow(e[1] - p[1], 2) + std::pow(e[2] - p[2], 2)) / upa();
    if (!n.far) return d < n.labelAu;
    return (eyeSunAu(app) > 30.0 || d < 30.0) && d < n.labelAu;
}

void beltsLabels(AppContext& app, std::vector<ScreenLabel>& labels, const float vp[16]) {
    if (!beltsActive(app)) return;
    for (int j = 0; j < BELT_NAMED; j++) {
        float p[3], x, y;
        if (!beltsLabelShown(app, j) || !beltsObjPos(app, j, p) || !sxProject(vp, p, x, y)) continue;
        const BeltNamed& n = beltNamed(j);
        ScreenLabel l{x, y, n.name, {n.col[0], n.col[1], n.col[2]}};
        l.prio = 0.45f + 0.01f * (float)j;
        l.oy = -8.0f;
        labels.push_back(l);
    }
    float s[3], x, y;
    sunOf(app, s);
    double d = eyeSunAu(app);
    const float amber[3] = {1.0f, 0.8f, 0.5f}, red[3] = {1.0f, 0.62f, 0.5f}, blue[3] = {0.6f, 0.8f, 1.0f}, grey[3] = {0.75f, 0.78f, 0.85f};
    if (d > 2.5 && d < 30.0) {
        const char* names[4] = {"3:1 KIRKWOOD GAP - 2.50 AU", "5:2 GAP - 2.82 AU", "7:3 GAP - 2.95 AU", "2:1 GAP - 3.27 AU"};
        float x0[4] = {-0.92f, -0.46f, 0.0f, 0.46f};
        for (int k = 0; k < 4; k++)
            if (bestOnRing(s, beltsRingAu(k), vp, x0[k], x0[k] + 0.46f, true, x, y)) pushLabel(labels, k, names[k], x, y, grey, 0.2f);
        if (bestOnRing(s, beltsRingAu(5), vp, -0.7f, 0.7f, false, x, y)) pushLabel(labels, 4, "ASTEROID BELT", x, y, amber, 0.3f);
    }
    if (d > 3.0 && d < 60.0) {
        double th = bodyAngle(app, 5);
        float p[3];
        ringPoint(s, th + PI_D / 3.0, 5.2, p);
        if (sxProject(vp, p, x, y)) pushLabel(labels, 5, "JUPITER TROJANS L4", x, y, red, 0.25f);
        ringPoint(s, th - PI_D / 3.0, 5.2, p);
        if (sxProject(vp, p, x, y)) pushLabel(labels, 6, "TROJANS L5", x, y, red, 0.25f);
        ringPoint(s, th + PI_D, 4.4, p);
        if (sxProject(vp, p, x, y)) pushLabel(labels, 7, "HILDAS", x, y, red, 0.25f);
    }
    if (d > 20.0 && d < 1200.0) {
        if (bestOnRing(s, 45.0, vp, -0.7f, 0.7f, false, x, y)) pushLabel(labels, 8, "KUIPER BELT", x, y, blue, 0.3f);
        double th = bodyAngle(app, 8);
        float p[3];
        ringPoint(s, th + PI_D / 2.0, 36.0, p);
        if (sxProject(vp, p, x, y)) pushLabel(labels, 9, "PLUTINOS", x, y, blue, 0.25f);
    }
}

void beltsText(AppContext& app, OverlayVertex* out, uint32_t& used) {
    if (!beltsActive(app) || beltDistance(app) > 1.2) return;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    const char* l1 = "THE ASTEROID BELT IS MOSTLY EMPTY - AVERAGE GAP BETWEEN ASTEROIDS IS ~1 MILLION KM;";
    const char* l2 = "ALL TOGETHER THEY WEIGH LESS THAN 4% OF THE MOON";
    float pxs = 2.0f / W, pys = 2.0f / H;
    int s1 = std::min(fitScale(l1, W, 2), fitScale(l2, W, 2));
    float y = H - 88.0f;
    bigText(out, used, l1, W, y, pxs, pys, s1, 1.0f, 0.85f, 0.5f);
    bigText(out, used, l2, W, y + 7.0f * s1 + 8.0f, pxs, pys, s1, 1.0f, 0.85f, 0.5f);
}

std::vector<CardLine> beltsCard(const AppContext& app, int j) {
    std::vector<CardLine> lines;
    if (j < 0 || j >= BELT_NAMED) return lines;
    const BeltNamed& n = beltNamed(j);
    char buf[96];
    lines.push_back({n.name, {WHITE[0], WHITE[1], WHITE[2]}});
    lines.push_back({n.kind, {n.col[0], n.col[1], n.col[2]}});
    lines.push_back({n.disc, {TEXT[0], TEXT[1], TEXT[2]}});
    std::snprintf(buf, sizeof(buf), "DIAMETER %.0f KM", n.diameterKm);
    lines.push_back({buf, {TEXT[0], TEXT[1], TEXT[2]}});
    double years = std::pow(n.a, 1.5);
    std::snprintf(buf, sizeof(buf), years < 100.0 ? "YEAR LASTS %.2f EARTH YEARS" : "YEAR LASTS %.0f EARTH YEARS", years);
    lines.push_back({buf, {TEXT[0], TEXT[1], TEXT[2]}});
    float p[3], s[3];
    if (beltsObjPos(app, j, p)) {
        sunOf(app, s);
        double r = std::sqrt(std::pow(p[0] - s[0], 2) + std::pow(p[1] - s[1], 2) + std::pow(p[2] - s[2], 2)) / upa();
        std::snprintf(buf, sizeof(buf), "SUN DISTANCE %.2f AU", r);
        lines.push_back({buf, {GREEN[0], GREEN[1], GREEN[2]}});
        std::snprintf(buf, sizeof(buf), "SUNLIGHT TAKES %.1f MIN", r * SX_LIGHT_AU_SEC / 60.0);
        lines.push_back({buf, {GREEN[0], GREEN[1], GREEN[2]}});
    }
    wrapText(lines, std::string("FACT: ") + n.fact, 30, FACT);
    lines.push_back({"ESC = FREE-FLY", {HINT[0], HINT[1], HINT[2]}});
    return lines;
}
