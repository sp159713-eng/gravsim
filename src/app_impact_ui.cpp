#include "vk_app.h"
#include "lagrange_pen.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace {

const float GOLD[3] = {1.0f, 0.82f, 0.35f};
const float WHITE[3] = {0.92f, 0.94f, 1.0f};
const float DIM[3] = {0.58f, 0.64f, 0.78f};
const float HEAD[3] = {0.5f, 0.68f, 1.0f};
const float RED[3] = {1.0f, 0.42f, 0.22f};
const float YEL[3] = {1.0f, 0.9f, 0.4f};
const float CYAN[3] = {0.45f, 0.9f, 1.0f};
const float SCARLET[3] = {1.0f, 0.3f, 0.25f};

int fit(const LgPen& pen, const std::string& s, int sc, float maxW) {
    while (sc > 1 && pen.width(s, sc) > maxW) sc--;
    return sc;
}

std::string diaText(float m) {
    char b[32];
    if (m < 1000.0f) std::snprintf(b, sizeof b, "%.0f M", m);
    else std::snprintf(b, sizeof b, "%.2f KM", m / 1000.0f);
    return b;
}

std::string targetText(const ImpactState& S) {
    char b[64];
    std::snprintf(b, sizeof b, "%.1f%c %.1f%c", std::fabs(S.lat), S.lat >= 0.0f ? 'N' : 'S', std::fabs(S.lon), S.lon >= 0.0f ? 'E' : 'W');
    bool delhi = std::fabs(S.lat - 28.61f) < 2.5f && std::fabs(S.lon - 77.21f) < 2.5f;
    return std::string(delhi ? "NEW DELHI " : "TARGET ") + b;
}

}

