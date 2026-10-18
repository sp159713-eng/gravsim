#include "vk_app.h"
#include "sbdraw.h"
#include "massctl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace sbd;

namespace {

const double VEL_PER_UNIT = 0.5;
const float PREVIEW_COL[4][3] = {{0.6f, 0.8f, 1.0f}, {1.0f, 0.8f, 0.5f}, {1.0f, 0.95f, 0.6f}, {0.85f, 0.6f, 1.0f}};
const float RAD[4] = {1.8f, 3.6f, 5.8f, 3.0f};
const float SWATCH[3][3] = {{0.45f, 0.65f, 1.0f}, {1.0f, 0.68f, 0.38f}, {1.0f, 0.93f, 0.55f}};
const float BH_RING[3] = {1.0f, 0.62f, 0.22f};
const float BH_HOT[3] = {1.0f, 0.92f, 0.75f};
const float BH_TRAIL[3] = {0.65f, 0.45f, 1.0f};
const float GRID_COL[3] = {0.4f, 0.55f, 0.9f};
const float WHITE[3] = {1.0f, 1.0f, 1.0f};
const float BLACK[3] = {0.0f, 0.0f, 0.0f};
const char* const NAMES[4] = {"1 ROCK", "2 GAS GIANT", "3 STAR", "4 BLACK HOLE"};

void barRect(float W, float H, int i, float& x0, float& y0, float& x1, float& y1, int& ts) {
    ts = H >= 800.0f ? 2 : 1;
    float bw = 14.0f * 6.0f * ts + 44.0f, bh = 7.0f * ts + 18.0f, gap = 8.0f;
    float total = 4.0f * bw + 3.0f * gap;
    x0 = 0.5f * (W - total) + (float)i * (bw + gap);
    y0 = H - 14.0f - bh;
    x1 = x0 + bw;
    y1 = y0 + bh;
}

void drawBody(Draw& D, const View& V, const SandboxState& S, const SbBody& b) {
    View::P c = V.proj(b.x, b.y, b.z);
    if (!c.ok) return;
    float R = std::max(2.5f, (float)b.r * V.ppu / c.w);
    if (b.type == 3) {
        D.disc(c.x, c.y, R, BLACK, 1.0f);
        return;
    }
    if (b.type == 2) {
        const float core[3] = {1.0f, 0.97f, 0.88f};
        if (R < 6.0f) {
            D.disc(c.x, c.y, R, b.col, 1.0f);
            D.disc(c.x, c.y, R * 0.6f, core, 1.0f);
        } else {
            D.shaded(c.x, c.y, R, [&](float rho, float) {
                float t = rho * rho;
                return Col{core[0] + (b.col[0] - core[0]) * t, core[1] + (b.col[1] - core[1]) * t, core[2] + (b.col[2] - core[2]) * t};
            });
        }
        return;
    }
    const SbBody* star = nullptr;
    double best = 1e30;
    for (const SbBody& o : S.bodies) {
        if (&o == &b || o.type != 2) continue;
        double dx = o.x - b.x, dy = o.y - b.y, dz = o.z - b.z, d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < best) { best = d2; star = &o; }
    }
    double L[3] = {-0.4, 0.5, 0.75};
    bool lit = false;
    if (star) {
        double d[3] = {star->x - b.x, star->y - b.y, star->z - b.z};
        double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) + 1e-9;
        for (int a = 0; a < 3; a++) d[a] /= len;
        L[0] = d[0] * V.right[0] + d[1] * V.right[1] + d[2] * V.right[2];
        L[1] = d[0] * V.up[0] + d[1] * V.up[1] + d[2] * V.up[2];
        L[2] = d[0] * V.back[0] + d[1] * V.back[1] + d[2] * V.back[2];
        lit = true;
    }
    const float amb = lit ? 0.06f : 0.35f;
    bool gas = b.type == 1;
    if (R < 5.0f) {
        float I = amb + (1.0f - amb) * (0.5f + 0.5f * (float)L[2]);
        D.disc(c.x, c.y, R, b.col, I);
        return;
    }
    D.shaded(c.x, c.y, R, [&](float rho, float a) {
        float nx = rho * std::cos(a), ny = -rho * std::sin(a), nz = std::sqrt(std::max(0.0f, 1.0f - rho * rho));
        float d = nx * (float)L[0] + ny * (float)L[1] + nz * (float)L[2];
        float I = amb + (1.0f - amb) * std::max(0.0f, d);
        float band = gas ? 0.88f + 0.12f * std::sin(ny * 9.0f) : 1.0f;
        return Col{std::min(1.0f, b.col[0] * I * band), std::min(1.0f, b.col[1] * I * band), std::min(1.0f, b.col[2] * I * band)};
    });
}

}

