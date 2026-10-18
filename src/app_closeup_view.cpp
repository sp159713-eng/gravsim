#include "closeup_view.h"
#include "closeup_data.h"
#include "nebulae.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr float TAN30 = 0.57735027f;
constexpr float MERCURY_AU = 0.387f;

bool proj(const float vp[16], const float p[3], float& x, float& y, float& w) {
    float c[4];
    for (int row = 0; row < 4; row++) c[row] = vp[row] * p[0] + vp[row + 4] * p[1] + vp[row + 8] * p[2] + vp[row + 12];
    w = c[3];
    if (w <= 1e-9f) return false;
    x = c[0] / w;
    y = c[1] / w;
    return true;
}

float bhDrawRadius(const AppContext& app) {
    float h = cuHalfHeight(app);
    return app.cu.kind == 1 ? 0.011f * h : 0.03f * h;
}

std::string commas(long long v) {
    std::string s = std::to_string(v);
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ",");
    return s;
}

struct Txt {
    OverlayVertex* out;
    uint32_t& used;
    float W, H, px, py;
    int s;
    void at(const char* t, float x, float y, const float c[3], int sc = 0) {
        emitText(out, used, t, -1.0f + x * px, -1.0f + y * py, px, py, (float)(sc ? sc : s), c[0], c[1], c[2]);
    }
    float width(const char* t, int sc = 0) { return (float)std::strlen(t) * 6.0f * (float)(sc ? sc : s); }
};

void circle(OverlayVertex* out, uint32_t& used, float cx, float cy, float r, float W, float H, const float c[3], int segs = 64) {
    for (int i = 0; i < segs && used + 2 <= MAX_VERTS; i++) {
        float a0 = 6.2831853f * i / segs, a1 = 6.2831853f * (i + 1) / segs;
        out[used++] = {-1.0f + (cx + r * std::cos(a0)) * 2.0f / W, -1.0f + (cy + r * std::sin(a0)) * 2.0f / H, c[0], c[1], c[2]};
        out[used++] = {-1.0f + (cx + r * std::cos(a1)) * 2.0f / W, -1.0f + (cy + r * std::sin(a1)) * 2.0f / H, c[0], c[1], c[2]};
    }
}

void polyline(OverlayVertex* out, uint32_t& used, const float vp[16], const std::vector<std::array<float, 3>>& pts, const float c[3]) {
    for (size_t i = 1; i < pts.size() && used + 2 <= MAX_VERTS; i++) {
        float x0, y0, w0, x1, y1, w1;
        if (!proj(vp, pts[i - 1].data(), x0, y0, w0) || !proj(vp, pts[i].data(), x1, y1, w1)) continue;
        out[used++] = {x0, y0, c[0], c[1], c[2]};
        out[used++] = {x1, y1, c[0], c[1], c[2]};
    }
}

}

float cuHalfHeight(const AppContext& app) {
    const CloseupState& c = app.cu;
    if (c.kind == 1) return 2300.0f * c.zoom;
    const Binary& b = binaryAt(c.obj);
    return b.aAu * (1.0f + b.ecc) * 1.5f * c.zoom;
}

void closeupViewMatrix(const AppContext& app, float aspect, float vp[16], Camera& cam) {
    const CloseupState& c = app.cu;
    cam = Camera{};
    cam.free = false;
    cam.yaw = c.vyaw;
    cam.pitch = c.vpitch;
    float D = cuHalfHeight(app) / TAN30;
    cam.dist = D;
    cam.nearPlane = D * 0.01f;
    cameraViewProjEx(cam, aspect, nullptr, cam.nearPlane, D * 200.0f, vp);
}

bool closeupFill(const AppContext& app, float aspect, CuDraw& d) {
    const CloseupState& c = app.cu;
    if (c.mode == 0 || c.alpha <= 0.002f) return false;
    std::memset(d.push, 0, sizeof(d.push));
    float H = (float)app.swapchainExtent.height;
    float* p = d.push;
    p[1] = c.clock;
    p[2] = c.alpha;
    p[3] = aspect;
    p[4] = H;
    p[5] = 1.0f;
    d.bh = false;
    if (c.kind == 0) {
        p[0] = (float)c.obj;
        p[5] = c.zoom * 1.0f;
        p[6] = c.panX;
        p[7] = c.panY;
        return true;
    }
    closeupViewMatrix(app, aspect, d.vp, d.cam);
    float x, y, w;
    bool bhOn = c.mode == 2 || c.alpha > 0.999f;
    d.bhRadius = bhDrawRadius(app);
    if (c.kind == 1) {
        p[0] = 20.0f;
        for (int i = 0; i < S_COUNT; i++) {
            float pos[3];
            starPosition(S_STARS[i], c.time * 1.0, pos);
            if (proj(d.vp, pos, x, y, w)) {
                p[8 + i * 4] = x;
                p[9 + i * 4] = y;
                p[10 + i * 4] = 4.5f;
            }
        }
        d.bh = bhOn;
        d.galMode = 1.0f;
        return true;
    }
    const Binary& b = binaryAt(c.obj);
    float bh[3], st[3];
    binaryPositions(b, c.time, bh, st);
    p[0] = 21.0f + (float)c.obj;
    if (proj(d.vp, st, x, y, w)) {
        float ppa = 0.5f * H * 1.7320508f / w;
        p[8] = x;
        p[9] = y;
        p[10] = std::max(b.starRadiusAu * ppa, 6.0f);
    }
    p[12] = b.starColor[0];
    p[13] = b.starColor[1];
    p[14] = b.starColor[2];
    p[15] = b.stream;
    if (proj(d.vp, bh, x, y, w)) {
        p[16] = x;
        p[17] = y;
    }
    for (int a = 0; a < 3; a++) d.bhPos[a] = bh[a];
    d.bh = bhOn;
    d.galMode = b.stream > 0.0f ? 1.0f : 2.0f;
    return true;
}

