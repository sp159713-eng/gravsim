#include "lagrange_pen.h"
#include <algorithm>

namespace {

const float EARTH_C[3] = {0.35f, 0.65f, 1.0f};
const float SUN_C[3] = {1.0f, 0.82f, 0.3f};
const float UNSTABLE_C[3] = {1.0f, 0.55f, 0.25f};
const float STABLE_C[3] = {0.4f, 1.0f, 0.5f};
const float WHITE[3] = {0.82f, 0.86f, 0.94f};
const float DIM[3] = {0.5f, 0.56f, 0.7f};
const float TITLE[3] = {0.6f, 0.8f, 1.0f};
const float ROCK_C[3] = {0.95f, 0.8f, 0.55f};

void loop(LgPen& pen, float cx, float cy, const LgHalo& h, float t, const float col[3], float k) {
    float lx = 0.0f, ly = 0.0f;
    for (int i = 0; i <= 40; i++) {
        LgHalo s = h;
        s.rate = 1.0f;
        s.phase = 0.0f;
        float dx, dy;
        lgHaloPoint(s, 6.2831853f * (float)i / 40.0f, dx, dy);
        if (i > 0) pen.line(lx, ly, cx + dx, cy - dy, col, 0.6f);
        lx = cx + dx;
        ly = cy - dy;
    }
    float dx, dy;
    lgHaloPoint(h, t, dx, dy);
    pen.dot(cx + dx, cy - dy, 2.5f * k, col);
}

}

void lgEmitInset(AppContext& app, LgPen& pen, float W, float H) {
    LagrangeState& S = app.lg;
    if (S.jupiter) return;
    float k = H / 720.0f;
    int sc1 = std::max(1, (int)std::lround(k));
    LgRect r = lgInsetRect(W, H);
    float w = r.x1 - r.x0, h = r.y1 - r.y0;
    pen.rect(r.x0, r.y0, r.x1, r.y1, DIM);
    pen.text("REAL SCALE: L1 AND L2", r.x0 + 8.0f * k, r.y0 + 7.0f * k, sc1, TITLE);
    float cx = r.x0 + 0.5f * w, cy = r.y0 + 0.5f * h + 6.0f * k;
    float km = 0.5f * w * 0.76f / 1.5f;
    pen.circle(cx, cy, 0.384f * km, DIM, 40, 0.8f);
    for (float q = 1.0f; q <= 4.0f * k; q += 1.5f) pen.circle(cx, cy, q, EARTH_C, 14);
    float lx = cx - 1.5f * km, rx = cx + 1.5f * km;
    pen.line(lx, cy, rx, cy, DIM, 0.35f);
    for (int i = 0; i < 2; i++) {
        float px = i == 0 ? lx : rx;
        pen.circle(px, cy, 5.0f * k, UNSTABLE_C, 16);
        pen.dot(px, cy, 2.0f * k, UNSTABLE_C);
    }
    pen.text("L1", lx - 6.0f * (float)sc1 * 1.0f, cy + 10.0f * k, sc1, UNSTABLE_C);
    pen.text("L2", rx - 6.0f * (float)sc1 * 1.0f, cy + 10.0f * k, sc1, UNSTABLE_C);
    float t = (float)S.t;
    LgHalo jw = {0.13f * km, 0.45f * km, 0.0f, 2.0f, 0.0f};
    loop(pen, rx, cy, jw, t, STABLE_C, k);
    pen.text("JWST", rx + 8.0f * k, cy - 0.45f * km - 10.0f * k, sc1, STABLE_C);
    LgHalo so = {0.12f * km, 0.42f * km, 0.0f, 2.0f, 1.5f}, ds = {0.07f * km, 0.2f * km, 0.0f, 2.0f, 3.5f};
    loop(pen, lx, cy, so, t, ROCK_C, k);
    loop(pen, lx, cy, ds, t, EARTH_C, k);
    pen.text("SOHO", lx - 8.0f * k, cy - 0.45f * km - 10.0f * k, sc1, ROCK_C);
    pen.text("DSCOVR", lx - 10.0f * k, cy + 0.26f * km + 8.0f * k, sc1, EARTH_C);
    pen.text("EARTH", cx - 15.0f * (float)sc1, cy + 7.0f * k, sc1, EARTH_C);
    pen.text("MOON ORBIT", cx - 30.0f * (float)sc1, cy - 0.384f * km - 9.0f * k, sc1, DIM);
    pen.text("< SUN", r.x0 + 8.0f * k, r.y0 + h - 26.0f * k, sc1, SUN_C);
    const char* cap = "1.5 MILLION KM = 4X THE MOON'S DISTANCE";
    pen.text(cap, r.x0 + 0.5f * (w - pen.width(cap, sc1)), r.y1 - 13.0f * k, sc1, WHITE);
}

void lgEmitCard(AppContext& app, LgPen& pen, float W, float H) {
    LagrangeState& S = app.lg;
    int idx = S.hover >= 0 ? S.hover : S.selected;
    if (idx < 0) return;
    float k = H / 720.0f;
    const char* names[5] = {"L1 - BETWEEN THE SUN AND THE PLANET", "L2 - JUST BEYOND THE PLANET", "L3 - FAR SIDE OF THE SUN", "L4 - 60 DEGREES AHEAD OF THE PLANET", "L5 - 60 DEGREES BEHIND THE PLANET"};
    std::vector<std::pair<std::string, const float*>> rows;
    rows.push_back({names[idx], WHITE});
    if (idx == 1) rows.push_back({S.jupiter ? "NO SPACECRAFT PARKED HERE" : "JWST PARKS HERE - EARTH, MOON AND SUN ALL BEHIND ITS SUNSHIELD", TITLE});
    if (idx == 0) rows.push_back({S.jupiter ? "NO SPACECRAFT PARKED HERE" : "SOHO WATCHES THE SUN NONSTOP", TITLE});
    if (idx < 3) rows.push_back({"UNSTABLE - SPACECRAFT FIRE THRUSTERS TO STAY", UNSTABLE_C});
    else {
        rows.push_back({"STABLE - ASTEROIDS COLLECT HERE", STABLE_C});
        if (S.jupiter) rows.push_back({"JUPITER'S TROJANS: OVER 10,000 KNOWN", ROCK_C});
        else if (idx == 3) rows.push_back({"EARTH'S OWN TROJAN 2010 TK7 LIBRATES HERE", ROCK_C});
    }
    size_t longest = 0;
    for (auto& r : rows) longest = std::max(longest, r.first.size());
    float maxW = W - 340.0f * k - 40.0f * k;
    int sc = fitScale(std::string(longest, 'X').c_str(), maxW, std::max(1, (int)std::lround(2.0f * k)));
    float pad = 8.0f * k, row = (8.0f * (float)sc + 5.0f * k);
    float w = (float)longest * 6.0f * (float)sc + 2.0f * pad, h = row * (float)rows.size() + 2.0f * pad - 3.0f * k;
    float x0 = 14.0f * k, y1 = H - 56.0f * k, y0 = y1 - h;
    pen.rect(x0, y0, x0 + w, y1, idx < 3 ? UNSTABLE_C : STABLE_C);
    for (size_t i = 0; i < rows.size(); i++)
        pen.text(rows[i].first, x0 + pad, y0 + pad + row * (float)i, sc, rows[i].second);
}