bool sbBarHit(const AppContext& app, double fx, double fy, int& which) {
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    for (int i = 0; i < 4; i++) {
        float x0, y0, x1, y1;
        int ts;
        barRect(W, H, i, x0, y0, x1, y1, ts);
        if (fx >= x0 && fx <= x1 && fy >= y0 && fy <= y1) {
            which = i;
            return true;
        }
    }
    return false;
}

void emitSandbox(AppContext& app, OverlayVertex* out, uint32_t& used) {
    const SandboxState& S = app.sb;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    View V;
    cameraViewProj(app.camera, W / H, V.vp);
    V.W = W;
    V.H = H;
    V.ppu = 0.5f * H / std::tan(30.0f * 3.14159265f / 180.0f);
    {
        const Camera& c = app.camera;
        double cp = std::cos(c.pitch), sp = std::sin(c.pitch), sy = std::sin(c.yaw), cy = std::cos(c.yaw);
        V.back[0] = cp * sy; V.back[1] = sp; V.back[2] = cp * cy;
        V.right[0] = cy; V.right[1] = 0.0; V.right[2] = -sy;
        V.up[0] = V.back[1] * V.right[2] - V.back[2] * V.right[1];
        V.up[1] = V.back[2] * V.right[0] - V.back[0] * V.right[2];
        V.up[2] = V.back[0] * V.right[1] - V.back[1] * V.right[0];
    }
    Draw D{out, used, W, H, &V};

    if (app.showGrid) {
        for (int i = -8; i <= 8; i++) {
            double g = 25.0 * i, e = 200.0;
            float k = i == 0 ? 0.2f : 0.09f;
            double a0[3] = {g, 0, -e}, a1[3] = {g, 0, e}, b0[3] = {-e, 0, g}, b1[3] = {e, 0, g};
            D.seg3(a0, a1, GRID_COL, k);
            D.seg3(b0, b1, GRID_COL, k);
        }
        for (int r = 50; r <= 200; r += 50) D.circle3(0, 0, 0, r, 72, GRID_COL, 0.12f);
    }
    {
        const float gasCol[3] = {1.0f, 0.55f, 0.25f}, discCol[3] = {0.75f, 0.85f, 1.0f};
        for (const SbPart& p : S.parts) {
            View::P c = V.proj(p.x, p.y, p.z);
            if (!c.ok) continue;
            const float* pc = p.disc ? discCol : gasCol;
            D.seg(c.x - 1.3f, c.y, c.x + 1.3f, c.y, pc, 1.0f);
            D.seg(c.x, c.y - 1.3f, c.x, c.y + 1.3f, pc, 1.0f);
        }
    }
    for (const SbBody& b : S.bodies) {
        const float* tc = b.type == 3 ? BH_TRAIL : b.col;
        size_t n = b.trail.size();
        for (size_t i = 0; i + 1 < n; i++) {
            float f = (float)(i + 1) / (float)n;
            double p[3] = {b.trail[i][0], b.trail[i][1], b.trail[i][2]}, q[3] = {b.trail[i + 1][0], b.trail[i + 1][1], b.trail[i + 1][2]};
            D.seg3(p, q, tc, 0.8f * f * std::sqrt(f));
        }
        if (n) {
            double p[3] = {b.trail[n - 1][0], b.trail[n - 1][1], b.trail[n - 1][2]}, q[3] = {b.x, b.y, b.z};
            D.seg3(p, q, tc, 0.8f);
        }
        if (std::fabs(b.y) > 0.8) {
            double p[3] = {b.x, b.y, b.z}, q[3] = {b.x, 0.0, b.z};
            D.seg3(p, q, tc, 0.3f);
            D.circle3(b.x, 0.0, b.z, std::max(1.5, b.r * 0.8), 14, tc, 0.28f);
        }
        View::P c = V.proj(b.x, b.y, b.z);
        if (!c.ok) continue;
        float rp = std::max(2.5f, (float)b.r * V.ppu / c.w);
        if (b.type == 2) {
            D.circle(c.x, c.y, rp * 1.25f, b.col, 0.4f);
            D.circle(c.x, c.y, rp * 1.6f, b.col, 0.18f);
            D.circle(c.x, c.y, rp * 2.2f, b.col, 0.07f);
        } else if (b.type == 3) {
            D.circle3(b.x, b.y, b.z, b.r * 2.0, 56, BH_RING, 0.9f);
            D.circle3(b.x, b.y, b.z, b.r * 1.5, 56, BH_HOT, 0.8f);
            D.circle3(b.x, b.y, b.z, b.r * 2.6, 56, BH_RING, 0.25f);
            D.circle(c.x, c.y, rp * 1.25f, BH_HOT, 0.9f);
        }
    }
    int shown = 0;
    for (size_t i = S.flashes.size(); i-- > 0 && shown < 24; shown++) {
        const SbFlash& f = S.flashes[i];
        View::P c = V.proj(f.x, f.y, f.z);
        if (!c.ok) continue;
        float t = f.age / 1.3f, k = std::pow(1.0f - t, 1.5f);
        float base = std::max(6.0f, f.r * V.ppu / c.w * 1.5f), R = base + (base * 3.0f + 20.0f) * t;
        for (int j = 0; j < 5; j++) D.circle(c.x, c.y, R * (0.25f + 0.19f * (float)j), j < 2 ? WHITE : f.col, k * (1.4f - 0.2f * (float)j));
    }
    if (S.dragging) {
        int ty = std::max(0, std::min(3, S.type));
        const float* pc = PREVIEW_COL[ty];
        size_t n = S.preview.size();
        for (size_t i = 0; i + 1 < n; i++)
            if ((i / 2) % 2 == 0) {
                double p[3] = {S.preview[i][0], S.preview[i][1], S.preview[i][2]}, q[3] = {S.preview[i + 1][0], S.preview[i + 1][1], S.preview[i + 1][2]};
                D.seg3(p, q, pc, 1.0f - 0.65f * (float)i / (float)n);
            }
        if (n > 1) {
            View::P e = V.proj(S.preview[n - 1][0], S.preview[n - 1][1], S.preview[n - 1][2]);
            if (e.ok) D.circle(e.x, e.y, 4.0f, pc, 0.7f);
        }
        double a[3] = {S.sx, S.sy, S.sz}, b[3] = {S.ex, S.ey, S.ez}, foot[3] = {S.ex, 0.0, S.ez};
        D.seg3(a, b, WHITE, 0.7f);
        if (S.vertical) D.seg3(b, foot, WHITE, 0.25f);
        View::P p0 = V.proj(a[0], a[1], a[2]), p1 = V.proj(b[0], b[1], b[2]);
        if (p0.ok) D.circle(p0.x, p0.y, std::max(4.0f, RAD[ty] * massSize(S.mk) * V.ppu / p0.w), pc, 0.9f);
        if (p0.ok && p1.ok) {
            float len = std::sqrt((p1.x - p0.x) * (p1.x - p0.x) + (p1.y - p0.y) * (p1.y - p0.y));
            if (len > 8.0f) {
                float ux = (p1.x - p0.x) / len, uy = (p1.y - p0.y) / len;
                D.seg(p1.x, p1.y, p1.x - 9.0f * ux + 5.0f * uy, p1.y - 9.0f * uy - 5.0f * ux, WHITE, 0.8f);
                D.seg(p1.x, p1.y, p1.x - 9.0f * ux - 5.0f * uy, p1.y - 9.0f * uy + 5.0f * ux, WHITE, 0.8f);
            }
        }
    }
    for (int i = 0; i < 4; i++) {
        float x0, y0, x1, y1;
        int ts;
        barRect(W, H, i, x0, y0, x1, y1, ts);
        if (i == S.type) D.box(x0 - 2.0f, y0 - 2.0f, x1 + 2.0f, y1 + 2.0f, PREVIEW_COL[i], 1.0f);
        if (i == 3) {
            D.circle(x0 + 18.0f, 0.5f * (y0 + y1), 8.5f, BH_HOT, 1.0f);
            D.circle(x0 + 18.0f, 0.5f * (y0 + y1), 10.5f, BH_RING, 0.7f);
        }
    }
    uint32_t lineVerts = used;
    uint32_t triStart = used;

    std::vector<std::pair<float, int>> order;
    for (size_t i = 0; i < S.bodies.size(); i++) {
        View::P c = V.proj(S.bodies[i].x, S.bodies[i].y, S.bodies[i].z);
        if (c.ok) order.push_back({c.w, (int)i});
    }
    std::sort(order.begin(), order.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first > b.first; });
    for (const auto& o : order) drawBody(D, V, S, S.bodies[(size_t)o.second]);
    const float barBg[3] = {0.02f, 0.04f, 0.09f}, barSel[3] = {0.08f, 0.16f, 0.32f};
    for (int i = 0; i < 4; i++) {
        float x0, y0, x1, y1;
        int ts;
        barRect(W, H, i, x0, y0, x1, y1, ts);
        D.rect(x0, y0, x1, y1, i == S.type ? barSel : barBg);
    }
    for (int i = 0; i < 3; i++) {
        float x0, y0, x1, y1;
        int ts;
        barRect(W, H, i, x0, y0, x1, y1, ts);
        D.disc(x0 + 18.0f, 0.5f * (y0 + y1), 5.0f + 2.0f * (float)i, SWATCH[i], 1.0f);
    }
    uint32_t myTris = used - triStart;

    emitHelpUi(app, out, used);
    app.overlayLineVerts[app.currentFrame] = lineVerts;
    app.overlayPointVerts[app.currentFrame] = 0;
    app.overlayTriVerts[app.currentFrame] += myTris;

    uint32_t textStart = used;
    float px = 2.0f / W, py = 2.0f / H;
    int ts = H >= 800.0f ? 2 : 1;
    auto T = [&](const char* str, float x, float y, float sc, const float c[3], float k) {
        if (used + 600 > MAX_VERTS) return;
        emitText(out, used, str, -1.0f + x * px, -1.0f + y * py, px, py, sc, c[0] * k, c[1] * k, c[2] * k);
    };
    const float hud[3] = {0.6f, 0.8f, 1.0f}, dim[3] = {0.7f, 0.75f, 0.85f}, warn[3] = {1.0f, 0.6f, 0.4f};
    if (app.showLabels)
        for (const SbBody& b : S.bodies) {
            if (!b.name) continue;
            View::P c = V.proj(b.x, b.y, b.z);
            if (!c.ok) continue;
            float rp = std::max(2.5f, (float)b.r * V.ppu / c.w);
            T(b.name, c.x + rp + 5.0f, c.y - 3.5f * (float)ts, (float)ts, dim, 0.9f);
        }
    for (int i = 0; i < 4; i++) {
        float x0, y0, x1, y1;
        int bts;
        barRect(W, H, i, x0, y0, x1, y1, bts);
        T(NAMES[i], x0 + 36.0f, y0 + 9.0f, (float)bts, i == S.type ? WHITE : dim, 1.0f);
    }
    {
        static const double MSUN[4] = {3.003e-6, 9.546e-4, 1.0, 10.0};
        float bx0, by0, bx1, by1;
        int bts;
        barRect(W, H, 0, bx0, by0, bx1, by1, bts);
        char mb[128];
        std::snprintf(mb, sizeof(mb), "MASS %s   X%.3g   [ ] / SCROLL", fmtMass(MSUN[std::max(0, std::min(3, S.type))] * massMult(S.mk), true).c_str(), massMult(S.mk));
        float tw = (float)std::strlen(mb) * 6.0f * (float)bts;
        T(mb, 0.5f * (W - tw), by0 - 10.0f - 7.0f * (float)bts, (float)bts, WHITE, 1.0f);
    }
    if (S.capT > 0.0f && S.capName[0] && !app.showHelp) {
        float k = std::min(1.0f, S.capT / 1.5f);
        float nsc = (float)ts + 1.0f;
        float nw = (float)std::strlen(S.capName) * 6.0f * nsc, fw = (float)std::strlen(S.capFact) * 6.0f * (float)ts;
        float fy = H - 14.0f - (7.0f * (float)ts + 18.0f) - 10.0f - 7.0f * (float)ts - 30.0f - 7.0f * (float)ts;
        T(S.capFact, 0.5f * (W - fw), fy, (float)ts, dim, k);
        T(S.capName, 0.5f * (W - nw), fy - 12.0f - 7.0f * nsc, nsc, WHITE, k);
    }
    if (!app.showHelp) {
        char buf[96];
        T("SANDBOX", 12.0f, 12.0f, (float)ts + 1.0f, hud, 1.0f);
        float y = 12.0f + 7.0f * ((float)ts + 1.0f) + 8.0f;
        if (S.paused) std::snprintf(buf, sizeof(buf), "BODIES %d/120   PAUSED", (int)S.bodies.size());
        else std::snprintf(buf, sizeof(buf), "BODIES %d/120   SPEED %.2gX", (int)S.bodies.size(), S.speed);
        T(buf, 12.0f, y, (float)ts, WHITE, 0.95f);
        y += 7.0f * ts + 8.0f;
        const char* legend[5] = {
            "DRAG: THROW   SHIFT+DRAG: UP / DOWN   CLICK: ORBIT",
            "RIGHT DRAG: ORBIT CAM   MIDDLE DRAG: PAN   SCROLL: ZOOM",
            "1-4: BODY TYPE   SPACE: PAUSE   + / -: SPEED   R: RESET",
            "C: CLEAR   X: SUN OFF / ON   P / SHIFT+P: PRESETS   G: GRID   H: HELP",
            "[ ] OR SCROLL WHILE DRAGGING: MASS X2 / X0.5",
        };
        for (int i = 0; i < 5; i++) {
            T(legend[i], 12.0f, y, (float)ts, dim, 0.85f);
            y += 7.0f * ts + 5.0f;
        }
        if (S.full) T("BODY LIMIT REACHED - C TO CLEAR", 12.0f, y, (float)ts, warn, 1.0f);
        if (S.dragging) {
            double dx = S.ex - S.sx, dy = S.ey - S.sy, dz = S.ez - S.sz;
            double dpx = app.cursorFb[0] - S.pressPx, dpy = app.cursorFb[1] - S.pressPy;
            bool orbit = !S.demo && dpx * dpx + dpy * dpy < 36.0;
            if (orbit) std::snprintf(buf, sizeof(buf), "RELEASE: CIRCULAR ORBIT");
            else std::snprintf(buf, sizeof(buf), "SPEED %.1f%s", VEL_PER_UNIT * std::sqrt(dx * dx + dy * dy + dz * dz), S.vertical ? "  VERTICAL" : "");
            View::P e = V.proj(S.ex, S.ey, S.ez);
            if (e.ok) T(buf, e.x + 14.0f, e.y + 10.0f, (float)ts, WHITE, 0.9f);
        }
    }
    app.overlayUiVerts[app.currentFrame] += used - textStart;
}
