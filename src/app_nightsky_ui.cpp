#include "vk_app.h"
#include "lagrange_pen.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

const float GOLD[3] = {1.0f, 0.82f, 0.35f};
const float WHITE[3] = {0.92f, 0.94f, 1.0f};
const float DIM[3] = {0.6f, 0.66f, 0.8f};
const float LINEC[3] = {0.28f, 0.5f, 0.95f};
const float NAMEC[3] = {0.5f, 0.68f, 1.0f};

int fit(const LgPen& pen, const std::string& s, int sc) {
    while (sc > 1 && pen.width(s, sc) > 0.94f * pen.W) sc--;
    return sc;
}

void disc(LgPen& pen, float cx, float cy, float r, const float c[3], float f) {
    if (r < 1.1f) {
        pen.line(cx - 0.7f, cy, cx + 0.7f, cy, c, f);
        return;
    }
    int R = (int)std::ceil(r);
    for (int dy = -R; dy <= R; dy++) {
        float h2 = r * r - (float)(dy * dy);
        if (h2 < 0.0f) continue;
        float h = std::sqrt(h2);
        pen.line(cx - h, cy + (float)dy, cx + h + 0.5f, cy + (float)dy, c, f);
    }
}

std::string lightYear(float ly) {
    char b[48];
    if (ly < 10.0f) std::snprintf(b, sizeof b, "%.1f LY", ly);
    else std::snprintf(b, sizeof b, "%d LY", (int)std::lround(ly));
    return b;
}

std::string leftIn(float ly) {
    int y = nsYear() - (int)std::lround(ly);
    char b[64];
    if (y >= 1) std::snprintf(b, sizeof b, "THE LIGHT YOU SEE LEFT IT IN %d", y);
    else std::snprintf(b, sizeof b, "THE LIGHT YOU SEE LEFT IT IN %d BC", 1 - y);
    return b;
}

}

