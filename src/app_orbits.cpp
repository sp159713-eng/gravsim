#include "vk_app.h"
#include <cmath>

namespace {

const int SEGS = 128;

bool project(const float m[16], const float p[3], float& x, float& y) {
    float clip[4];
    for (int row = 0; row < 4; row++)
        clip[row] = m[row] * p[0] + m[row + 4] * p[1] + m[row + 8] * p[2] + m[row + 12];
    if (clip[3] <= 1e-9f) return false;
    x = clip[0] / clip[3];
    y = clip[1] / clip[3];
    return true;
}

void cross(const double a[3], const double b[3], double out[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

double norm(const double a[3]) {
    return std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
}

int primaryOf(const AppContext& app, uint32_t i) {
    int par = app.bodyParent[i];
    return par >= 0 && par < (int)BODIES ? par : 0;
}

void solve(const AppContext& app, uint32_t i, std::vector<std::array<float, 3>>& pts) {
    pts.clear();
    int par = primaryOf(app, i);
    const Particle& b = app.displayBodies[i];
    const Particle& p = app.displayBodies[par];
    double mu = (double)app.simG * p.pos[3];
    if (mu <= 0.0) return;
    double r[3], v[3];
    for (int a = 0; a < 3; a++) {
        r[a] = (double)b.pos[a] - p.pos[a];
        v[a] = (double)b.vel[a] - p.vel[a];
    }
    double rl = norm(r), v2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    double h[3];
    cross(r, v, h);
    double hl = norm(h);
    if (rl < 1e-12 || hl < 1e-14) return;
    double vh[3];
    cross(v, h, vh);
    double ev[3];
    for (int a = 0; a < 3; a++) ev[a] = vh[a] / mu - r[a] / rl;
    double e = norm(ev);
    double P[3], W[3], Q[3];
    for (int a = 0; a < 3; a++) {
        P[a] = e > 1e-6 ? ev[a] / e : r[a] / rl;
        W[a] = h[a] / hl;
    }
    cross(W, P, Q);
    double invA = 2.0 / rl - v2 / mu;
    double semiP = hl * hl / mu;
    if (e < 1.0 && invA > 1e-12) {
        double a = 1.0 / invA, bb = a * std::sqrt(1.0 - e * e);
        for (int k = 0; k <= SEGS; k++) {
            double E = 6.283185307179586 * k / SEGS;
            double x = a * (std::cos(E) - e), y = bb * std::sin(E);
            pts.push_back({(float)(x * P[0] + y * Q[0]), (float)(x * P[1] + y * Q[1]), (float)(x * P[2] + y * Q[2])});
        }
        return;
    }
    double c = (semiP / (3.0 * rl) - 1.0) / e;
    if (e <= 1.0 || c >= 1.0) return;
    double lim = std::min(std::acos(std::max(c, -1.0)), std::acos(-1.0 / e) - 0.02);
    for (int k = 0; k <= SEGS; k++) {
        double nu = -lim + 2.0 * lim * k / SEGS;
        double rr = semiP / (1.0 + e * std::cos(nu));
        double x = rr * std::cos(nu), y = rr * std::sin(nu);
        pts.push_back({(float)(x * P[0] + y * Q[0]), (float)(x * P[1] + y * Q[1]), (float)(x * P[2] + y * Q[2])});
    }
}

}

void emitOrbitLines(AppContext& app, OverlayVertex* out, uint32_t& used, const float viewProj[16]) {
    if (!app.showOrbits || !app.displayValid) return;
    uint32_t count = app.n < BODIES ? app.n : BODIES;
    int sig = (app.trueScale ? 1 : 0) + (app.visit.active ? 2 : 0) + 4 * (int)app.visit.star + 1000003 * (int)app.tm.launchDay;
    double now = glfwGetTime();
    if (sig != app.orbitSig || now - app.orbitStamp > 0.25 || app.orbitStamp < 0.0) {
        app.orbitSig = sig;
        app.orbitStamp = now;
        for (uint32_t i = 1; i < count; i++) {
            if (app.bodyActive[i]) solve(app, i, app.orbitPts[i]);
            else app.orbitPts[i].clear();
        }
        app.orbitPts[0].clear();
    }
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    std::vector<float> sx(SEGS + 1), sy(SEGS + 1);
    std::vector<char> ok(SEGS + 1);
    for (uint32_t i = 1; i < count; i++) {
        const auto& pts = app.orbitPts[i];
        if (pts.empty() || !app.bodyActive[i]) continue;
        const float* centre = app.displayBodies[primaryOf(app, i)].pos;
        float lo[2] = {1e30f, 1e30f}, hi[2] = {-1e30f, -1e30f};
        for (size_t k = 0; k < pts.size(); k++) {
            float p[3] = {pts[k][0] + centre[0], pts[k][1] + centre[1], pts[k][2] + centre[2]};
            ok[k] = project(viewProj, p, sx[k], sy[k]);
            if (!ok[k]) continue;
            lo[0] = std::fmin(lo[0], sx[k]); hi[0] = std::fmax(hi[0], sx[k]);
            lo[1] = std::fmin(lo[1], sy[k]); hi[1] = std::fmax(hi[1], sy[k]);
        }
        if ((hi[0] - lo[0]) * W * 0.5f < 6.0f && (hi[1] - lo[1]) * H * 0.5f < 6.0f) continue;
        const float* col = app.bodyColors[i];
        float c[3] = {col[0] * 0.4f, col[1] * 0.4f, col[2] * 0.4f};
        for (size_t k = 1; k < pts.size() && used + 2 <= MAX_VERTS; k++) {
            if (!ok[k - 1] || !ok[k]) continue;
            out[used++] = {sx[k - 1], sy[k - 1], c[0], c[1], c[2]};
            out[used++] = {sx[k], sy[k], c[0], c[1], c[2]};
        }
    }
}
