#include "vk_app.h"
#include <algorithm>
#include <cmath>

namespace lg {

double potential(double mu, double x, double y) {
    double r1 = std::sqrt((x + mu) * (x + mu) + y * y) + 1e-4;
    double r2 = std::sqrt((x - 1.0 + mu) * (x - 1.0 + mu) + y * y) + 1e-4;
    return 0.5 * (x * x + y * y) + (1.0 - mu) / r1 + mu / r2;
}

void accel(double mu, const double s[4], double a[4]) {
    double dx1 = s[0] + mu, dx2 = s[0] - 1.0 + mu;
    double r1 = std::sqrt(dx1 * dx1 + s[1] * s[1]) + 1e-9;
    double r2 = std::sqrt(dx2 * dx2 + s[1] * s[1]) + 1e-9;
    double i1 = (1.0 - mu) / (r1 * r1 * r1), i2 = mu / (r2 * r2 * r2);
    a[0] = s[2];
    a[1] = s[3];
    a[2] = s[0] + 2.0 * s[3] - i1 * dx1 - i2 * dx2;
    a[3] = s[1] - 2.0 * s[2] - i1 * s[1] - i2 * s[1];
}

void step(double mu, LgBody& b, double h) {
    double s[4] = {b.x, b.y, b.vx, b.vy}, k1[4], k2[4], k3[4], k4[4], t[4];
    accel(mu, s, k1);
    for (int i = 0; i < 4; i++) t[i] = s[i] + 0.5 * h * k1[i];
    accel(mu, t, k2);
    for (int i = 0; i < 4; i++) t[i] = s[i] + 0.5 * h * k2[i];
    accel(mu, t, k3);
    for (int i = 0; i < 4; i++) t[i] = s[i] + h * k3[i];
    accel(mu, t, k4);
    for (int i = 0; i < 4; i++) s[i] += h / 6.0 * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    b.x = s[0];
    b.y = s[1];
    b.vx = s[2];
    b.vy = s[3];
    b.since += h;
    if (b.since >= TRAIL_EVERY) {
        b.since -= TRAIL_EVERY;
        pushTrail(b);
    }
}

void pushTrail(LgBody& b) {
    b.tx[b.head] = (float)b.x;
    b.ty[b.head] = (float)b.y;
    b.head = (b.head + 1) % LG_TRAIL;
    b.count = std::min(b.count + 1, LG_TRAIL);
}

void circInit(double mu, double x, double y, LgBody& b) {
    double dx1 = x + mu, dx2 = x - 1.0 + mu;
    double r1 = std::sqrt(dx1 * dx1 + y * y) + 1e-9, r2 = std::sqrt(dx2 * dx2 + y * y) + 1e-9;
    double ax = -(1.0 - mu) * dx1 / (r1 * r1 * r1) - mu * dx2 / (r2 * r2 * r2);
    double ay = -(1.0 - mu) * y / (r1 * r1 * r1) - mu * y / (r2 * r2 * r2);
    double rho = std::sqrt(x * x + y * y) + 1e-9;
    double g = -(ax * x + ay * y) / rho;
    double w = std::sqrt(std::max(g, 0.0) / rho);
    b = LgBody{};
    b.x = x;
    b.y = y;
    b.vx = -(w - 1.0) * y;
    b.vy = (w - 1.0) * x;
    pushTrail(b);
}

void keplerInit(double mu, double dr, double dth, int sign, LgBody& b) {
    double r = 1.0 + dr, th = sign * 1.0471975512 + dth;
    double vt = std::sqrt((1.0 - mu) / r) - r;
    b = LgBody{};
    b.x = -mu + r * std::cos(th);
    b.y = r * std::sin(th);
    b.vx = -vt * std::sin(th);
    b.vy = vt * std::cos(th);
    pushTrail(b);
}

static double colinear(double mu, double x) {
    double a = x + mu, c = x - 1.0 + mu;
    return x - (1.0 - mu) * a / std::pow(std::fabs(a), 3.0) - mu * c / std::pow(std::fabs(c), 3.0);
}

static double bisect(double mu, double lo, double hi) {
    for (int i = 0; i < 100; i++) {
        double mid = 0.5 * (lo + hi);
        if (colinear(mu, mid) < 0.0) lo = mid;
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

void solvePoints(double mu, double pt[5][2]) {
    pt[0][0] = bisect(mu, -mu + 1e-3, 1.0 - mu - 1e-3);
    pt[1][0] = bisect(mu, 1.0 - mu + 1e-3, 3.0);
    pt[2][0] = bisect(mu, -3.0, -mu - 1e-3);
    for (int i = 0; i < 3; i++) pt[i][1] = 0.0;
    pt[3][0] = pt[4][0] = 0.5 - mu;
    pt[3][1] = 0.8660254037844386;
    pt[4][1] = -0.8660254037844386;
}

double rand01(LagrangeState& S) {
    S.rng = S.rng * 1664525u + 1013904223u;
    return (double)(S.rng >> 8) / 16777216.0;
}

void dropTest(LagrangeState& S, double x, double y) {
    if ((int)S.tests.size() >= MAX_TESTS) S.tests.erase(S.tests.begin());
    LgBody b;
    circInit(S.mu, x, y, b);
    S.tests.push_back(b);
}

static void respawn(LagrangeState& S, LgBody& b, int group) {
    keplerInit(S.mu, (rand01(S) * 2.0 - 1.0) * 0.025, (rand01(S) * 2.0 - 1.0) * 0.35, group, b);
    b.group = group;
}

void populate(LagrangeState& S) {
    S.swarm.clear();
    S.tests.clear();
    if (S.jupiter) {
        S.swarm.resize(SWARM);
        for (int i = 0; i < SWARM; i++) respawn(S, S.swarm[i], i < SWARM / 2 ? 1 : -1);
    } else {
        circInit(S.mu, S.pt[3][0] + 0.01, S.pt[3][1] + 0.045, S.tk7);
    }
}

void advance(LagrangeState& S, double units) {
    S.acc += units;
    int guard = 0;
    while (S.acc >= STEP && guard++ < 400) {
        S.acc -= STEP;
        S.t += STEP;
        for (LgBody& b : S.swarm) {
            step(S.mu, b, STEP);
            double d2 = (b.x - 1.0 + S.mu) * (b.x - 1.0 + S.mu) + b.y * b.y;
            if (d2 < 1e-4 || b.x * b.x + b.y * b.y > 4.0) respawn(S, b, b.group);
        }
        if (!S.jupiter) {
            step(S.mu, S.tk7, STEP);
            double d2 = (S.tk7.x - 1.0 + S.mu) * (S.tk7.x - 1.0 + S.mu) + S.tk7.y * S.tk7.y;
            if (d2 < 4e-4 || S.tk7.x * S.tk7.x + S.tk7.y * S.tk7.y > 4.0)
                circInit(S.mu, S.pt[3][0] + 0.01, S.pt[3][1] + 0.045, S.tk7);
        }
        for (LgBody& b : S.tests) {
            if (!b.alive) continue;
            step(S.mu, b, STEP);
            double d1 = (b.x + S.mu) * (b.x + S.mu) + b.y * b.y;
            double d2 = (b.x - 1.0 + S.mu) * (b.x - 1.0 + S.mu) + b.y * b.y;
            if (d1 < 4e-4 || d2 < 1e-5 || b.x * b.x + b.y * b.y > 30.0) b.alive = false;
        }
    }
    S.tests.erase(std::remove_if(S.tests.begin(), S.tests.end(), [](const LgBody& b) { return !b.alive; }), S.tests.end());
}

void buildContours(LagrangeState& S) {
    S.contour.clear();
    double mu = S.mu;
    double om[5];
    for (int i = 0; i < 5; i++) om[i] = potential(mu, S.pt[i][0], S.pt[i][1]);
    struct Lvl { double v; uint8_t cls; };
    std::vector<Lvl> lv;
    double l4 = om[3];
    lv.push_back({l4 + 0.2 * (om[2] - l4), 0});
    lv.push_back({l4 + 0.55 * (om[2] - l4), 0});
    lv.push_back({om[2] + 0.0004, 1});
    lv.push_back({om[1] + 0.0004, 1});
    lv.push_back({om[0] + 0.0004, 1});
    const double up[] = {0.04, 0.12, 0.3, 0.6, 1.1, 2.0};
    for (double u : up) lv.push_back({om[0] + u, 2});
    const int NX = 190, NY = 126;
    std::vector<double> g((NX + 1) * (NY + 1));
    for (int j = 0; j <= NY; j++)
        for (int i = 0; i <= NX; i++)
            g[j * (NX + 1) + i] = potential(mu, -GX + 2.0 * GX * i / NX, -GY + 2.0 * GY * j / NY);
    for (const Lvl& L : lv) {
        for (int j = 0; j < NY; j++)
            for (int i = 0; i < NX; i++) {
                double a = g[j * (NX + 1) + i], b = g[j * (NX + 1) + i + 1];
                double c = g[(j + 1) * (NX + 1) + i + 1], d = g[(j + 1) * (NX + 1) + i];
                double x0 = -GX + 2.0 * GX * i / NX, x1 = -GX + 2.0 * GX * (i + 1) / NX;
                double y0 = -GY + 2.0 * GY * j / NY, y1 = -GY + 2.0 * GY * (j + 1) / NY;
                double ex[4], ey[4];
                bool has[4] = {false, false, false, false};
                auto cut = [&](int e, double v0, double v1, double ax, double ay, double bx, double by) {
                    if ((v0 > L.v) == (v1 > L.v)) return;
                    double t = (L.v - v0) / (v1 - v0);
                    ex[e] = ax + t * (bx - ax);
                    ey[e] = ay + t * (by - ay);
                    has[e] = true;
                };
                cut(0, a, b, x0, y0, x1, y0);
                cut(1, b, c, x1, y0, x1, y1);
                cut(2, d, c, x0, y1, x1, y1);
                cut(3, a, d, x0, y0, x0, y1);
                int n = has[0] + has[1] + has[2] + has[3];
                auto add = [&](int p, int q) { S.contour.push_back({(float)ex[p], (float)ey[p], (float)ex[q], (float)ey[q], L.cls}); };
                if (n == 2) {
                    int p = -1, q = -1;
                    for (int e = 0; e < 4; e++)
                        if (has[e]) (p < 0 ? p : q) = e;
                    add(p, q);
                } else if (n == 4) {
                    double mid = 0.25 * (a + b + c + d);
                    if ((mid > L.v) == (a > L.v)) {
                        add(0, 1);
                        add(2, 3);
                    } else {
                        add(0, 3);
                        add(1, 2);
                    }
                }
            }
    }
}

}

LgView lgView(float W, float H) {
    float k = H / 720.0f;
    float s = std::min(0.255f * W, (H - 310.0f * k) / (2.0f * (float)lg::GY));
    return {0.42f * W, 150.0f * k + 0.5f * (H - 310.0f * k), s, k, W, H};
}

LgRect lgInsetRect(float W, float H) {
    float k = H / 720.0f;
    return {W - 340.0f * k, H - 262.0f * k, W - 14.0f * k, H - 62.0f * k};
}
