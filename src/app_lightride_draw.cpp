#include "vk_app.h"
#include "lagrange_pen.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {

const double PLANET_KM[8] = {2440.0, 6052.0, 6371.0, 3390.0, 69911.0, 58232.0, 25362.0, 24622.0};
const float PLANET_C[8][3] = {{0.65f, 0.6f, 0.55f}, {0.95f, 0.85f, 0.6f}, {0.3f, 0.6f, 1.0f}, {0.9f, 0.45f, 0.25f}, {0.9f, 0.75f, 0.55f}, {0.9f, 0.82f, 0.55f}, {0.55f, 0.85f, 0.9f}, {0.3f, 0.45f, 1.0f}};
const char* NAMES[8] = {"MERCURY", "VENUS", "EARTH", "MARS", "JUPITER", "SATURN", "URANUS", "NEPTUNE"};
const float SUN_C[3] = {1.0f, 0.55f, 0.16f};
const float GOLD[3] = {1.0f, 0.85f, 0.4f};
const float WHITE[3] = {0.92f, 0.95f, 1.0f};
const float DIM[3] = {0.5f, 0.56f, 0.68f};
const float ORBIT_C[3] = {0.25f, 0.4f, 0.7f};
const float CAPTION_S = 7.0f;

struct View {
    const LrBasis& b;
    const double* cam;
    float W, H, fpx;
    bool proj(const double p[3], double& z, float& sx, float& sy) const {
        double d[3] = {p[0] - cam[0], p[1] - cam[1], p[2] - cam[2]};
        z = d[0] * b.f[0] + d[1] * b.f[1] + d[2] * b.f[2];
        if (z < 1e-6) return false;
        double x = d[0] * b.r[0] + d[1] * b.r[1] + d[2] * b.r[2];
        double u = d[0] * b.u[0] + d[1] * b.u[1] + d[2] * b.u[2];
        sx = W * 0.5f + (float)(x / z) * fpx;
        sy = H * 0.5f - (float)(u / z) * fpx;
        return true;
    }
};

void fillDisc(LgPen& pen, float cx, float cy, float r, const float c[3], float limb, float bright) {
    int y0 = std::max(0, (int)std::floor(cy - r)), y1 = std::min((int)pen.H - 1, (int)std::ceil(cy + r));
    for (int y = y0; y <= y1; y++) {
        float dy = ((float)y + 0.5f - cy) / r;
        if (std::fabs(dy) >= 1.0f) continue;
        float half = r * std::sqrt(1.0f - dy * dy);
        float xa = std::max(0.0f, cx - half), xb = std::min(pen.W, cx + half);
        if (xb <= xa) continue;
        const int N = 6;
        for (int s = 0; s < N; s++) {
            float a = xa + (xb - xa) * (float)s / N, bx = xa + (xb - xa) * (float)(s + 1) / N;
            float mx = 0.5f * (a + bx);
            float dx = (mx - cx) / r;
            float rho2 = std::min(1.0f, dx * dx + dy * dy);
            float mu = std::sqrt(1.0f - rho2);
            float f = bright * (1.0f - limb * (1.0f - mu));
            pen.line(a, (float)y + 0.5f, bx, (float)y + 0.5f, c, f);
        }
    }
}

uint32_t mix(uint32_t h) {
    h ^= h >> 16;
    h *= 0x7feb352dU;
    h ^= h >> 15;
    h *= 0x846ca68bU;
    h ^= h >> 16;
    return h;
}

float unit(uint32_t h) { return (float)(h & 0xffffff) / 16777216.0f; }

void centred(LgPen& pen, const std::string& s, float cy, int sc, const float c[3], float f) {
    float col[3] = {c[0] * f, c[1] * f, c[2] * f};
    pen.text(s, 0.5f * (pen.W - pen.width(s, sc)), cy, sc, col);
}

