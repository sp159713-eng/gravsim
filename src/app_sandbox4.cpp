#include "vk_app.h"
#include <algorithm>
#include <cmath>

namespace {

const double SOFT2 = 1.0;
const double PI = 3.14159265358979;
const double SUN_GM_REF = 10000.0;

double vcirc(double gm, double r) {
    return std::sqrt(gm * r * r / std::pow(r * r + SOFT2, 1.5));
}

SbBody star(const char* name, double m, double r, float cr, float cg, float cb) {
    SbBody b;
    b.name = name;
    b.m = m;
    b.r = r;
    b.type = 2;
    b.col[0] = cr; b.col[1] = cg; b.col[2] = cb;
    return b;
}

SbBody planet(const char* name, double m, double r, int type, float cr, float cg, float cb) {
    SbBody b;
    b.name = name;
    b.m = m;
    b.r = r;
    b.type = type;
    b.col[0] = cr; b.col[1] = cg; b.col[2] = cb;
    return b;
}

void orbitAt(SbBody& b, double cx, double cz, double cvx, double cvz, double gm, double r, double phi) {
    double sp = vcirc(gm, r);
    b.x = cx + r * std::cos(phi);
    b.z = cz + r * std::sin(phi);
    b.vx = cvx - std::sin(phi) * sp;
    b.vz = cvz + std::cos(phi) * sp;
}

void zeroMomentum(std::vector<SbBody>& bs) {
    double m = 0, px = 0, py = 0, pz = 0, cx = 0, cy = 0, cz = 0;
    for (const SbBody& b : bs) {
        m += b.m;
        px += b.m * b.vx; py += b.m * b.vy; pz += b.m * b.vz;
        cx += b.m * b.x; cy += b.m * b.y; cz += b.m * b.z;
    }
    for (SbBody& b : bs) {
        b.vx -= px / m; b.vy -= py / m; b.vz -= pz / m;
        b.x -= cx / m; b.y -= cy / m; b.z -= cz / m;
    }
}

float rnd(SandboxState& S) {
    S.rng = S.rng * 1664525u + 1013904223u;
    return (float)(S.rng >> 8) / 16777216.0f;
}

void trappist(SandboxState& S) {
    const double GM = 1500.0, A1 = 7.0;
    const double P[7] = {1.51, 2.42, 4.05, 6.1, 9.2, 12.35, 18.77};
    const char* N[7] = {"B", "C", "D", "E", "F", "G", "H"};
    const float C[7][3] = {{0.78f, 0.5f, 0.35f}, {0.72f, 0.56f, 0.42f}, {0.8f, 0.7f, 0.5f}, {0.4f, 0.75f, 0.6f}, {0.35f, 0.7f, 0.9f}, {0.45f, 0.85f, 0.55f}, {0.75f, 0.85f, 0.95f}};
    SbBody s = star("TRAPPIST-1", GM, 3.5, 1.0f, 0.3f, 0.16f);
    s.sun = true;
    S.bodies.push_back(s);
    for (int i = 0; i < 7; i++) {
        double a = A1 * std::pow(P[i] / P[0], 2.0 / 3.0);
        SbBody b = planet(N[i], 0.08, 0.8 + 0.02 * i, 0, C[i][0], C[i][1], C[i][2]);
        orbitAt(b, 0, 0, 0, 0, GM, a, 0.8 * i + 0.3);
        S.bodies.push_back(b);
    }
}

void kepler16(SandboxState& S) {
    const double mA = 3100.0, mB = 900.0, M = mA + mB, a = 22.0, e = 0.16;
    double rp = a * (1.0 - e), v = std::sqrt(M * (1.0 + e) / (a * (1.0 - e)));
    SbBody A = star("KEPLER-16A", mA, 6.0, 1.0f, 0.68f, 0.33f);
    SbBody B = star("KEPLER-16B", mB, 3.5, 1.0f, 0.32f, 0.2f);
    A.x = -mB / M * rp; A.vz = -mB / M * v;
    B.x = mA / M * rp; B.vz = mA / M * v;
    S.bodies.push_back(A);
    S.bodies.push_back(B);
    SbBody p = planet("KEPLER-16 B", 0.5, 3.0, 1, 0.8f, 0.7f, 0.5f);
    orbitAt(p, 0, 0, 0, 0, M, 69.0, 2.0);
    S.bodies.push_back(p);
}

void massTransfer(SandboxState& S) {
    const double mG = 2200.0, mW = 1800.0, M = mG + mW, d = 60.0;
    double v = std::sqrt(M / d);
    SbBody G = star("RED GIANT", mG, 18.0, 1.0f, 0.38f, 0.16f);
    SbBody W = star("WHITE DWARF", mW, 2.5, 0.8f, 0.9f, 1.0f);
    G.x = -mW / M * d; G.vz = -mW / M * v; G.role = 1;
    W.x = mG / M * d; W.vz = mG / M * v; W.role = 2;
    S.bodies.push_back(G);
    S.bodies.push_back(W);
}

void figure8(SandboxState& S) {
    const double m = 1000.0, L = 40.0, vs = std::sqrt(m / L);
    const double x = 0.97000436, y = -0.24308753, vx3 = -0.93240737, vy3 = -0.86473146;
    const char* N[3] = {"ALPHA", "BETA", "GAMMA"};
    const float C[3][3] = {{1.0f, 0.6f, 0.3f}, {0.4f, 0.8f, 1.0f}, {0.8f, 1.0f, 0.5f}};
    for (int i = 0; i < 3; i++) {
        SbBody b = star(N[i], m, 2.5, C[i][0], C[i][1], C[i][2]);
        if (i == 0) { b.x = -x * L; b.z = -y * L; b.vx = -vx3 / 2 * vs; b.vz = -vy3 / 2 * vs; }
        if (i == 1) { b.x = x * L; b.z = y * L; b.vx = -vx3 / 2 * vs; b.vz = -vy3 / 2 * vs; }
        if (i == 2) { b.vx = vx3 * vs; b.vz = vy3 * vs; }
        S.bodies.push_back(b);
    }
}

void chaotic(SandboxState& S) {
    const double m = 1500.0, R = 24.0;
    const char* N[3] = {"STAR 1", "STAR 2", "STAR 3"};
    const float C[3][3] = {{1.0f, 0.7f, 0.35f}, {0.6f, 0.8f, 1.0f}, {1.0f, 0.45f, 0.4f}};
    double v0 = 0.8 * std::sqrt(m / (1.732 * R));
    for (int i = 0; i < 3; i++) {
        SbBody b = star(N[i], m, 2.2, C[i][0], C[i][1], C[i][2]);
        double phi = 2.0 * PI * i / 3.0 + 0.3 * (rnd(S) - 0.5);
        double r = R * (1.0 + 0.16 * (rnd(S) - 0.5));
        b.x = r * std::cos(phi);
        b.z = r * std::sin(phi);
        b.vx = -std::sin(phi) * v0 + 1.6 * (rnd(S) - 0.5);
        b.vz = std::cos(phi) * v0 + 1.6 * (rnd(S) - 0.5);
        S.bodies.push_back(b);
    }
    zeroMomentum(S.bodies);
}

void intruder(SandboxState& S, SbBody b, double r0, double v0, double rp, double th, double tilt) {
    double E = 0.5 * v0 * v0 - SUN_GM_REF / r0;
    double L = rp * std::sqrt(2.0 * (E + SUN_GM_REF / rp));
    double sa = std::min(0.98, L / (r0 * v0)), ca = std::sqrt(1.0 - sa * sa);
    double px = std::cos(th), pz = std::sin(th), tx = -pz, tz = px;
    double pos[3] = {r0 * px, 0.0, r0 * pz};
    double vel[3] = {v0 * (-ca * px + sa * tx), 0.0, v0 * (-ca * pz + sa * tz)};
    double ct = std::cos(tilt), st = std::sin(tilt);
    b.x = pos[0]; b.y = -pos[2] * st; b.z = pos[2] * ct;
    b.vx = vel[0]; b.vy = -vel[2] * st; b.vz = vel[2] * ct;
    S.bodies.push_back(b);
}

}

void sbPresetBuildMore(AppContext& app, int idx) {
    SandboxState& S = app.sb;
    switch (idx) {
    case 1: trappist(S); break;
    case 2: kepler16(S); break;
    case 3: massTransfer(S); break;
    case 4: figure8(S); break;
    case 5: chaotic(S); break;
    case 6:
        sbSolarBuild(S);
        intruder(S, planet("ROGUE", 6.4, 4.4, 1, 0.75f, 0.55f, 0.95f), 300.0, 12.0, 60.0, 2.6, 0.12);
        break;
    case 7:
        sbSolarBuild(S);
        intruder(S, planet("BLACK HOLE", 10000.0, 3.0, 3, 0.0f, 0.0f, 0.0f), 260.0, 15.0, 24.0, 2.6, 0.1);
        break;
    default: break;
    }
}
