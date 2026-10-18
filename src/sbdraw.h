#pragma once
#include "vk_app.h"
#include <algorithm>
#include <array>
#include <cmath>

constexpr uint32_t DRAW_RESERVE = 26000;

namespace sbd {
typedef std::array<float, 3> Col;

struct View {
    float vp[16];
    float W, H, ppu;
    float wn = 0.3f;
    double back[3], right[3], up[3];
    struct P {
        float x, y, w;
        bool ok;
    };
    void clip(const float p[3], float c[4]) const {
        for (int r = 0; r < 4; r++) c[r] = vp[r] * p[0] + vp[r + 4] * p[1] + vp[r + 8] * p[2] + vp[r + 12];
    }
    P proj(double x, double y, double z) const {
        float p[3] = {(float)x, (float)y, (float)z}, c[4];
        clip(p, c);
        if (c[3] < wn) return {0, 0, c[3], false};
        return {(c[0] / c[3] + 1.0f) * 0.5f * W, (c[1] / c[3] + 1.0f) * 0.5f * H, c[3], true};
    }
};

struct Draw {
    OverlayVertex* out;
    uint32_t& used;
    float W, H;
    const View* v;

    bool room(uint32_t n) const { return used + n < MAX_VERTS - DRAW_RESERVE; }

    void vert(float x, float y, const float c[3], float k) {
        out[used++] = {-1.0f + 2.0f * x / W, -1.0f + 2.0f * y / H, std::min(1.0f, c[0] * k), std::min(1.0f, c[1] * k), std::min(1.0f, c[2] * k)};
    }

    void seg(float x0, float y0, float x1, float y1, const float c[3], float k) {
        if (!room(2)) return;
        if ((x0 < 0 && x1 < 0) || (x0 > W && x1 > W) || (y0 < 0 && y1 < 0) || (y0 > H && y1 > H)) return;
        vert(x0, y0, c, k);
        vert(x1, y1, c, k);
    }

    void seg3(const double a[3], const double b[3], const float c[3], float k) {
        float pa[3] = {(float)a[0], (float)a[1], (float)a[2]}, pb[3] = {(float)b[0], (float)b[1], (float)b[2]};
        float ca[4], cb[4];
        v->clip(pa, ca);
        v->clip(pb, cb);
        const float wn = v->wn;
        if (ca[3] < wn && cb[3] < wn) return;
        if (ca[3] < wn) {
            float t = (wn - ca[3]) / (cb[3] - ca[3]);
            for (int i = 0; i < 4; i++) ca[i] += (cb[i] - ca[i]) * t;
        } else if (cb[3] < wn) {
            float t = (wn - cb[3]) / (ca[3] - cb[3]);
            for (int i = 0; i < 4; i++) cb[i] += (ca[i] - cb[i]) * t;
        }
        seg((ca[0] / ca[3] + 1.0f) * 0.5f * W, (ca[1] / ca[3] + 1.0f) * 0.5f * H, (cb[0] / cb[3] + 1.0f) * 0.5f * W, (cb[1] / cb[3] + 1.0f) * 0.5f * H, c, k);
    }

    void circle(float cx, float cy, float r, const float c[3], float k) {
        if (r < 1.0f || cx + r < 0 || cx - r > W || cy + r < 0 || cy - r > H) return;
        int n = std::min(72, std::max(16, (int)(r * 0.8f)));
        float px = cx + r, py = cy;
        for (int i = 1; i <= n; i++) {
            float a = 6.2831853f * (float)i / (float)n;
            float qx = cx + r * std::cos(a), qy = cy + r * std::sin(a);
            seg(px, py, qx, qy, c, k);
            px = qx;
            py = qy;
        }
    }

    void circle3(double cx, double cy, double cz, double r, int n, const float c[3], float k) {
        double p[3] = {cx + r, cy, cz};
        for (int i = 1; i <= n; i++) {
            double a = 6.2831853 * (double)i / (double)n;
            double q[3] = {cx + r * std::cos(a), cy, cz + r * std::sin(a)};
            seg3(p, q, c, k);
            p[0] = q[0]; p[1] = q[1]; p[2] = q[2];
        }
    }

    void disc(float cx, float cy, float r, const float c[3], float k) {
        if (cx + r < 0 || cx - r > W || cy + r < 0 || cy - r > H || !room(90)) return;
        int n = std::min(28, std::max(10, (int)(r * 0.7f) + 8));
        float px = cx + r, py = cy;
        for (int i = 1; i <= n; i++) {
            float a = 6.2831853f * (float)i / (float)n;
            float qx = cx + r * std::cos(a), qy = cy + r * std::sin(a);
            vert(cx, cy, c, k);
            vert(px, py, c, k);
            vert(qx, qy, c, k);
            px = qx;
            py = qy;
        }
    }

    template <class F>
    void shaded(float cx, float cy, float R, F f) {
        if (cx + R < 0 || cx - R > W || cy + R < 0 || cy - R > H) return;
        int N = std::min(22, std::max(10, (int)(R * 0.6f)));
        if (!room(15 * (uint32_t)N)) return;
        const float rho[4] = {0.0f, 0.6f, 0.88f, 1.0f};
        for (int i = 0; i < N; i++) {
            float a0 = 6.2831853f * (float)i / (float)N, a1 = 6.2831853f * (float)(i + 1) / (float)N;
            float c0x = std::cos(a0), c0y = std::sin(a0), c1x = std::cos(a1), c1y = std::sin(a1);
            Col mid = f(0.0f, 0.5f * (a0 + a1));
            Col C = f(rho[1], a0), D = f(rho[1], a1);
            vert(cx, cy, mid.data(), 1.0f);
            vert(cx + rho[1] * R * c0x, cy + rho[1] * R * c0y, C.data(), 1.0f);
            vert(cx + rho[1] * R * c1x, cy + rho[1] * R * c1y, D.data(), 1.0f);
            for (int k = 1; k < 3; k++) {
                float r0 = rho[k], r1 = rho[k + 1];
                Col A = f(r0, a0), B = f(r0, a1);
                C = f(r1, a0);
                D = f(r1, a1);
                float ax = cx + r0 * R * c0x, ay = cy + r0 * R * c0y, bx = cx + r0 * R * c1x, by = cy + r0 * R * c1y;
                float qx = cx + r1 * R * c0x, qy = cy + r1 * R * c0y, dx = cx + r1 * R * c1x, dy = cy + r1 * R * c1y;
                vert(ax, ay, A.data(), 1.0f); vert(bx, by, B.data(), 1.0f); vert(qx, qy, C.data(), 1.0f);
                vert(bx, by, B.data(), 1.0f); vert(dx, dy, D.data(), 1.0f); vert(qx, qy, C.data(), 1.0f);
            }
        }
    }

    void rect(float x0, float y0, float x1, float y1, const float c[3]) {
        vert(x0, y0, c, 1.0f); vert(x1, y0, c, 1.0f); vert(x1, y1, c, 1.0f);
        vert(x0, y0, c, 1.0f); vert(x1, y1, c, 1.0f); vert(x0, y1, c, 1.0f);
    }

    void box(float x0, float y0, float x1, float y1, const float c[3], float k) {
        seg(x0, y0, x1, y0, c, k); seg(x1, y0, x1, y1, c, k); seg(x1, y1, x0, y1, c, k); seg(x0, y1, x0, y0, c, k);
    }
};

}