void wrapText(const std::string& s, size_t maxChars, std::vector<std::string>& lines) {
    std::string cur, word;
    auto push = [&]() {
        if (!cur.empty()) lines.push_back(cur);
        cur.clear();
    };
    for (size_t i = 0; i <= s.size(); i++) {
        if (i == s.size() || s[i] == ' ') {
            if (!word.empty()) {
                if (!cur.empty() && cur.size() + 1 + word.size() > maxChars) push();
                if (!cur.empty()) cur += ' ';
                cur += word;
                word.clear();
            }
        } else word += s[i];
    }
    push();
}

void drawDust(LgPen& pen, const View& v, const LightRideState& S) {
    const double L = 0.15;
    long long cx = (long long)std::floor(S.pos[0] / L), cy = (long long)std::floor(S.pos[1] / L), cz = (long long)std::floor(S.pos[2] / L);
    double len = std::min(S.warp * LR_C_AU_S * 0.04, 0.12);
    float col[3] = {0.55f, 0.7f, 1.0f};
    for (int ix = -3; ix <= 3; ix++)
        for (int iy = -3; iy <= 3; iy++)
            for (int iz = -3; iz <= 3; iz++)
                for (int j = 0; j < 2; j++) {
                    long long gx = cx + ix, gy = cy + iy, gz = cz + iz;
                    uint32_t h = mix((uint32_t)(gx * 73856093LL) ^ (uint32_t)(gy * 19349663LL) ^ (uint32_t)(gz * 83492791LL) ^ (uint32_t)(j * 2654435761U));
                    double p[3] = {((double)gx + unit(h)) * L, ((double)gy + unit(mix(h + 1))) * L, ((double)gz + unit(mix(h + 2))) * L};
                    double z;
                    float x0, y0, x1, y1;
                    if (!v.proj(p, z, x0, y0) || z > 0.45 || z < 0.004) continue;
                    double q[3] = {p[0] + S.fwd[0] * len, p[1] + S.fwd[1] * len, p[2] + S.fwd[2] * len};
                    double z2;
                    if (!v.proj(q, z2, x1, y1) || z2 < 0.002) {
                        x1 = x0;
                        y1 = y0;
                    }
                    if (x0 < -50 || x0 > pen.W + 50 || y0 < -50 || y0 > pen.H + 50) continue;
                    float f = 0.2f * (float)(1.0 - z / 0.45);
                    pen.line(x0, y0, x1, y1, col, f);
                }
}

}

void drawLightRide(AppContext& app, VkCommandBuffer cmd, float aspect) {
    float vp[16];
    cameraViewProj(app.camera, aspect, vp);
    if (app.showSky) drawSky(app, cmd, vp);
}

