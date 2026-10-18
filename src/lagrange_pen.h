#pragma once
#include "vk_app.h"
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

struct LgText {
    std::string s;
    float x, y;
    int sc;
    float c[3];
};

struct LgPen {
    OverlayVertex* out;
    uint32_t& used;
    float W, H;
    std::vector<LgText> q;

    void line(float x0, float y0, float x1, float y1, const float c[3], float f = 1.0f) {
        if (used + 2 > MAX_VERTS - 16000) return;
        out[used++] = {-1.0f + 2.0f * x0 / W, -1.0f + 2.0f * y0 / H, c[0] * f, c[1] * f, c[2] * f};
        out[used++] = {-1.0f + 2.0f * x1 / W, -1.0f + 2.0f * y1 / H, c[0] * f, c[1] * f, c[2] * f};
    }

    void circle(float cx, float cy, float r, const float c[3], int seg = 32, float f = 1.0f) {
        for (int i = 0; i < seg; i++) {
            float a0 = 6.2831853f * (float)i / (float)seg, a1 = 6.2831853f * (float)(i + 1) / (float)seg;
            line(cx + r * std::cos(a0), cy + r * std::sin(a0), cx + r * std::cos(a1), cy + r * std::sin(a1), c, f);
        }
    }

    void rect(float x0, float y0, float x1, float y1, const float c[3]) {
        line(x0, y0, x1, y0, c);
        line(x1, y0, x1, y1, c);
        line(x1, y1, x0, y1, c);
        line(x0, y1, x0, y0, c);
    }

    void dot(float x, float y, float r, const float c[3]) {
        line(x - r, y, x + r, y, c);
        line(x, y - r, x, y + r, c);
    }

    void text(const std::string& s, float x, float y, int sc, const float c[3]) {
        q.push_back({s, x, y, sc, {c[0], c[1], c[2]}});
    }

    float width(const std::string& s, int sc) const { return (float)s.size() * 6.0f * (float)sc; }

    void flush() {
        float px = 2.0f / W, py = 2.0f / H;
        for (const LgText& t : q)
            emitText(out, used, t.s.c_str(), -1.0f + t.x * px, -1.0f + t.y * py, px, py, (float)t.sc, t.c[0], t.c[1], t.c[2]);
        q.clear();
    }
};

struct LgHalo {
    float ax, ay, tilt, rate, phase;
};

inline void lgHaloPoint(const LgHalo& h, float t, float& dx, float& dy) {
    float a = h.rate * t + h.phase;
    float ex = h.ax * std::cos(a), ey = h.ay * std::sin(a);
    float c = std::cos(h.tilt), s = std::sin(h.tilt);
    dx = ex * c - ey * s;
    dy = ex * s + ey * c;
}

void lgEmitInset(AppContext& app, LgPen& pen, float W, float H);
void lgEmitCard(AppContext& app, LgPen& pen, float W, float H);
