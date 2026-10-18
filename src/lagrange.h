#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

constexpr int LG_TRAIL = 130;

struct LgBody {
    double x = 0.0, y = 0.0, vx = 0.0, vy = 0.0;
    float tx[LG_TRAIL] = {}, ty[LG_TRAIL] = {};
    int head = 0, count = 0;
    double since = 0.0;
    bool alive = true;
    int group = 0;
};

struct LgSeg {
    float x0, y0, x1, y1;
    uint8_t cls;
};

struct LagrangeState {
    bool ready = false;
    bool paused = false;
    bool jupiter = false;
    float speed = 1.0f;
    double mu = 0.01;
    double t = 0.0;
    double acc = 0.0;
    double pt[5][2] = {};
    int hover = -1;
    int selected = -1;
    bool leftPrev = false;
    bool tabPrev = false;
    bool clearPrev = false;
    uint32_t rng = 12345u;
    std::vector<LgBody> swarm;
    std::vector<LgBody> tests;
    LgBody tk7;
    std::vector<LgSeg> contour;
};

struct LgView {
    float cx, cy, s, k, W, H;
    void to(double x, double y, float& px, float& py) const {
        px = cx + s * (float)x;
        py = cy - s * (float)y;
    }
    void from(double px, double py, double& x, double& y) const {
        x = (px - cx) / s;
        y = (cy - py) / s;
    }
};

struct LgRect {
    float x0, y0, x1, y1;
    bool has(double x, double y) const { return x >= x0 && x <= x1 && y >= y0 && y <= y1; }
};

namespace lg {

constexpr double MU_EARTH = 0.01;
constexpr double MU_JUPITER = 0.00095;
constexpr double RATE = 2.0;
constexpr double STEP = 0.01;
constexpr double TRAIL_EVERY = 0.05;
constexpr int MAX_TESTS = 60;
constexpr int SWARM = 200;
constexpr double GX = 1.55;
constexpr double GY = 1.0;

double potential(double mu, double x, double y);
void accel(double mu, const double s[4], double a[4]);
void step(double mu, LgBody& b, double h);
void pushTrail(LgBody& b);
void circInit(double mu, double x, double y, LgBody& b);
void keplerInit(double mu, double dr, double dth, int sign, LgBody& b);
void solvePoints(double mu, double pt[5][2]);
void buildContours(LagrangeState& S);
void populate(LagrangeState& S);
void advance(LagrangeState& S, double units);
void dropTest(LagrangeState& S, double x, double y);
double rand01(LagrangeState& S);

}

LgView lgView(float W, float H);
LgRect lgInsetRect(float W, float H);
void lgSetMode(struct AppContext& app, bool jupiter);
void lgEnter(struct AppContext& app);
void lgRestart(struct AppContext& app);
void lgUpdate(struct AppContext& app, float dt, bool mouseNow, bool pressUi);
void lgDemoDrop(struct AppContext& app);
