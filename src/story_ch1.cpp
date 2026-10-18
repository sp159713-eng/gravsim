#include "story.h"
#include "vk_app.h"
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

struct Track {
    double x, y, z, vx, vy, vz;
    double ang, accum, rmin;
    bool thrown;
    bool seen;
};

std::vector<Track> g_tracks;
bool g_baseline = false;
const double STAR_GM = 9000.0;
bool g_rockSpawned = false;

const SbBody* findStar(const SandboxState& S) {
    const SbBody* best = nullptr;
    for (const SbBody& b : S.bodies)
        if (b.sun && (!best || b.m > best->m)) best = &b;
    return best;
}

}

bool g_ch1AutoRock = false;

void ch1Reset() {
    g_rockSpawned = false;
    g_tracks.clear();
    g_baseline = false;
}

void ch1BuildSystem(AppContext& app) {
    ch1Reset();
    sbPresetBegin(app, 0, "UNKNOWN SYSTEM", "RED DWARF AND FOUR WORLDS");
    SandboxState& S = app.sb;
    std::snprintf(S.capName, sizeof S.capName, "UNCHARTED SYSTEM");
    std::snprintf(S.capFact, sizeof S.capFact, "A RED DWARF AND FOUR WORLDS - THE ARK DRIFTS IN ITS GLOW");
    S.capT = 9.0f;
    SbBody star;
    star.m = STAR_GM;
    star.r = 7.5;
    star.type = 2;
    star.sun = true;
    star.name = "RED DWARF";
    star.col[0] = 1.0f; star.col[1] = 0.26f; star.col[2] = 0.10f;
    S.bodies.push_back(star);
    struct P { double r, phi, m, rad; int type; const char* name; float c[3]; };
    const P planets[4] = {
        {26.0, 0.7, 0.8, 1.5, 0, "CINDER", {0.75f, 0.38f, 0.28f}},
        {42.0, 2.9, 1.4, 1.9, 0, "HARBOR", {0.40f, 0.62f, 0.85f}},
        {66.0, 4.6, 30.0, 3.6, 1, "GOLIATH", {0.88f, 0.62f, 0.42f}},
        {96.0, 1.7, 6.0, 2.4, 1, "SHROUD", {0.55f, 0.72f, 0.78f}},
    };
    for (const P& p : planets) {
        SbBody b;
        double sp = std::sqrt(STAR_GM / p.r);
        b.x = p.r * std::cos(p.phi);
        b.z = p.r * std::sin(p.phi);
        b.vx = -std::sin(p.phi) * sp;
        b.vz = std::cos(p.phi) * sp;
        b.m = p.m;
        b.r = p.rad;
        b.type = p.type;
        b.name = p.name;
        for (int c = 0; c < 3; c++) b.col[c] = p.c[c];
        S.bodies.push_back(b);
    }
    app.camera.dist = S.cam[2] = 210.0f;
    app.camera.pitch = S.cam[1] = 0.85f;
}

bool ch1OrbitCheck(AppContext& app, float t) {
    SandboxState& S = app.sb;
    const SbBody* star = findStar(S);
    if (!star || !S.ready) return false;
    if (t < 0.5f) return false;
    if (g_ch1AutoRock && !g_rockSpawned && t > 1.5f) {
        g_rockSpawned = true;
        SbBody rock;
        rock.x = 45.0;
        rock.vz = std::sqrt(STAR_GM / 45.0);
        rock.m = 1.0;
        rock.r = 1.8;
        rock.name = "ROCK";
        app.sb.bodies.push_back(rock);
    }
    std::vector<Track> next;
    next.reserve(S.bodies.size());
    for (Track& tr : g_tracks) tr.seen = false;
    for (const SbBody& b : S.bodies) {
        if (&b == star) continue;
        int best = -1;
        double bd = 1e18;
        double lim = 4.0 + 1.4 * std::sqrt(b.vx * b.vx + b.vy * b.vy + b.vz * b.vz) * 0.1 * (S.speed > 1.0f ? S.speed : 1.0f);
        for (size_t i = 0; i < g_tracks.size(); i++) {
            if (g_tracks[i].seen) continue;
            double dx = b.x - g_tracks[i].x, dy = b.y - g_tracks[i].y, dz = b.z - g_tracks[i].z;
            double d = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (d < bd) {
                bd = d;
                best = (int)i;
            }
        }
        double rx = b.x - star->x, rz = b.z - star->z;
        double ang = std::atan2(rz, rx);
        double rr = std::sqrt(rx * rx + rz * rz);
        Track tr{};
        if (best >= 0 && bd < lim) {
            tr = g_tracks[best];
            g_tracks[best].seen = true;
            double d = ang - tr.ang;
            while (d > 3.14159265358979) d -= 6.28318530717959;
            while (d < -3.14159265358979) d += 6.28318530717959;
            tr.accum += d;
            if (rr < tr.rmin) tr.rmin = rr;
        } else {
            tr.thrown = g_baseline;
            tr.accum = 0.0;
            tr.rmin = rr;
        }
        tr.x = b.x; tr.y = b.y; tr.z = b.z;
        tr.vx = b.vx; tr.vy = b.vy; tr.vz = b.vz;
        tr.ang = ang;
        tr.seen = false;
        next.push_back(tr);
        if (tr.thrown && std::fabs(tr.accum) >= 6.28318530717959 && tr.rmin > star->r * 1.5) {
            double vx = b.vx - star->vx, vy = b.vy - star->vy, vz = b.vz - star->vz;
            double dx = b.x - star->x, dy = b.y - star->y, dz = b.z - star->z;
            double r = std::sqrt(dx * dx + dy * dy + dz * dz);
            double e = 0.5 * (vx * vx + vy * vy + vz * vz) - star->m / r;
            if (e < 0.0 && r < 400.0) return true;
        }
    }
    g_tracks = next;
    g_baseline = true;
    return false;
}
