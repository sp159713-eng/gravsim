#include "vk_app.h"
#include "sbdraw.h"
#include "massctl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace sbd;

namespace {

const float WHITE[3] = {1.0f, 1.0f, 1.0f};
const float BH_RING[3] = {1.0f, 0.62f, 0.22f};
const float BH_HOT[3] = {1.0f, 0.92f, 0.75f};
const float BH_TRAIL[3] = {0.65f, 0.45f, 1.0f};
const float BLACK[3] = {0.0f, 0.0f, 0.0f};
const float PREVIEW_COL[6][3] = {{0.6f, 0.8f, 1.0f}, {1.0f, 0.8f, 0.5f}, {1.0f, 0.95f, 0.6f}, {0.85f, 0.6f, 1.0f}, {0.7f, 0.85f, 1.0f}, {0.85f, 0.6f, 1.0f}};
const char* const SOLAR_NAMES[4] = {"1 ROCK", "2 GAS GIANT", "3 STAR", "4 BLACK HOLE"};
const char* const GALAXY_NAMES[4] = {"1 STAR", "2 BLACK HOLE", "3 SUPERMASSIVE", "4 DWARF GALAXY"};
const double YEAR_UNITS = 4.4958;

void makeView(const AppContext& app, View& V) {
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    cameraViewProj(app.camera, W / H, V.vp);
    V.W = W;
    V.H = H;
    V.ppu = 0.5f * H / std::tan(30.0f * 3.14159265f / 180.0f);
    V.wn = std::max(1e-9f, app.camera.nearPlane);
    rayBasis(app.camera, V.back, V.right, V.up);
}

float worldRadius(const AppContext& app, int kind) {
    bool gal = app.scene == Scene::GALAXY;
    static const float galR[6] = {0.18f, 0.2f, 0.18f, 0.3f, 1.2f, 0.5f};
    static const float trueR[6] = {5.4e-5f, 5.9e-4f, 5.9e-3f, 5.0e-5f, 1e-4f, 5.0e-5f};
    static const float enlR[6] = {0.45f, 1.4f, 2.0f, 0.8f, 1.0f, 0.8f};
    return gal ? galR[kind] : app.trueScale ? trueR[kind] : enlR[kind];
}

float minPx(int kind) {
    static const float px[6] = {3.5f, 5.0f, 7.0f, 6.0f, 5.0f, 8.0f};
    return px[kind];
}

float bodyPx(const AppContext& app, const View& V, const ThrowBody& b, float w) {
    float wr = worldRadius(app, b.kind) * b.size;
    return std::max(minPx(b.kind) * b.size, wr * V.ppu / w);
}

void barRect(float W, float H, int i, float& x0, float& y0, float& x1, float& y1, int& ts) {
    ts = (H >= 800.0f && W >= 1100.0f) ? 2 : 1;
    float bw = 14.0f * 6.0f * ts + 44.0f, bh = 7.0f * ts + 18.0f, gap = 8.0f;
    float total = 4.0f * bw + 3.0f * gap;
    x0 = 0.5f * (W - total) + (float)i * (bw + gap);
    y0 = H - 14.0f - bh;
    x1 = x0 + bw;
    y1 = y0 + bh;
}

bool sunOf(const AppContext& app, double s[3]) {
    if (app.scene == Scene::SOLAR && app.displayValid) {
        for (int a = 0; a < 3; a++) s[a] = app.displayBodies[0].pos[a];
        return true;
    }
    return false;
}

void drawRockOrGas(Draw& D, const View& V, const AppContext& app, const ThrowBody& b, View::P c, float R) {
    double L[3] = {-0.4, 0.5, 0.75}, sun[3];
    bool lit = false;
    if (sunOf(app, sun)) {
        double d[3] = {sun[0] - b.p[0], sun[1] - b.p[1], sun[2] - b.p[2]};
        double len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) + 1e-12;
        for (int a = 0; a < 3; a++) d[a] /= len;
        L[0] = d[0] * V.right[0] + d[1] * V.right[1] + d[2] * V.right[2];
        L[1] = d[0] * V.up[0] + d[1] * V.up[1] + d[2] * V.up[2];
        L[2] = d[0] * V.back[0] + d[1] * V.back[1] + d[2] * V.back[2];
        lit = true;
    }
    const float amb = lit ? 0.1f : 0.4f;
    bool gas = b.kind == 1;
    if (R < 5.0f) {
        D.disc(c.x, c.y, R, b.col, amb + (1.0f - amb) * (0.5f + 0.5f * (float)L[2]));
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

void drawBodyTri(Draw& D, const View& V, const AppContext& app, const ThrowBody& b) {
    View::P c = V.proj(b.p[0], b.p[1], b.p[2]);
    if (!c.ok) return;
    float R = bodyPx(app, V, b, c.w);
    if (b.kind == 3 || b.kind == 5) {
        D.disc(c.x, c.y, R, BLACK, 1.0f);
    } else if (b.kind == 2) {
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
    } else if (b.kind == 4) {
        const float core[3] = {1.0f, 0.97f, 0.9f};
        D.disc(c.x, c.y, std::max(2.5f, R * 0.25f), core, 1.0f);
        D.disc(c.x, c.y, R * 0.55f, b.col, 0.35f);
    } else {
        drawRockOrGas(D, V, app, b, c, R);
    }
}

void drawBodyLines(Draw& D, const View& V, const AppContext& app, const ThrowBody& b) {
    const float* tc = (b.kind == 3 || b.kind == 5) ? BH_TRAIL : b.col;
    size_t n = b.trail.size();
    for (size_t i = 0; i + 1 < n; i++) {
        float f = (float)(i + 1) / (float)n;
        double p[3] = {b.trail[i][0], b.trail[i][1], b.trail[i][2]}, q[3] = {b.trail[i + 1][0], b.trail[i + 1][1], b.trail[i + 1][2]};
        D.seg3(p, q, tc, 0.8f * f * std::sqrt(f));
    }
    if (n) {
        double p[3] = {b.trail[n - 1][0], b.trail[n - 1][1], b.trail[n - 1][2]};
        D.seg3(p, b.p, tc, 0.8f);
    }
    View::P c = V.proj(b.p[0], b.p[1], b.p[2]);
    if (!c.ok) return;
    float R = bodyPx(app, V, b, c.w);
    if (b.kind == 2) {
        D.circle(c.x, c.y, R * 1.25f, b.col, 0.4f);
        D.circle(c.x, c.y, R * 1.6f, b.col, 0.18f);
        D.circle(c.x, c.y, R * 2.2f, b.col, 0.07f);
    } else if (b.kind == 3 || b.kind == 5) {
        D.circle(c.x, c.y, R * 1.25f, BH_HOT, 0.9f);
        D.circle(c.x, c.y, R * 1.7f, BH_RING, 0.9f);
        D.circle(c.x, c.y, R * 2.1f, BH_RING, 0.45f);
        D.circle(c.x, c.y, R * 2.7f, BH_RING, 0.18f);
    } else if (b.kind == 4) {
        for (int j = 0; j < 4; j++) D.circle(c.x, c.y, R * (0.5f + 0.35f * (float)j), b.col, 0.5f - 0.1f * (float)j);
    }
}

}

bool throwBarHit(const AppContext& app, double fx, double fy, int& which) {
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

void emitThrowLines(AppContext& app, OverlayVertex* out, uint32_t& used) {
    if (!throwOk(app)) return;
    ThrowState& T = app.th;
    View V;
    makeView(app, V);
    Draw D{out, used, V.W, V.H, &V};
    for (const ThrowBody& b : T.bodies) drawBodyLines(D, V, app, b);
    int shown = 0;
    for (size_t i = T.flashes.size(); i-- > 0 && shown < 24; shown++) {
        const ThrowFlash& f = T.flashes[i];
        View::P c = V.proj(f.p[0], f.p[1], f.p[2]);
        if (!c.ok) continue;
        float t = f.age / 1.3f, k = std::pow(1.0f - t, 1.5f);
        float base = 10.0f, R = base + (base * 3.0f + 20.0f) * t;
        for (int j = 0; j < 5; j++) D.circle(c.x, c.y, R * (0.25f + 0.19f * (float)j), j < 2 ? WHITE : f.col, k * (1.4f - 0.2f * (float)j));
    }
    if (!T.on) return;
    bool gal = app.scene == Scene::GALAXY;
    int kind = throwKind(gal, std::max(0, std::min(3, T.type)));
    const float* pc = PREVIEW_COL[kind];
    if (T.dragging) {
        size_t n = T.preview.size();
        for (size_t i = 0; i + 1 < n; i++)
            if ((i / 2) % 2 == 0) {
                double p[3] = {T.preview[i][0], T.preview[i][1], T.preview[i][2]}, q[3] = {T.preview[i + 1][0], T.preview[i + 1][1], T.preview[i + 1][2]};
                D.seg3(p, q, pc, 1.0f - 0.6f * (float)i / (float)n);
            }
        if (n > 1) {
            View::P e = V.proj(T.preview[n - 1][0], T.preview[n - 1][1], T.preview[n - 1][2]);
            if (e.ok) D.circle(e.x, e.y, 4.0f, pc, 0.7f);
        }
        double a[3] = {T.sx, T.sy, T.sz}, b[3] = {T.ex, T.ey, T.ez}, foot[3] = {T.ex, T.sy, T.ez};
        D.seg3(a, b, WHITE, 0.7f);
        if (T.vertical) D.seg3(b, foot, WHITE, 0.25f);
        View::P p0 = V.proj(a[0], a[1], a[2]), p1 = V.proj(b[0], b[1], b[2]);
        if (p0.ok) D.circle(p0.x, p0.y, std::max(5.0f, minPx(kind) * massSize(T.mk) * 1.3f), pc, 0.9f);
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
        barRect(V.W, V.H, i, x0, y0, x1, y1, ts);
        int ki = throwKind(gal, i);
        if (i == T.type) D.box(x0 - 2.0f, y0 - 2.0f, x1 + 2.0f, y1 + 2.0f, PREVIEW_COL[ki], 1.0f);
        if (ki == 3 || ki == 5) {
            D.circle(x0 + 18.0f, 0.5f * (y0 + y1), 8.5f, BH_HOT, 1.0f);
            D.circle(x0 + 18.0f, 0.5f * (y0 + y1), 10.5f, BH_RING, 0.7f);
        }
    }
}

void emitThrowTris(AppContext& app, OverlayVertex* out, uint32_t& used) {
    if (!throwOk(app)) return;
    ThrowState& T = app.th;
    View V;
    makeView(app, V);
    Draw D{out, used, V.W, V.H, &V};
    std::vector<std::pair<float, int>> order;
    for (size_t i = 0; i < T.bodies.size(); i++) {
        View::P c = V.proj(T.bodies[i].p[0], T.bodies[i].p[1], T.bodies[i].p[2]);
        if (c.ok) order.push_back({c.w, (int)i});
    }
    std::sort(order.begin(), order.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) { return a.first > b.first; });
    for (const auto& o : order) drawBodyTri(D, V, app, T.bodies[(size_t)o.second]);
    if (!T.on) return;
    bool gal = app.scene == Scene::GALAXY;
    const float barBg[3] = {0.02f, 0.04f, 0.09f}, barSel[3] = {0.08f, 0.16f, 0.32f};
    for (int i = 0; i < 4; i++) {
        float x0, y0, x1, y1;
        int ts;
        barRect(V.W, V.H, i, x0, y0, x1, y1, ts);
        D.rect(x0, y0, x1, y1, i == T.type ? barSel : barBg);
        int ki = throwKind(gal, i);
        if (ki != 3 && ki != 5) D.disc(x0 + 18.0f, 0.5f * (y0 + y1), 6.0f, ki == 4 ? PREVIEW_COL[4] : PREVIEW_COL[ki], 1.0f);
    }
}

void emitThrowText(AppContext& app, OverlayVertex* out, uint32_t& used) {
    if (!throwOk(app) || !app.th.on || app.showHelp) return;
    ThrowState& T = app.th;
    View V;
    makeView(app, V);
    float W = V.W, H = V.H;
    float px = 2.0f / W, py = 2.0f / H;
    bool gal = app.scene == Scene::GALAXY;
    auto Tx = [&](const char* str, float x, float y, float sc, const float c[3], float k) {
        if (used + 600 > MAX_VERTS) return;
        emitText(out, used, str, -1.0f + x * px, -1.0f + y * py, px, py, sc, c[0] * k, c[1] * k, c[2] * k);
    };
    const float orange[3] = {1.0f, 0.65f, 0.25f}, dim[3] = {0.7f, 0.75f, 0.85f}, warn[3] = {1.0f, 0.6f, 0.4f};
    int ts = H >= 800.0f ? 2 : 1;
    const char* const* names = gal ? GALAXY_NAMES : SOLAR_NAMES;
    float bx0, by0, bx1, by1;
    int bts;
    barRect(W, H, 0, bx0, by0, bx1, by1, bts);
    for (int i = 0; i < 4; i++) {
        float x0, y0, x1, y1;
        barRect(W, H, i, x0, y0, x1, y1, bts);
        Tx(names[i], x0 + 36.0f, y0 + 9.0f, (float)bts, i == T.type ? WHITE : dim, 1.0f);
    }
    double total = throwDefMass(gal, T.type) * massMult(T.mk);
    if (gal && T.type == 3) total += DWARF_STARS;
    char mb[160];
    std::snprintf(mb, sizeof(mb), "MASS %s   X%.3g   [ ] / SCROLL", fmtMass(throwMsun(gal, total), !gal).c_str(), massMult(T.mk));
    float tw = (float)std::strlen(mb) * 6.0f * (float)bts;
    Tx(mb, 0.5f * (W - tw), by0 - 10.0f - 7.0f * (float)bts, (float)bts, WHITE, 1.0f);
    float y = 62.0f;
    Tx("THROW MODE", 12.0f, y, 3.0f, orange, 1.0f);
    y += 21.0f + 8.0f;
    const char* legend[3] = {
        "DRAG: THROW   SHIFT+DRAG: UP / DOWN   CLICK: ORBIT",
        "RIGHT DRAG: LOOK   1-4 OR CLICK BAR: BODY TYPE",
        "[ ] OR SCROLL WHILE DRAGGING: MASS   C: CLEAR   F: EXIT",
    };
    for (int i = 0; i < 3; i++) {
        Tx(legend[i], 12.0f, y, (float)ts, dim, 0.9f);
        y += 7.0f * ts + 5.0f;
    }
    if (T.full) Tx("BODY LIMIT REACHED - C TO CLEAR", 12.0f, y, (float)ts, warn, 1.0f);
    if (T.dragging) {
        char buf[96];
        if (T.orbit) std::snprintf(buf, sizeof(buf), "RELEASE: CIRCULAR ORBIT");
        else if (gal) std::snprintf(buf, sizeof(buf), "SPEED %.1fX ORBITAL%s", T.speed / std::max(T.vc, 1e-12), T.vertical ? "  VERTICAL" : "");
        else {
            double kms = T.speed * YEAR_UNITS / solarUnitsPerAu(app.trueScale) * 4.7404;
            std::snprintf(buf, sizeof(buf), "SPEED %.1fX ORBITAL  %.0f KM/S%s", T.speed / std::max(T.vc, 1e-12), kms, T.vertical ? "  VERTICAL" : "");
        }
        View::P e = V.proj(T.ex, T.ey, T.ez);
        if (e.ok) Tx(buf, e.x + 14.0f, e.y + 10.0f, (float)ts, WHITE, 0.9f);
    }
}

void emitHelpUiThrow(AppContext& app, OverlayVertex* out, uint32_t& used) {
    uint32_t t0 = used;
    emitThrowTris(app, out, used);
    uint32_t mine = used - t0;
    emitHelpUi(app, out, used);
    app.overlayTriVerts[app.currentFrame] += mine;
    uint32_t u0 = used;
    emitThrowText(app, out, used);
    app.overlayUiVerts[app.currentFrame] += used - u0;
}