void emitLightRide(AppContext& app, OverlayVertex* out, uint32_t& used) {
    LightRideState& S = app.lr;
    if (!S.ready) return;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float k = H / 1080.0f;
    LgPen pen{out, used, W, H, {}};
    LrBasis b;
    lrMakeBasis(S, b);
    View v{b, S.pos, W, H, 0.8660254f * H};
    auto scl = [&](float base) { return std::max(1, (int)std::lround(base * k)); };

    double sunRel[3] = {-S.pos[0], -S.pos[1], -S.pos[2]};
    double sunD = std::sqrt(sunRel[0] * sunRel[0] + sunRel[1] * sunRel[1] + sunRel[2] * sunRel[2]);
    const double sunR = 696340.0 / LR_KM_AU;
    double sunZ = sunRel[0] * b.f[0] + sunRel[1] * b.f[1] + sunRel[2] * b.f[2];

    double pp[8][3];
    for (int i = 0; i < 8; i++) lrPlanetPos(S, i, S.t, pp[i]);

    for (int i = 0; i < 8; i++) {
        double rad = std::hypot(pp[i][0], pp[i][2]);
        const int N = 160;
        bool have = false;
        float lx = 0, ly = 0;
        double lz = 0;
        for (int s = 0; s <= N; s++) {
            double a = 6.283185307 * s / N;
            double p[3] = {rad * std::cos(a), 0.0, rad * std::sin(a)};
            double z;
            float sx, sy;
            bool ok = v.proj(p, z, sx, sy) && z > 0.02 && std::fabs(sx) < 20000 && std::fabs(sy) < 20000;
            if (ok && have) pen.line(lx, ly, sx, sy, ORBIT_C, 0.4f);
            have = ok;
            lx = sx;
            ly = sy;
            lz = z;
        }
        (void)lz;
    }

    drawDust(pen, v, S);

    if (sunD > sunR * 1.0001 && sunZ > 0.0) {
        float cx = W * 0.5f + (float)((sunRel[0] * b.r[0] + sunRel[1] * b.r[1] + sunRel[2] * b.r[2]) / sunZ) * v.fpx;
        float cy = H * 0.5f - (float)((sunRel[0] * b.u[0] + sunRel[1] * b.u[1] + sunRel[2] * b.u[2]) / sunZ) * v.fpx;
        double al = std::asin(sunR / sunD);
        float r = (float)(std::tan(al) * v.fpx);
        r = std::min(r, 6000.0f);
        if (r < 1500.0f) {
            for (int i = 1; i <= 10; i++) pen.circle(cx, cy, r * (1.0f + 0.04f * (float)i) + 2.0f, SUN_C, 64, 0.45f / (float)i);
        }
        fillDisc(pen, cx, cy, std::max(r, 2.0f), SUN_C, 0.5f, 0.8f);
        std::string sl = "THE SUN";
        if (cy - r - 20 * k > 10 && cx > 0 && cx < W) pen.text(sl, cx - pen.width(sl, scl(2)) * 0.5f, cy - r - 24.0f * k, scl(2), GOLD);
    }

    int shown = 0;
    for (int i = 0; i < 8; i++) {
        double rel[3] = {pp[i][0] - S.pos[0], pp[i][1] - S.pos[1], pp[i][2] - S.pos[2]};
        double d = std::sqrt(rel[0] * rel[0] + rel[1] * rel[1] + rel[2] * rel[2]);
        double z;
        float sx, sy;
        bool vis = v.proj(pp[i], z, sx, sy);
        bool on = vis && sx > 0 && sx < W && sy > 0 && sy < H;
        bool target = S.capIdx >= 0 && S.ms[S.capIdx].planet == i && S.capAge < CAPTION_S;
        if (on) {
            float r = (float)(PLANET_KM[i] / LR_KM_AU / z * v.fpx);
            if (r > 1.5f) fillDisc(pen, sx, sy, r, PLANET_C[i], 0.35f, 1.0f);
            else {
                pen.dot(sx, sy, 3.0f * k + 1.0f, PLANET_C[i]);
                pen.circle(sx, sy, 5.0f * k + 2.0f, PLANET_C[i], 16, 0.6f);
            }
            float ly = sy + std::max(r, 5.0f * k) + 12.0f * k;
            if (ly < 250.0f * k && std::fabs(sx - W * 0.5f) < 330.0f * k) continue;
            std::string nm = NAMES[i];
            pen.text(nm, sx - pen.width(nm, scl(2)) * 0.5f, ly, scl(2), WHITE);
            char kmb[48], line[80];
            lrFormatKm(d * LR_KM_AU, kmb, sizeof kmb);
            std::snprintf(line, sizeof line, "%s KM", kmb);
            pen.text(line, sx - pen.width(line, scl(1.5f)) * 0.5f, ly + 18.0f * k, scl(1.5f), DIM);
            shown++;
        } else if (target || d < 12.0) {
            double x = rel[0] * b.r[0] + rel[1] * b.r[1] + rel[2] * b.r[2];
            double u = rel[0] * b.u[0] + rel[1] * b.u[1] + rel[2] * b.u[2];
            double l = std::hypot(x, u);
            if (l < 1e-12) continue;
            float mx = W * 0.5f - 90.0f * k, my = H * 0.5f - 120.0f * k;
            float s = (float)std::min(mx / std::max(1e-9, std::fabs(x / l)), my / std::max(1e-9, std::fabs(u / l)));
            float ex = W * 0.5f + (float)(x / l) * s, ey = H * 0.5f - (float)(u / l) * s;
            if (std::fabs(ex - W * 0.5f) < 330.0f * k) ey = std::max(ey, 270.0f * k);
            std::string nm = std::string(x < 0 ? "< " : "") + NAMES[i] + (x >= 0 ? " >" : "");
            pen.text(nm, x < 0 ? ex : ex - pen.width(nm, scl(2)), ey, scl(2), PLANET_C[i]);
        }
    }

    char buf[96], buf2[96];
    float capIn = 1.0f;
    double distKm = S.t * LR_C_KMS, distAu = S.t * LR_C_AU_S;

    centred(pen, "ELAPSED LIGHT-TIME", 16.0f * k, scl(2), DIM, 1.0f);
    lrFormatClock(S.t, buf, sizeof buf);
    centred(pen, buf, 40.0f * k, scl(9), WHITE, 1.0f);
    lrFormatKm(distKm, buf2, sizeof buf2);
    std::snprintf(buf, sizeof buf, "%s KM", buf2);
    centred(pen, buf, 132.0f * k, scl(4), GOLD, 1.0f);
    if (distAu < 1000.0) std::snprintf(buf, sizeof buf, "%.3f AU", distAu);
    else if (distAu < 5000.0) std::snprintf(buf, sizeof buf, "%.1f AU", distAu);
    else std::snprintf(buf, sizeof buf, "%.0f AU   =   %.3f LIGHT-YEARS", distAu, distAu / 63241.077);
    centred(pen, buf, 178.0f * k, scl(3), WHITE, 1.0f);
    centred(pen, "SPEED OF LIGHT 299,792 KM/S", 226.0f * k, scl(4), GOLD, 1.0f);
    (void)capIn;

    if (S.warp <= 1.0) std::snprintf(buf, sizeof buf, "TIME x1  (REAL SPEED)");
    else if (S.warp < 1e6) std::snprintf(buf, sizeof buf, "TIME x%.0f", S.warp);
    else std::snprintf(buf, sizeof buf, "TIME x%.2e", S.warp);
    std::string w = std::string(buf) + (S.autoWarp ? "  AUTO" : "") + (S.paused || S.frozen ? "  PAUSED" : "");
    pen.text(w, 16.0f * k, H - 66.0f * k, scl(2.5f), GOLD);
    pen.text("+ / -  WARP    TAB  NEXT MILESTONE    A  AUTO    V  LOOK BACK    DRAG  LOOK    ESC  EXIT", 16.0f * k, H - 28.0f * k, scl(1.5f), DIM);

    float capY = H - 150.0f * k;
    if (S.warp <= 1.0 && S.t < 60.0 && !S.paused) {
        float f = std::min(1.0f, (float)(60.0 - S.t) / 4.0f);
        centred(pen, "THIS IS REAL SPEED - LIGHT IS SLOW AT SPACE SCALE", capY - 70.0f * k, scl(3), WHITE, f);
    }
    if (S.capIdx >= 0 && S.capAge < CAPTION_S) {
        const LrMilestone& m = S.ms[S.capIdx];
        float f = std::max(0.0f, std::min(1.0f, std::min((float)S.capAge / 0.4f, (CAPTION_S - (float)S.capAge) / 1.2f)));
        int hs = scl(4);
        centred(pen, m.head, capY - 10.0f * k, hs, GOLD, f);
        std::vector<std::string> lines;
        wrapText(m.body, (size_t)(W * 0.85f / (6.0f * (float)scl(3))), lines);
        float y = capY + 40.0f * k;
        for (const std::string& ln : lines) {
            centred(pen, ln, y, scl(3), WHITE, f);
            y += 34.0f * k;
        }
    }
    uint32_t lineVerts = used;
    pen.flush();
    app.overlayLineVerts[app.currentFrame] = lineVerts;
    app.overlayPointVerts[app.currentFrame] = used - lineVerts;
}