void emitImpact(AppContext& app, OverlayVertex* out, uint32_t& used) {
    ImpactState& S = app.im;
    if (!S.ready) return;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float k = H / 720.0f;
    LgPen pen{out, used, W, H, {}};
    auto scl = [&](float base) { return std::max(1, (int)std::lround(base * k)); };
    const ImpactResult& r = S.r;
    float panelX = (S.panelEdge * 0.5f + 0.5f) * W;
    float freeW = panelX - 24.0f * k;
    float tHit = imHitT(S);
    char buf[160];

    std::string title = "IMPACT SIMULATOR";
    pen.text(title, 16.0f * k, 14.0f * k, scl(2.4f), GOLD);
    std::string name = S.preset >= 0 ? IM_PRESETS[S.preset].name : "CUSTOM ASTEROID";
    pen.text(name, 16.0f * k, 40.0f * k, scl(2.0f), WHITE);
    const char* hints[3] = {"[ ] SIZE   ; ' SPEED   , . ANGLE", "TAB TYPE   1-5 PRESETS   SPACE LAUNCH", "CLICK GLOBE TARGET   DRAG TURN   ESC EXIT"};
    for (int i = 0; i < 3; i++) pen.text(hints[i], 16.0f * k, (68.0f + 20.0f * (float)i) * k, fit(pen, hints[i], scl(1.5f), freeW), DIM);

    float x0 = panelX + 14.0f * k, y = 58.0f * k;
    int sc = scl(2.0f);
    float rowH = 21.0f * k;
    float vx = x0 + 11.0f * 6.0f * (float)sc;
    pen.line(panelX, 0.0f, panelX, H, DIM, 0.5f);
    auto head = [&](const char* s) {
        y += 5.0f * k;
        pen.text(s, x0, y, sc, HEAD);
        y += rowH;
    };
    auto row = [&](const char* lab, const std::string& v, const float* c = WHITE) {
        pen.text(lab, x0, y, sc, DIM);
        pen.text(v, vx, y, sc, c);
        y += rowH;
    };
    auto line = [&](const std::string& v, const float* c = WHITE) {
        pen.text(v, x0, y, sc, c);
        y += rowH;
    };

    head(S.miss ? "ASTEROID - IF IT HAD HIT" : "ASTEROID");
    row("DIAMETER", diaText(S.p.diaM) + "  [ ]");
    std::snprintf(buf, sizeof buf, "%.0f KM/S  ; '", S.p.speedKms);
    row("SPEED", buf);
    std::snprintf(buf, sizeof buf, "%s %.0f  TAB", imTypeName(S.p.type), imDensity(S.p.type));
    row("TYPE", buf);
    std::snprintf(buf, sizeof buf, "%.0f DEG  , .", S.p.angleDeg);
    row("ANGLE", buf);
    row("MASS", imNum(r.massKg) + " KG");

    head("ENERGY");
    line(imNum(r.energyMt) + " MEGATONS TNT", GOLD);
    line("= " + imNum(r.hiroshimas) + " HIROSHIMAS", WHITE);
    line("= " + imNum(r.tsars) + " TSAR BOMBAS", WHITE);

    head("RESULT");
    if (r.airburst) {
        std::snprintf(buf, sizeof buf, "AIRBURST AT %s", imKm(r.burstKm).c_str());
        line(buf, YEL);
        line("BREAKS UP, NO CRATER", DIM);
    } else {
        line("GROUND IMPACT", RED);
        row("TRANSIENT", imKm(r.transientKm) + " CRATER");
        row("FINAL", imKm(r.finalKm) + (r.complexCrater ? " COMPLEX" : " SIMPLE"));
    }
    row("FIREBALL", imKm(r.fireballKm) + " RADIUS");
    row("5 PSI", imKm(r.r5Km) + " BUILDINGS", RED);
    row("1 PSI", imKm(r.r1Km) + " WINDOWS", YEL);

    head("HOW OFTEN");
    line(imRecur(r.recurYears), WHITE);

    float cx = 0.5f * freeW + 12.0f * k;
    float capY = H - 60.0f * k;
    std::string c1, c2;
    const float* col1 = GOLD;
    if (S.miss) {
        c1 = S.running && S.t > 4.0f ? "APOPHIS MISSES EARTH" : "APOPHIS - A CLOSE FLYBY, NOT AN IMPACT";
        c2 = "13 APRIL 2029 - ABOUT 32000 KM ABOVE THE SURFACE, INSIDE THE RING OF GEOSTATIONARY SATELLITES";
        if (!S.running) c1 = "PRESS SPACE TO WATCH THE FLYBY";
    } else if (!S.running) {
        c1 = "PRESS SPACE TO LAUNCH";
        c2 = S.preset >= 0 ? IM_PRESETS[S.preset].note : targetText(S);
        col1 = WHITE;
    } else if (tHit < 0.0f) {
        c1 = "INCOMING";
        std::snprintf(buf, sizeof buf, "%s AT %.0f KM/S, %.0f DEG", diaText(S.p.diaM).c_str(), S.p.speedKms, S.p.angleDeg);
        c2 = buf;
        col1 = YEL;
    } else if (tHit < 2.5f) {
        c1 = r.airburst ? "AIRBURST" : "IMPACT";
        std::snprintf(buf, sizeof buf, "%s MEGATONS - FIREBALL %s", imNum(r.energyMt).c_str(), imKm(r.fireballKm).c_str());
        c2 = buf;
        col1 = SCARLET;
    } else if (tHit < 10.0f) {
        c1 = "SHOCKWAVE";
        std::snprintf(buf, sizeof buf, "5 PSI AT %s    1 PSI AT %s", imKm(r.r5Km).c_str(), imKm(r.r1Km).c_str());
        c2 = buf;
        col1 = RED;
    } else {
        c1 = S.preset >= 0 ? IM_PRESETS[S.preset].name : "AFTERMATH";
        c2 = S.preset >= 0 ? IM_PRESETS[S.preset].note : std::string("PRESS SPACE TO REPLAY, OR CLICK THE GLOBE FOR A NEW TARGET");
        col1 = GOLD;
    }
    int s1 = fit(pen, c1, scl(3.0f), freeW), s2 = scl(1.6f);
    pen.text(c1, cx - 0.5f * pen.width(c1, s1), capY - 40.0f * k, s1, col1);
    std::string c3;
    if (pen.width(c2, s2) > freeW) {
        size_t cut = c2.rfind(' ', c2.size() / 2 + 6);
        if (cut == std::string::npos) cut = c2.size() / 2;
        c3 = c2.substr(cut + 1);
        c2 = c2.substr(0, cut);
    }
    pen.text(c2, cx - 0.5f * pen.width(c2, s2), capY - 6.0f * k, fit(pen, c2, s2, freeW), DIM);
    if (!c3.empty()) pen.text(c3, cx - 0.5f * pen.width(c3, s2), capY + 16.0f * k, fit(pen, c3, s2, freeW), DIM);

    if (S.running && !S.miss && r.veil > 0.2f && tHit > 6.0f) {
        std::string d = "A GLOBAL DUST VEIL DARKENS THE SKY";
        int sd = fit(pen, d, scl(2.0f), freeW);
        pen.text(d, cx - 0.5f * pen.width(d, sd), 146.0f * k, sd, YEL);
    }
    if (S.running && !S.miss && r.extinction && tHit > 8.0f) {
        std::string d = "75% OF SPECIES WENT EXTINCT";
        int sd = fit(pen, d, scl(4.0f), freeW);
        float a = std::min(1.0f, (tHit - 8.0f) / 3.0f);
        float c[3] = {SCARLET[0] * a, SCARLET[1] * a, SCARLET[2] * a};
        pen.text(d, cx - 0.5f * pen.width(d, sd), 170.0f * k, sd, c);
    }

    float ringK = S.miss ? 0.0f : (tHit < 0.0f ? 0.55f : std::min(1.0f, std::max(0.0f, (tHit - 6.0f) / 3.0f)));
    if (ringK > 0.05f) {
        const char* labs[2] = {"5 PSI - BUILDINGS", "1 PSI - WINDOWS"};
        float rads[2] = {r.r5Km, r.r1Km};
        const float* cols[2] = {RED, YEL};
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < 18; j++) {
                float bearing = (1.2f + 0.35f * (float)((j % 2 == 0 ? 1 : -1) * ((j + 1) / 2))) ;
                float P[3];
                imSurfacePoint(S, rads[i], bearing, P);
                float dv = P[0] * S.camPos[0] + P[1] * S.camPos[1] + P[2] * S.camPos[2];
                float nx, ny;
                if (dv < 1.0f + 1e-6f || !imProject(S, P, nx, ny)) continue;
                float px = (nx * 0.5f + 0.5f) * W, py = (ny * 0.5f + 0.5f) * H;
                int ls = scl(1.5f);
                float tw = pen.width(labs[i], ls);
                if (px < 10.0f * k || px + tw > panelX - 10.0f * k || py < 140.0f * k || py > H - 135.0f * k) continue;
                float c[3] = {cols[i][0] * ringK, cols[i][1] * ringK, cols[i][2] * ringK};
                pen.text(labs[i], px, py, ls, c);
                break;
            }
        }
    }
    if (S.miss) {
        float M[3] = {S.I[0] * 3.51f, S.I[1] * 3.51f, S.I[2] * 3.51f};
        float nx, ny;
        if (imProject(S, M, nx, ny)) {
            float px = (nx * 0.5f + 0.5f) * W, py = (ny * 0.5f + 0.5f) * H;
            if (px > 10.0f * k && px + 100.0f * k < panelX) pen.text("32000 KM", px + 8.0f * k, py, scl(1.6f), CYAN);
        }
    }
    if (!S.running && !S.miss) {
        float nx, ny;
        if (imProject(S, S.I, nx, ny)) {
            float px = (nx * 0.5f + 0.5f) * W, py = (ny * 0.5f + 0.5f) * H;
            std::string t = targetText(S);
            if (px + 20.0f * k + pen.width(t, scl(1.5f)) < panelX) pen.text(t, px + 20.0f * k, py - 28.0f * k, scl(1.5f), WHITE);
        }
    }

    uint32_t lineVerts = used;
    pen.flush();
    app.overlayLineVerts[app.currentFrame] = lineVerts;
    app.overlayPointVerts[app.currentFrame] = used - lineVerts;
}
