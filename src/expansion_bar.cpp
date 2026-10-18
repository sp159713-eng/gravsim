#include "vk_app.h"
#include "lagrange_pen.h"
#include "expansion.h"
#include "cosmo.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

void ExTexts::flush(OverlayVertex* out, uint32_t& used, float W, float H) {
    float px = 2.0f / W, py = 2.0f / H;
    for (const Item& t : q)
        emitText(out, used, t.s.c_str(), -1.0f + t.x * px, -1.0f + t.y * py, px, py, t.sc, t.c[0], t.c[1], t.c[2]);
    q.clear();
}

namespace {

struct Label {
    std::string s;
    float x;
    const float* col;
};

}

void exEmitBar(LgPen& pen, ExTexts& tx, float W, float H, float k, double tSec) {
    const float DIM[3] = {0.62f, 0.68f, 0.82f};
    const float WHITE[3] = {0.95f, 0.96f, 1.0f};
    const float CUR[3] = {1.0f, 0.95f, 0.3f};
    const float TICK[3] = {0.5f, 0.55f, 0.7f};
    float xl = 0.035f * W, xr = 0.965f * W, xw = xr - xl;
    float yb = H - 74.0f * k, th = 12.0f * k;
    float xB = xl + (float)EX_BREAK * xw;
    auto X = [&](double u) { return xl + (float)u * xw; };

    int cols = (int)xw;
    for (int i = 0; i <= cols; i++) {
        float x = xl + (float)i;
        if (std::fabs(x - xB) < 4.0f * k) continue;
        double u = (double)i / (double)cols;
        double a = cosmo::scaleAt(exUToTime(u));
        float rgb[3];
        exTempColor(cosmo::tempK(a), rgb);
        float c[3];
        c[0] = std::min(1.0f, std::max(rgb[0] * 1.9f, 0.10f));
        c[1] = std::min(1.0f, std::max(rgb[1] * 1.9f, 0.11f));
        c[2] = std::min(1.0f, std::max(rgb[2] * 1.9f, 0.20f));
        pen.line(x, yb, x, yb + th, c);
    }
    pen.line(xB - 5.0f * k, yb + th + 4.0f * k, xB + 1.0f * k, yb - 4.0f * k, WHITE);
    pen.line(xB - 1.0f * k, yb + th + 4.0f * k, xB + 5.0f * k, yb - 4.0f * k, WHITE);

    const std::vector<ExEra>& E = exEras();
    std::vector<Label> labels;
    for (const ExEra& e : E) {
        double t = e.t0;
        if (e.bar[0] == 'N' && e.bar[1] == 'O') t = cosmo::nowSec();
        labels.push_back({e.bar, X(exTimeToU(t)), e.col});
        if (e.t1 > e.t0 && e.t1 < 1.0e3 * cosmo::GYR_S) {
            float xa = X(exTimeToU(e.t0)), xb = X(exTimeToU(e.t1));
            pen.line(xa, yb - 2.0f * k, xb, yb - 2.0f * k, e.col, 0.9f);
        }
    }
    std::sort(labels.begin(), labels.end(), [](const Label& a, const Label& b) { return a.x < b.x; });
    float sc = std::max(1.0f, 1.25f * k);
    float rowEdge[12];
    for (float& r : rowEdge) r = -1e9f;
    float rowH = 11.5f * k;
    for (const Label& l : labels) {
        float w = tx.width(l.s, sc);
        int r = 0;
        while (r < 11 && l.x < rowEdge[r] + 5.0f * k) r++;
        rowEdge[r] = l.x + w;
        float y = yb - 12.0f * k - (float)(r + 1) * rowH + 3.0f * k;
        pen.line(l.x, y + 9.0f * k, l.x, yb - 2.0f * k, l.col, 0.55f);
        tx.add(l.s, l.x + 2.0f * k, y, sc, l.col);
    }

    float ts = std::max(1.0f, 1.15f * k);
    float ty = yb + th + 6.0f * k;
    auto tick = [&](double u, const std::string& s, bool leftAlign) {
        float x = X(u);
        pen.line(x, yb + th, x, yb + th + 4.0f * k, TICK);
        float w = tx.width(s, ts);
        tx.add(s, leftAlign ? x - 2.0f * k : x - 0.5f * w, ty, ts, DIM);
    };
    tick(0.0, "1E-43 S", true);
    const int dec[3] = {-30, -20, -10};
    for (int d : dec) {
        char b[24];
        std::snprintf(b, sizeof b, "1E%d S", d);
        tick(EX_BREAK * (d + 43.0) / 43.0, b, false);
    }
    tick(EX_BREAK, "1 S", false);
    for (int g = 10; g <= 50; g += 10) {
        char b[24];
        std::snprintf(b, sizeof b, "%d GYR", g);
        tick(EX_BREAK + (1.0 - EX_BREAK) * g / EX_TEND_GYR, b, false);
    }
    const float BRK[3] = {1.0f, 0.9f, 0.5f};
    std::string note = "AXIS BREAK: LOG SECONDS (LEFT) - LINEAR BILLION YEARS (RIGHT)";
    float ns = std::max(1.0f, 1.2f * k);
    tx.add(note, xB - 0.5f * tx.width(note, ns) + 0.0f, ty + 14.0f * k, ns, BRK);

    float xc = X(exTimeToU(tSec));
    pen.line(xc, yb - 6.0f * k, xc, yb + th + 6.0f * k, CUR);
    pen.line(xc - 1.0f, yb - 6.0f * k, xc - 1.0f, yb + th + 6.0f * k, CUR);
    pen.line(xc, yb + th + 6.0f * k, xc - 5.0f * k, yb + th + 13.0f * k, CUR);
    pen.line(xc, yb + th + 6.0f * k, xc + 5.0f * k, yb + th + 13.0f * k, CUR);
    pen.line(xc - 5.0f * k, yb + th + 13.0f * k, xc + 5.0f * k, yb + th + 13.0f * k, CUR);

    const float FOOT[3] = {1.0f, 0.62f, 0.4f};
    std::string foot = "NEW DATA (DESI 2025) HINTS DARK ENERGY MAY BE WEAKENING - STILL UNCERTAIN";
    float fs = std::max(1.0f, 1.3f * k);
    tx.add(foot, 0.5f * W - 0.5f * tx.width(foot, fs), H - 17.0f * k, fs, FOOT);
}