void emitCloseupLines(AppContext& app, OverlayVertex* out, uint32_t& used) {
    const CloseupState& c = app.cu;
    if (c.mode != 2 || c.kind == 0) return;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float vp[16];
    Camera cam;
    closeupViewMatrix(app, W / H, vp, cam);
    std::vector<std::array<float, 3>> pts;
    if (c.kind == 1) {
        for (int i = 0; i < S_COUNT; i++) {
            const StarOrbit& s = S_STARS[i];
            pts.clear();
            for (int k = 0; k <= 240; k++) {
                float pos[3];
                orbitPoint(s.aAu, s.ecc, s.incDeg, s.nodeDeg, s.argDeg, 6.2831853f * k / 240.0f, pos);
                pts.push_back({pos[0], pos[1], pos[2]});
            }
            const float col[3] = {s.color[0] * 0.55f, s.color[1] * 0.55f, s.color[2] * 0.55f};
            polyline(out, used, vp, pts, col);
        }
        float k = std::max(1.0f, H / 720.0f);
        float cx = 170.0f * k, cy = H * 0.64f, rm = 62.0f * k;
        const float mc[3] = {0.55f, 0.65f, 0.85f}, hc[3] = {1.0f, 0.6f, 0.2f};
        circle(out, used, cx, cy, rm, W, H, mc);
        float rh = rm * horizonAu() / MERCURY_AU;
        for (float f = 1.0f; f > 0.05f; f -= 0.12f) circle(out, used, cx, cy, rh * f, W, H, hc, 24);
        return;
    }
    const Binary& b = binaryAt(c.obj);
    for (int who = 0; who < 2; who++) {
        pts.clear();
        float ratio = who ? b.mBh / (b.mBh + b.mStar) : -b.mStar / (b.mBh + b.mStar);
        for (int k = 0; k <= 160; k++) {
            float pos[3];
            orbitPoint(b.aAu, b.ecc, b.incDeg, 35.0f, 20.0f, 6.2831853f * k / 160.0f, pos);
            pts.push_back({pos[0] * ratio, pos[1] * ratio, pos[2] * ratio});
        }
        const float col[3] = {who ? b.starColor[0] * 0.5f : 0.45f, who ? b.starColor[1] * 0.5f : 0.4f, who ? b.starColor[2] * 0.5f : 0.35f};
        polyline(out, used, vp, pts, col);
    }
}

