#include "vk_app.h"
#include <cmath>

namespace {

const double SOFT2 = 1.0;
const size_t MAX_PARTS = 1400;
const double EMIT_RATE = 14.0;
const double DISC_R = 18.0;
const double ACCRETE_R = 3.4;
const double MAX_AGE = 110.0;

float rnd(SandboxState& S) {
    S.rng = S.rng * 1664525u + 1013904223u;
    return (float)(S.rng >> 8) / 16777216.0f;
}

}

void sbPartStep(SandboxState& S, double h) {
    const SbBody* G = nullptr;
    const SbBody* W = nullptr;
    const SbBody* heavy[8];
    int nh = 0;
    for (const SbBody& b : S.bodies) {
        if (b.role == 1) G = &b;
        if (b.role == 2) W = &b;
        if (b.m >= 100.0 && nh < 8) heavy[nh++] = &b;
    }
    if (!G || !W) {
        S.parts.clear();
        S.emitAcc = 0;
        return;
    }
    S.emitAcc += EMIT_RATE * h;
    while (S.emitAcc >= 1.0 && S.parts.size() < MAX_PARTS) {
        S.emitAcc -= 1.0;
        double dx = W->x - G->x, dz = W->z - G->z, d = std::sqrt(dx * dx + dz * dz) + 1e-9;
        double ux = dx / d, uz = dz / d;
        double M = G->m + W->m, om = std::sqrt(M / (d * d * d));
        double lx = G->r + 3.0;
        double cx = (G->m * G->x + W->m * W->x) / M, cz = (G->m * G->z + W->m * W->z) / M;
        SbPart p;
        p.x = G->x + ux * lx + (rnd(S) - 0.5) * 1.5;
        p.z = G->z + uz * lx + (rnd(S) - 0.5) * 1.5;
        p.y = (rnd(S) - 0.5) * 1.2;
        double rx = p.x - cx, rz = p.z - cz;
        double push = 3.5 + 1.5 * rnd(S);
        p.vx = -om * rz + ux * push + (rnd(S) - 0.5) * 1.0;
        p.vz = om * rx + uz * push + (rnd(S) - 0.5) * 1.0;
        p.vy = (rnd(S) - 0.5) * 0.3;
        p.age = 0;
        p.disc = false;
        S.parts.push_back(p);
    }
    size_t keep = 0;
    for (size_t i = 0; i < S.parts.size(); i++) {
        SbPart p = S.parts[i];
        double ax = 0, ay = 0, az = 0;
        for (int k = 0; k < nh; k++) {
            double dx = heavy[k]->x - p.x, dy = heavy[k]->y - p.y, dz = heavy[k]->z - p.z;
            double d2 = dx * dx + dy * dy + dz * dz + SOFT2, inv = (heavy[k]->role == 1 ? 0.3 : 1.0) * heavy[k]->m / (d2 * std::sqrt(d2));
            ax += dx * inv; ay += dy * inv; az += dz * inv;
        }
        p.vx += h * ax; p.vy += h * ay; p.vz += h * az;
        p.x += h * p.vx; p.y += h * p.vy; p.z += h * p.vz;
        p.age += h;
        double wx = p.x - W->x, wy = p.y - W->y, wz = p.z - W->z, wr = std::sqrt(wx * wx + wy * wy + wz * wz);
        double gx = p.x - G->x, gy = p.y - G->y, gz = p.z - G->z;
        bool dead = p.age > MAX_AGE || wr < ACCRETE_R || (p.age > 1.0 && gx * gx + gy * gy + gz * gz < G->r * G->r) || wr > 60.0;
        if (!dead && wr < DISC_R) {
            double rvx = p.vx - W->vx, rvy = p.vy - W->vy, rvz = p.vz - W->vz;
            double hx = wy * rvz - wz * rvy, hy = wz * rvx - wx * rvz, hz = wx * rvy - wy * rvx;
            double hl = std::sqrt(hx * hx + hy * hy + hz * hz);
            if (hl < 1e-6) dead = true;
            else {
                double tx = hy * wz - hz * wy, tz = hx * wy - hy * wx, ty = hz * wx - hx * wz;
                double tl = std::sqrt(tx * tx + ty * ty + tz * tz) + 1e-9;
                double sp = std::sqrt(W->m / wr) * 0.9, k = std::min(1.0, 1.2 * h);
                p.vx += k * (W->vx + tx / tl * sp - p.vx);
                p.vy += k * (W->vy + ty / tl * sp - p.vy);
                p.vz += k * (W->vz + tz / tl * sp - p.vz);
                p.vy -= k * 1.5 * (p.vy - W->vy);
                p.y -= k * 1.5 * (p.y - W->y);
                p.disc = true;
            }
        }
        if (!dead) S.parts[keep++] = p;
    }
    S.parts.resize(keep);
}