void emitNightSky(AppContext& app, OverlayVertex* out, uint32_t& used) {
    const NightSkyState& S = app.ns;
    if (!S.ready) return;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float k = H / 720.0f;
    LgPen pen{out, used, W, H, {}};
    auto scl = [&](float base) { return std::max(1, (int)std::lround(base * k)); };
    auto px = [&](float nx) { return (nx * 0.5f + 0.5f) * W; };
    auto py = [&](float ny) { return (ny * 0.5f + 0.5f) * H; };
    bool earth = S.view == 0 && S.moveT >= 1.0f;
    float away = 1.0f - S.earthBlend;

    if (S.lineFade > 0.01f) {
        for (const NsGroup& g : S.groups) {
            float sx = 0.0f, ymax = -1e9f;
            int cnt = 0;
            for (const NsLink& l : g.links) {
                const NsCatStar& a = S.cat[l.a];
                const NsCatStar& b = S.cat[l.b];
                if (!a.ok || !b.ok) continue;
                float f = S.lineFade * std::min(a.vis, b.vis) * 0.75f;
                if (f < 0.02f) continue;
                if (std::fabs(a.nx) > 3.0f || std::fabs(a.ny) > 3.0f || std::fabs(b.nx) > 3.0f || std::fabs(b.ny) > 3.0f) continue;
                pen.line(px(a.nx), py(a.ny), px(b.nx), py(b.ny), LINEC, f);
            }
            for (int id : g.stars) {
                const NsCatStar& c = S.cat[id];
                if (!c.ok || c.vis < 0.3f || std::fabs(c.nx) > 1.0f || std::fabs(c.ny) > 1.0f) continue;
                sx += px(c.nx);
                ymax = std::max(ymax, py(c.ny));
                cnt++;
            }
            if (cnt >= 2) {
                int sc = scl(2.0f);
                float tx = sx / (float)cnt - 0.5f * pen.width(g.name, sc);
                float ty = ymax + 24.0f * k;
                if (tx > 6.0f && tx + pen.width(g.name, sc) < W - 6.0f && ty > 4.0f && ty < H - 20.0f)
                    pen.text(g.name, tx, ty, sc, NAMEC);
            }
        }
    }

    for (const NsPoint& p : S.pts) {
        float x = px(p.x), y = py(p.y);
        float m = p.mag;
        float r = k * (0.55f + 0.34f * (6.7f - m));
        r = std::max(0.5f * k, std::min(r, 6.5f * k));
        float inten = std::max(0.22f, std::min(1.0f, 0.28f + 0.13f * (6.7f - m))) * p.vis;
        if (m < 2.6f) disc(pen, x, y, r * 2.1f, p.rgb, 0.16f * inten);
        if (m < 1.0f) {
            float len = r * 3.2f;
            pen.line(x - len, y, x + len, y, p.rgb, 0.35f * inten);
            pen.line(x, y - len, x, y + len, p.rgb, 0.35f * inten);
        }
        disc(pen, x, y, r, p.rgb, inten);
    }

    if (S.sunPt >= 0) {
        const NsPoint& p = S.pts[S.sunPt];
        float x = px(p.x), y = py(p.y);
        pen.circle(x, y, 9.0f * k, GOLD, 20);
        char b[64];
        std::snprintf(b, sizeof b, "THE SUN - MAG %.1f", p.mag);
        pen.text(b, x + 14.0f * k, y - 3.0f * k, scl(2), GOLD);
    }

    if (S.lineFade > 0.3f) {
        for (const NsPoint& p : S.pts) {
            if (p.id < 0 || p.mag > 1.6f || p.vis < 0.5f) continue;
            const std::string& nm = S.cat[p.id].name;
            float x = px(p.x), y = py(p.y);
            int sc = scl(1.5f);
            if (x < 10.0f || x + pen.width(nm, sc) > W - 10.0f || y < 10.0f || y > H - 40.0f) continue;
            pen.text(nm, x + 8.0f * k, y - 4.0f * k, sc, WHITE);
        }
    }

    if (S.earthBlend > 0.5f) {
        const char* lab[4] = {"N", "E", "S", "W"};
        for (int i = 0; i < 4; i++) {
            float az = 1.5707963f * (float)i;
            float alt = 0.045f;
            float dir[3];
            for (int j = 0; j < 3; j++)
                dir[j] = std::cos(alt) * (std::cos(az) * S.N[j] + std::sin(az) * S.E[j]) + std::sin(alt) * S.Z[j];
            float nx, ny;
            if (!nsNdc(S, dir, nx, ny) || std::fabs(nx) > 0.95f || std::fabs(ny) > 0.95f) continue;
            pen.text(lab[i], px(nx) - 3.0f * (float)scl(2), py(ny) - 3.0f * (float)scl(2), scl(2), GOLD);
        }
    }

    char buf[160];
    if (earth) {
        float hrs = S.lst;
        int hh = (int)hrs, mm = (int)((hrs - (float)hh) * 60.0f);
        std::snprintf(buf, sizeof buf, "NIGHT SKY - %s - SKY TIME %02d:%02d", nsPlaceName(S.place), hh, mm);
    } else if (S.moveT < 1.0f) {
        std::snprintf(buf, sizeof buf, "TRAVELLING TO %s - %d%%", nsViewName(S.view), (int)std::lround(S.moveT * 100.0f));
    } else {
        float d = std::sqrt(S.obs[0] * S.obs[0] + S.obs[1] * S.obs[1] + S.obs[2] * S.obs[2]);
        std::snprintf(buf, sizeof buf, "THE NIGHT SKY SEEN FROM %s - %s FROM EARTH", nsViewName(S.view), lightYear(d).c_str());
    }
    pen.text(buf, 16.0f * k, 14.0f * k, fit(pen, buf, scl(2.4f)), GOLD);
    std::string hint = "DRAG LOOK    SCROLL ZOOM    + / -  TIME    L  PLACE    C  CONSTELLATIONS    T  SEE IT FROM ANOTHER STAR    ESC  EXIT";
    pen.text(hint, 16.0f * k, 44.0f * k, fit(pen, hint, scl(1.5f)), DIM);

    if (away > 0.02f) {
        std::string cap = "CONSTELLATIONS ARE AN ILLUSION OF OUR POINT OF VIEW - THE STARS ARE AT VERY DIFFERENT DISTANCES";
        int sc = fit(pen, cap, scl(2.2f));
        float col[3] = {GOLD[0] * away, GOLD[1] * away, GOLD[2] * away};
        pen.text(cap, 0.5f * (W - pen.width(cap, sc)), H - 44.0f * k, sc, col);
        if (!S.showLines) {
            std::string tip = "PRESS C TO DRAW THE CONSTELLATION LINES";
            float c2[3] = {DIM[0] * away, DIM[1] * away, DIM[2] * away};
            pen.text(tip, 0.5f * (W - pen.width(tip, scl(1.5f))), H - 74.0f * k, scl(1.5f), c2);
        }
    }

    if (S.hover >= 0 && S.hover < (int)S.pts.size()) {
        const NsPoint& p = S.pts[S.hover];
        float x = px(p.x), y = py(p.y);
        pen.circle(x, y, 11.0f * k, WHITE, 24, 0.8f);
        std::string name = p.id == -2 ? "THE SUN" : S.cat[p.id].name;
        char b1[64], b2[64];
        std::snprintf(b1, sizeof b1, "MAG %.1f", p.mag);
        std::snprintf(b2, sizeof b2, "DISTANCE %s%s", lightYear(p.dist).c_str(), earth ? "" : " FROM HERE");
        std::string l3 = leftIn(p.dist);
        int s1 = scl(3), s2 = scl(2);
        float w = std::max(pen.width(name, s1), std::max(pen.width(l3, s2), pen.width(b2, s2)));
        float tx = x + 20.0f * k, ty = y + 34.0f * k;
        if (tx + w > W - 10.0f) tx = x - 20.0f * k - w;
        if (ty + 90.0f * k > H) ty = y - 100.0f * k;
        pen.text(name, tx, ty, s1, GOLD);
        pen.text(b1, tx, ty + 28.0f * k, s2, WHITE);
        pen.text(b2, tx, ty + 48.0f * k, s2, WHITE);
        pen.text(l3, tx, ty + 68.0f * k, s2, WHITE);
    }

    uint32_t lineVerts = used;
    pen.flush();
    app.overlayLineVerts[app.currentFrame] = lineVerts;
    app.overlayPointVerts[app.currentFrame] = used - lineVerts;
}
