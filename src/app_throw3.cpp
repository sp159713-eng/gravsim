#include "throwint.h"
#include "massctl.h"
#include <algorithm>
#include <cmath>

namespace {

const double PI = 3.14159265358979;

struct Pt {
    double p[3], v[3], m;
};

bool galaxyScene(const AppContext& app) { return app.scene == Scene::GALAXY; }

}

Primary primaryOf(const AppContext& app) {
    Primary q{{0, 0, 0}, {0, 0, 0}, 1000.0};
    const Particle* r = nullptr;
    if (galaxyScene(app)) {
        if (app.readValid[app.currentFrame]) r = static_cast<const Particle*>(app.readMapped[app.currentFrame]);
    } else if (app.displayValid) {
        r = app.displayBodies;
    }
    if (r && r[0].pos[3] > 0.0f) {
        for (int a = 0; a < 3; a++) {
            q.p[a] = r[0].pos[a];
            q.v[a] = r[0].vel[a];
        }
        q.m = r[0].pos[3];
    }
    return q;
}

double distEye(const AppContext& app, const double p[3]) {
    float eye[3];
    cameraEye(app.camera, eye);
    double d = 0;
    for (int a = 0; a < 3; a++) d += (p[a] - eye[a]) * (p[a] - eye[a]);
    return std::sqrt(d);
}

void throwPreview(AppContext& app, const double v[3]) {
    ThrowState& T = app.th;
    T.preview.clear();
    bool gal = galaxyScene(app);
    std::vector<Pt> pts;
    if (gal) {
        Primary P = primaryOf(app);
        pts.push_back({{P.p[0], P.p[1], P.p[2]}, {P.v[0], P.v[1], P.v[2]}, P.m});
    } else if (app.displayValid) {
        for (int i = 0; i < 9; i++) {
            const Particle& q = app.displayBodies[i];
            pts.push_back({{q.pos[0], q.pos[1], q.pos[2]}, {q.vel[0], q.vel[1], q.vel[2]}, (double)q.pos[3]});
        }
    } else {
        return;
    }
    size_t planets = gal ? 1 : pts.size();
    for (const ThrowBody& b : T.bodies) pts.push_back({{b.p[0], b.p[1], b.p[2]}, {b.v[0], b.v[1], b.v[2]}, b.m});
    double mt = throwDefMass(gal, T.type) * massMult(T.mk);
    if (gal && T.type == 3) mt += DWARF_STARS;
    size_t me = pts.size();
    pts.push_back({{T.sx, T.sy, T.sz}, {v[0], v[1], v[2]}, mt});
    double G = app.simG, soft2 = (double)app.simSoft * app.simSoft, bs2 = soft2 * BODY_SOFT_RATIO * BODY_SOFT_RATIO;
    const Pt& c = pts[0];
    double r0 = std::sqrt((T.sx - c.p[0]) * (T.sx - c.p[0]) + (T.sy - c.p[1]) * (T.sy - c.p[1]) + (T.sz - c.p[2]) * (T.sz - c.p[2]));
    r0 = std::max(r0, 1e-4);
    double horizon = 2.0 * PI * std::sqrt(r0 * r0 * r0 / (G * c.m)) * 1.1;
    double hitR = gal ? 0.06 * std::cbrt(c.m) + 0.05 : bodyRadius(app.trueScale, 0);
    double pathLimit = 5.0 * distEye(app, pts[me].p), travelled = 0.0;
    size_t n = pts.size();
    int steps = 1500;
    double h = horizon / steps;
    std::vector<double> ax(n), ay(n), az(n);
    auto accel = [&]() {
        std::fill(ax.begin(), ax.end(), 0.0);
        std::fill(ay.begin(), ay.end(), 0.0);
        std::fill(az.begin(), az.end(), 0.0);
        for (size_t i = 0; i < n; i++)
            for (size_t j = i + 1; j < n; j++) {
                double dx = pts[j].p[0] - pts[i].p[0], dy = pts[j].p[1] - pts[i].p[1], dz = pts[j].p[2] - pts[i].p[2];
                double s2 = (i < planets && j < planets && !gal) ? bs2 : soft2;
                double d2 = dx * dx + dy * dy + dz * dz + s2;
                double inv = G / (d2 * std::sqrt(d2));
                ax[i] += pts[j].m * dx * inv; ay[i] += pts[j].m * dy * inv; az[i] += pts[j].m * dz * inv;
                ax[j] -= pts[i].m * dx * inv; ay[j] -= pts[i].m * dy * inv; az[j] -= pts[i].m * dz * inv;
            }
    };
    T.preview.push_back({(float)T.sx, (float)T.sy, (float)T.sz});
    accel();
    double last[3] = {T.sx, T.sy, T.sz};
    for (int i = 1; i <= steps; i++) {
        for (size_t k = 0; k < n; k++) {
            pts[k].v[0] += 0.5 * h * ax[k]; pts[k].v[1] += 0.5 * h * ay[k]; pts[k].v[2] += 0.5 * h * az[k];
            for (int a = 0; a < 3; a++) pts[k].p[a] += h * pts[k].v[a];
        }
        accel();
        for (size_t k = 0; k < n; k++) {
            pts[k].v[0] += 0.5 * h * ax[k]; pts[k].v[1] += 0.5 * h * ay[k]; pts[k].v[2] += 0.5 * h * az[k];
        }
        const Pt& q = pts[me];
        double dc = std::sqrt((q.p[0] - pts[0].p[0]) * (q.p[0] - pts[0].p[0]) + (q.p[1] - pts[0].p[1]) * (q.p[1] - pts[0].p[1]) + (q.p[2] - pts[0].p[2]) * (q.p[2] - pts[0].p[2]));
        travelled += std::sqrt((q.p[0] - last[0]) * (q.p[0] - last[0]) + (q.p[1] - last[1]) * (q.p[1] - last[1]) + (q.p[2] - last[2]) * (q.p[2] - last[2]));
        for (int a = 0; a < 3; a++) last[a] = q.p[a];
        bool hit = dc < hitR;
        if (i % 3 == 0 || hit) T.preview.push_back({(float)q.p[0], (float)q.p[1], (float)q.p[2]});
        if (hit || travelled > pathLimit) break;
    }
}