void emitCloseupText(AppContext& app, OverlayVertex* out, uint32_t& used) {
    const CloseupState& c = app.cu;
    if (c.mode != 2) return;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    Txt t{out, used, W, H, 2.0f / W, 2.0f / H, W >= 1000.0f ? 2 : 1};
    const float white[3] = {1.0f, 1.0f, 1.0f}, grey[3] = {0.62f, 0.67f, 0.77f}, gold[3] = {1.0f, 0.8f, 0.4f};
    char buf[160];
    float line = 11.0f * (float)t.s;
    const char* hint = c.kind == 0 ? "DRAG = PAN   SCROLL = ZOOM   ESC OR BACKSPACE = BACK" : "DRAG = ROTATE   SCROLL = ZOOM   + / - = TIME   ESC OR BACKSPACE = BACK";
    t.at(hint, std::floor((W - t.width(hint)) * 0.5f), H - 12.0f - line, grey);
    if (c.kind == 0) {
        const NebulaInfo& n = nebulaInfos()[c.obj];
        t.at(n.name, 16.0f, H - 16.0f - 3.0f * line, white, t.s + 1);
        return;
    }
    float vp[16];
    Camera cam;
    closeupViewMatrix(app, W / H, vp, cam);
    float x, y, w;
    float y0 = 52.0f;
    if (c.kind == 1) {
        const char* cap = "THESE ORBITS WEIGHED SGR A* AT 4 MILLION SUNS - NOBEL PRIZE 2020 (GENZEL, GHEZ)";
        bigText(out, used, cap, W, H - 14.0f - 3.0f * line, t.px, t.py, fitScale(cap, W, 2), 1.0f, 0.82f, 0.4f);
        for (int i = 0; i < S_COUNT; i++) {
            const StarOrbit& s = S_STARS[i];
            float pos[3];
            starPosition(s, c.time, pos);
            float v = starSpeedKms(s, pos);
            std::snprintf(buf, sizeof(buf), "%-5s %6.2f YR  %6s KM/S  %4.1f%% C", s.name, s.periodYr, commas((long long)v).c_str(), v / 2.998e3f);
            t.at(buf, 16.0f, y0 + line * (float)i, s.color);
            if (proj(vp, pos, x, y, w)) t.at(s.name, (x + 1.0f) * 0.5f * W + 9.0f, (y + 1.0f) * 0.5f * H - 16.0f, s.color);
        }
        std::snprintf(buf, sizeof(buf), "TIME %.2f YR PER SECOND   T = %+.1f YR", c.rate, c.time);
        t.at(buf, 16.0f, y0 + line * (S_COUNT + 1), white);
        float bx = 0.0f, by = 0.0f;
        const float o[3] = {0.0f, 0.0f, 0.0f};
        float drawn = bhDrawRadius(app) / horizonAu();
        if (proj(vp, o, bx, by, w)) {
            t.at("SGR A*", (bx + 1.0f) * 0.5f * W - 190.0f, (by + 1.0f) * 0.5f * H + 14.0f, gold);
            std::snprintf(buf, sizeof(buf), "BLACK HOLE DRAWN %sX TOO BIG", commas((long long)drawn).c_str());
            t.at(buf, (bx + 1.0f) * 0.5f * W - 190.0f, (by + 1.0f) * 0.5f * H + 14.0f + line, grey);
        }
        float k = std::max(1.0f, H / 720.0f);
        float cx = 170.0f * k, cy = H * 0.64f, rm = 62.0f * k;
        t.at("TRUE SCALE", cx - t.width("TRUE SCALE") * 0.5f, cy - rm - 2.0f * line, white);
        std::snprintf(buf, sizeof(buf), "EVENT HORIZON %.2f AU", horizonAu());
        t.at(buf, cx - t.width(buf) * 0.5f, cy + rm + 6.0f, gold);
        std::snprintf(buf, sizeof(buf), "MERCURY ORBIT %.2f AU", MERCURY_AU);
        t.at(buf, cx - t.width(buf) * 0.5f, cy + rm + 6.0f + line, grey);
        std::snprintf(buf, sizeof(buf), "= %.1f HORIZONS ACROSS", MERCURY_AU / horizonAu());
        t.at(buf, cx - t.width(buf) * 0.5f, cy + rm + 6.0f + 2.0f * line, grey);
        return;
    }
    const Binary& b = binaryAt(c.obj);
    float bh[3], st[3], bh2[3], st2[3];
    binaryPositions(b, c.time, bh, st);
    binaryPositions(b, c.time + 0.01, bh2, st2);
    float dtS = 0.01f * 86400.0f;
    float AU_KM = 1.496e8f;
    auto speed = [&](const float a[3], const float a2[3]) {
        float d = std::sqrt((a2[0] - a[0]) * (a2[0] - a[0]) + (a2[1] - a[1]) * (a2[1] - a[1]) + (a2[2] - a[2]) * (a2[2] - a[2]));
        return d * AU_KM / dtS;
    };
    std::snprintf(buf, sizeof(buf), "ORBIT %.2f DAYS   E = %.2f", b.periodDays, b.ecc);
    t.at(buf, 16.0f, y0, white);
    std::snprintf(buf, sizeof(buf), "COMPANION %s KM/S", commas((long long)speed(st, st2)).c_str());
    t.at(buf, 16.0f, y0 + line, b.starColor);
    std::snprintf(buf, sizeof(buf), "BLACK HOLE %s KM/S", commas((long long)speed(bh, bh2)).c_str());
    t.at(buf, 16.0f, y0 + 2.0f * line, gold);
    std::snprintf(buf, sizeof(buf), "TIME %.2f DAYS PER SECOND   T = %+.1f D", c.rate, c.time);
    t.at(buf, 16.0f, y0 + 4.0f * line, white);
    float drawn = bhDrawRadius(app) / (b.horizonKm / AU_KM);
    std::snprintf(buf, sizeof(buf), "HORIZON RADIUS %.0f KM", b.horizonKm);
    t.at(buf, 16.0f, y0 + 5.0f * line, grey);
    std::snprintf(buf, sizeof(buf), "DRAWN %sX TOO BIG TO SEE", commas((long long)drawn).c_str());
    t.at(buf, 16.0f, y0 + 6.0f * line, grey);
    if (proj(vp, bh, x, y, w)) t.at("BLACK HOLE", (x + 1.0f) * 0.5f * W + 20.0f, (y + 1.0f) * 0.5f * H + 16.0f, gold);
    if (proj(vp, st, x, y, w)) t.at(c.obj == 0 ? "HDE 226868 - BLUE SUPERGIANT" : "SUN-LIKE STAR", (x + 1.0f) * 0.5f * W + 14.0f, (y + 1.0f) * 0.5f * H - 26.0f, b.starColor);
}
