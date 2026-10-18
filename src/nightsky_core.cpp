#include "nightsky.h"
#include "realstars.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>

namespace {

const float PI = 3.14159265f;
const float LY_PER_PC = 3.26156f;
const float PLACE_LAT[NS_PLACES] = {28.6f, 0.0f, 51.5f, -33.9f};
const char* const PLACE_NAME[NS_PLACES] = {"NEW DELHI 28.6 N", "THE EQUATOR 0.0", "LONDON 51.5 N", "SYDNEY 33.9 S"};
const char* const VIEW_STAR[NS_VIEWS] = {"", "PROXIMA CENTAURI", "SIRIUS", "VEGA", "BETELGEUSE"};

float dot3(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

void cross3(const float* a, const float* b, float* o) {
    o[0] = a[1] * b[2] - a[2] * b[1];
    o[1] = a[2] * b[0] - a[0] * b[2];
    o[2] = a[0] * b[1] - a[1] * b[0];
}

void norm3(float* v) {
    float l = std::sqrt(dot3(v, v));
    if (l < 1e-9f) return;
    v[0] /= l; v[1] /= l; v[2] /= l;
}

void eqVec(float raH, float decDeg, float* o) {
    float ra = raH * PI / 12.0f, de = decDeg * PI / 180.0f;
    o[0] = std::cos(de) * std::cos(ra);
    o[1] = std::cos(de) * std::sin(ra);
    o[2] = std::sin(de);
}

void bvColour(float bv, float* rgb) {
    bv = std::max(-0.4f, std::min(2.0f, bv));
    float T = 4600.0f * (1.0f / (0.92f * bv + 1.7f) + 1.0f / (0.92f * bv + 0.62f));
    float t = T / 100.0f;
    float r = t <= 66.0f ? 255.0f : 329.7f * std::pow(t - 60.0f, -0.1332f);
    float g = t <= 66.0f ? 99.47f * std::log(t) - 161.12f : 288.12f * std::pow(t - 60.0f, -0.0755f);
    float b = t >= 66.0f ? 255.0f : (t <= 19.0f ? 0.0f : 138.52f * std::log(t - 10.0f) - 305.04f);
    float c[3] = {std::max(0.0f, std::min(255.0f, r)) / 255.0f, std::max(0.0f, std::min(255.0f, g)) / 255.0f, std::max(0.0f, std::min(255.0f, b)) / 255.0f};
    for (int i = 0; i < 3; i++) rgb[i] = 0.62f * c[i] + 0.38f;
}

void addStar(NightSkyState& S, const NsStar& s) {
    NsCatStar c{};
    c.name = s.name;
    float raH = s.raH, dec = s.decDeg, dist = s.distLy;
    std::string n = s.name, nA = n + " A";
    for (int i = 0; i < realStarCount(); i++) {
        const RealStar& r = realStarAt(i);
        if (n == r.name || nA == r.name) {
            raH = r.raH; dec = r.decDeg; dist = r.distLy;
            break;
        }
    }
    float d[3];
    eqVec(raH, dec, d);
    for (int i = 0; i < 3; i++) c.pos[i] = d[i] * dist;
    c.mag = s.mag;
    c.dist = dist;
    bvColour(s.bv, c.rgb);
    S.cat.push_back(c);
}

}

float nsHillAlt(float az) {
    return 0.035f + 0.020f * std::sin(3.0f * az + 1.0f) + 0.012f * std::sin(7.0f * az + 2.0f) + 0.006f * std::sin(17.0f * az + 0.5f);
}

int nsYear() {
    std::time_t t = std::time(nullptr);
    std::tm* lt = std::localtime(&t);
    return lt ? lt->tm_year + 1900 : 2026;
}

const char* nsPlaceName(int i) { return PLACE_NAME[i % NS_PLACES]; }

const char* nsViewName(int i) {
    if (i == 0) return "EARTH";
    return VIEW_STAR[i % NS_VIEWS];
}

int nsFindStar(const NightSkyState& S, const char* name) {
    std::string q = name;
    for (char& ch : q) ch = (char)std::toupper((unsigned char)ch);
    for (size_t i = 0; i < S.cat.size(); i++)
        if (S.cat[i].name == q) return (int)i;
    for (size_t i = 0; i < S.cat.size(); i++)
        if (S.cat[i].name.compare(0, q.size(), q) == 0) return (int)i;
    return -1;
}

void nsBuildCatalogue(NightSkyState& S) {
    S.cat.clear();
    S.groups.clear();
    for (int i = 0; i < NS_STARS_A_N; i++) addStar(S, NS_STARS_A[i]);
    for (int i = 0; i < NS_STARS_B_N; i++) addStar(S, NS_STARS_B[i]);
    for (int v = 1; v < NS_VIEWS; v++) S.viewStar[v] = nsFindStar(S, VIEW_STAR[v]);
    S.viewStar[0] = -1;
    for (int c = 0; c < NS_CONSTS_N; c++) {
        NsGroup g;
        g.name = NS_CONSTS[c].name;
        const char* const* p = NS_CONSTS[c].chain;
        int prev = -1;
        for (;; p++) {
            if (p[0] == nullptr) {
                prev = -1;
                if (p[1] == nullptr) break;
                continue;
            }
            int id = nsFindStar(S, p[0]);
            if (id < 0) continue;
            if (std::find(g.stars.begin(), g.stars.end(), id) == g.stars.end()) g.stars.push_back(id);
            if (prev >= 0) g.links.push_back({prev, id});
            prev = id;
        }
        S.groups.push_back(g);
    }
}

const float* nsViewPos(const NightSkyState& S, int v) {
    static const float zero[3] = {0, 0, 0};
    if (v <= 0 || S.viewStar[v] < 0) return zero;
    return S.cat[S.viewStar[v]].pos;
}

void nsSetLook(NightSkyState& S, float raH, float decDeg) {
    eqVec(raH, decDeg, S.L);
}

void nsSetView(NightSkyState& S, int v) {
    S.view = S.fromView = S.toView = v;
    S.moveT = 1.0f;
    const float* p = nsViewPos(S, v);
    for (int i = 0; i < 3; i++) S.obs[i] = S.fromPos[i] = p[i];
    S.earthBlend = v == 0 ? 1.0f : 0.0f;
}

void nsFrame(NightSkyState& S) {
    float lat = S.lat * PI / 180.0f, lst = S.lst * PI / 12.0f;
    S.Z[0] = std::cos(lat) * std::cos(lst);
    S.Z[1] = std::cos(lat) * std::sin(lst);
    S.Z[2] = std::sin(lat);
    const float pole[3] = {0, 0, 1};
    cross3(pole, S.Z, S.E);
    norm3(S.E);
    cross3(S.Z, S.E, S.N);
    norm3(S.N);
    for (int i = 0; i < 3; i++) S.F[i] = S.L[i];
    norm3(S.F);
    float b = S.earthBlend;
    float up[3];
    for (int i = 0; i < 3; i++) up[i] = pole[i] * (1.0f - b) + S.Z[i] * b;
    if (std::fabs(dot3(S.F, up)) > 0.995f) {
        for (int i = 0; i < 3; i++) up[i] = S.Z[i] * b + S.N[i] * (1.0f - b);
    }
    cross3(S.F, up, S.R);
    norm3(S.R);
    cross3(S.R, S.F, S.U);
    norm3(S.U);
}

bool nsNdc(const NightSkyState& S, const float dir[3], float& x, float& y) {
    float cz = dot3(dir, S.F);
    if (cz < 0.12f) return false;
    float th = std::tan(0.5f * S.fov * PI / 180.0f);
    x = dot3(dir, S.R) / cz / (th * S.aspect);
    y = -dot3(dir, S.U) / cz / th;
    return true;
}

void nsProject(NightSkyState& S, float aspect) {
    S.aspect = aspect;
    S.pts.clear();
    S.sunPt = -1;
    float b = S.earthBlend;
    for (size_t i = 0; i < S.cat.size(); i++) {
        NsCatStar& c = S.cat[i];
        c.ok = false;
        float v[3] = {c.pos[0] - S.obs[0], c.pos[1] - S.obs[1], c.pos[2] - S.obs[2]};
        float d = std::sqrt(dot3(v, v));
        if (d < 0.03f) continue;
        for (int k = 0; k < 3; k++) c.dir[k] = v[k] / d;
        c.adist = d;
        c.amag = std::max(-9.0f, c.mag + 5.0f * std::log10(d / c.dist));
        float x, y;
        if (!nsNdc(S, c.dir, x, y)) continue;
        c.nx = x; c.ny = y;
        float vis = 1.0f;
        if (b > 0.001f) {
            float alt = std::asin(std::max(-1.0f, std::min(1.0f, dot3(c.dir, S.Z))));
            float az = std::atan2(dot3(c.dir, S.E), dot3(c.dir, S.N));
            float t = std::max(0.0f, std::min(1.0f, (alt - nsHillAlt(az)) / 0.14f));
            t = t * t * (3.0f - 2.0f * t);
            vis = 1.0f - b * (1.0f - t);
        }
        c.vis = vis;
        c.ok = true;
        if (c.amag > 7.0f || vis < 0.02f || std::fabs(x) > 1.1f || std::fabs(y) > 1.1f) continue;
        NsPoint p{};
        p.id = (int)i;
        p.x = x; p.y = y;
        p.mag = c.amag; p.dist = d; p.vis = vis;
        for (int k = 0; k < 3; k++) p.rgb[k] = c.rgb[k];
        S.pts.push_back(p);
    }
    float sv[3] = {-S.obs[0], -S.obs[1], -S.obs[2]};
    float sd = std::sqrt(dot3(sv, sv));
    if (sd > 0.03f) {
        float dir[3] = {sv[0] / sd, sv[1] / sd, sv[2] / sd};
        float x, y;
        if (nsNdc(S, dir, x, y) && std::fabs(x) < 1.1f && std::fabs(y) < 1.1f) {
            NsPoint p{};
            p.id = -2;
            p.x = x; p.y = y;
            p.mag = 4.83f + 5.0f * std::log10(sd / (10.0f * LY_PER_PC));
            p.dist = sd; p.vis = 1.0f;
            p.rgb[0] = 1.0f; p.rgb[1] = 0.95f; p.rgb[2] = 0.78f;
            S.sunPt = (int)S.pts.size();
            S.pts.push_back(p);
        }
    }
}
