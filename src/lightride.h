#pragma once
#include <cmath>
#include <string>
#include <vector>

constexpr double LR_C_AU_S = 1.0 / 499.00478384;
constexpr double LR_KM_AU = 149597870.7;
constexpr double LR_C_KMS = 299792.458;
constexpr double LR_YEAR_S = 31557600.0;
constexpr double LR_PATH_DS = 0.0005;

struct LrMilestone {
    std::string head;
    std::string body;
    double t;
    int planet;
};

struct LrPath {
    std::vector<double> x, y, z;
    double endS = 0.0;
};

struct LrDust {
    float x, y, z;
};

struct LightRideState {
    bool ready = false;
    bool frozen = false;
    bool autoWarp = true;
    bool paused = false;
    bool back = false;
    bool backTouched = false;
    bool exitReq = false;
    bool tabPrev = false, plusPrev = false, minusPrev = false, autoPrev = false, viewPrev = false, restartPrev = false, escPrev = false, dragPrev = false;
    int wexp = 0;
    int capIdx = -1;
    double capAge = 0.0;
    double t = 0.0;
    double realT = 0.0;
    double warp = 1.0;
    double jd0 = 0.0;
    double tEarth = 500.0;
    double lastX = 0.0, lastY = 0.0;
    double pos[3] = {0, 0, 0};
    double fwd[3] = {1, 0, 0};
    double planetAu[8] = {};
    float yawOff = 0.0f, pitchOff = 0.14f;
    float backAmt = 0.0f;
    LrPath path;
    std::vector<LrMilestone> ms;
};

struct LrBasis {
    double f[3], r[3], u[3];
    double px;
    float W, H;
};

extern double g_lrStartAt;

void lrBuildPath(LightRideState& S);
void lrBuildMilestones(LightRideState& S);
void lrPose(LightRideState& S, double t);
void lrPlanetPos(const LightRideState& S, int i, double t, double out[3]);
void lrMakeBasis(const LightRideState& S, LrBasis& b);
void lrFormatDuration(double s, char* buf, size_t n);
void lrFormatClock(double s, char* buf, size_t n);
void lrFormatKm(double km, char* buf, size_t n);
