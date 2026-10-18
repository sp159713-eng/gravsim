#pragma once
#include "vk_app.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

struct SRect {
    float x0, y0, x1, y1;
    bool has(float x, float y) const { return x >= x0 && x <= x1 && y >= y0 && y <= y1; }
};

struct SPen {
    OverlayVertex* tri = nullptr;
    OverlayVertex* txt = nullptr;
    uint32_t nTri = 0, nTxt = 0;
    float W = 1.0f, H = 1.0f;
    int u = 1;
    float k = 1.0f;

    void rect(float x0, float y0, float x1, float y1, const float c0[3]) {
        const float c[3] = {c0[0] * k, c0[1] * k, c0[2] * k};
        rectRaw(x0, y0, x1, y1, c);
    }

    void rectRaw(float x0, float y0, float x1, float y1, const float c[3]) {
        if (nTri + 6 > MAX_VERTS) return;
        float a = -1.0f + 2.0f * x0 / W, b = -1.0f + 2.0f * y0 / H;
        float e = -1.0f + 2.0f * x1 / W, f = -1.0f + 2.0f * y1 / H;
        tri[nTri++] = {a, b, c[0], c[1], c[2]};
        tri[nTri++] = {e, b, c[0], c[1], c[2]};
        tri[nTri++] = {e, f, c[0], c[1], c[2]};
        tri[nTri++] = {a, b, c[0], c[1], c[2]};
        tri[nTri++] = {e, f, c[0], c[1], c[2]};
        tri[nTri++] = {a, f, c[0], c[1], c[2]};
    }

    void rect(const SRect& r, const float c[3]) { rect(r.x0, r.y0, r.x1, r.y1, c); }

    void frame(const SRect& r, float t, const float c[3]) {
        rect(r.x0, r.y0, r.x1, r.y0 + t, c);
        rect(r.x0, r.y1 - t, r.x1, r.y1, c);
        rect(r.x0, r.y0 + t, r.x0 + t, r.y1 - t, c);
        rect(r.x1 - t, r.y0 + t, r.x1, r.y1 - t, c);
    }

    void text(const std::string& s, float x, float y, int scale, const float c0[3]) {
        const float c[3] = {c0[0] * k, c0[1] * k, c0[2] * k};
        float px = 2.0f / W, py = 2.0f / H;
        int last = std::max(0, scale - 2);
        for (int oy = 0; oy <= last; oy += 2)
            for (int ox = 0; ox <= last; ox += 2) {
                int cy = oy == last - 1 ? last : oy, cx = ox == last - 1 ? last : ox;
                emitText(txt, nTxt, s.c_str(), -1.0f + (x + (float)cx) * px, -1.0f + (y + (float)cy) * py, px, py, (float)scale, c[0], c[1], c[2]);
            }
    }

    static float width(const std::string& s, int scale) { return (float)s.size() * 6.0f * (float)scale; }

    void center(const std::string& s, float cx, float y, int scale, const float c[3]) {
        text(s, std::floor(cx - 0.5f * width(s, scale)), y, scale, c);
    }
};

namespace sc {
const float BG[3] = {0.020f, 0.040f, 0.060f};
const float PANEL[3] = {0.030f, 0.060f, 0.095f};
const float EDGE[3] = {0.10f, 0.52f, 0.60f};
const float HEAD[3] = {0.045f, 0.20f, 0.24f};
const float PDA[3] = {0.62f, 1.0f, 0.86f};
const float GOLD[3] = {1.0f, 0.80f, 0.38f};
const float WHITE[3] = {1.0f, 1.0f, 1.0f};
const float DIM[3] = {0.50f, 0.58f, 0.68f};
const float GREEN[3] = {0.45f, 0.95f, 0.58f};
const float CYAN[3] = {0.55f, 0.88f, 1.0f};
const float RED[3] = {1.0f, 0.30f, 0.26f};
const float BTN[3] = {0.07f, 0.13f, 0.24f};
const float BTN_HOT[3] = {0.14f, 0.30f, 0.50f};
const float BTN_OFF[3] = {0.05f, 0.08f, 0.13f};
}

struct Nav {
    int move = 0;
    bool activate = false;
    int hoverIdx = -1;
    int clickIdx = -1;
};

Nav storyReadNav(AppContext& app, const std::vector<SRect>& rects);
int storyApplyNav(const Nav& n, int sel, int count);
std::vector<SRect> storyButtons(float W, float H, int u, int count, float topFrac, float bw, float bh);
void storyNavReset(const AppContext& app);
std::string storyClean(const char* s);
std::vector<std::string> storyWrap(const std::string& s, int maxChars);

void storyDrawHud(const AppContext& app, SPen& p);
void storyDrawPda(const AppContext& app, SPen& p);
void storyDrawTitle(const AppContext& app, SPen& p);
void storyDrawModes(const AppContext& app, SPen& p);
void storyDrawOptions(const AppContext& app, SPen& p);
void storyDrawPause(const AppContext& app, SPen& p);
void storyDrawHistory(const AppContext& app, SPen& p);
int storyUnit(const AppContext& app);
