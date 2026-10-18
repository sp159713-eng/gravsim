#pragma once
#include "vk_app.h"
#include <string>
#include <vector>

struct SolarX {
    bool gone = false;
    bool zeroPending = false;
    double t0 = 0.0;
    double snapT = 0.0;
    bool t0Pending = false;
    int sel = -1;
    float snapPos[3][3] = {};
    float snapVel[3][3] = {};
};

extern SolarX g_sx;
extern std::string g_sxDate;
extern bool g_sxNosunDemo;
extern std::string g_sxCamTo;
extern float g_sxCam[3];
extern bool g_sxCamSet;
extern int g_sxRate;

const double SX_YEAR_UNITS = 6.283185307179586 * 0.7155417527999327;
const double SX_LIGHT_AU_SEC = 499.0048;

bool solarxHome(const AppContext& app);
double solarxJd(const AppContext& app);
double solarxElapsed(const AppContext& app);
double solarxUnits(const AppContext& app, double units, float frameDt);
float solarxPlanetSunT(const AppContext& app);
void solarxApplyFlags(AppContext& app);
void solarxCamNow(AppContext& app, const char* to, bool setCam, float yaw, float pitch, float dist);
void solarxStep(AppContext& app, float dt);
void solarxRecord(AppContext& app, VkCommandBuffer cmd);
void solarxReset(AppContext& app);
void solarxLines(AppContext& app, OverlayVertex* out, uint32_t& used, const float vp[16]);
void solarxLabels(AppContext& app, std::vector<ScreenLabel>& labels, const float vp[16]);
void solarxText(AppContext& app, OverlayVertex* out, uint32_t& used, float px, float py);
bool solarxClick(AppContext& app, double fx, double fy);
std::vector<CardLine> solarxCard(const AppContext& app);
void solarxHalleyCard(const AppContext& app, std::vector<CardLine>& lines);
bool solarxObjPos(const AppContext& app, int k, float pos[3]);
std::string solarxDate(double jd);

void cometsSnapshot(const AppContext& app);
bool cometState(const AppContext& app, int k, float pos[3], float vel[3]);
void cometsLines(AppContext& app, OverlayVertex* out, uint32_t& used, const float vp[16], bool home);
void cometsLabels(AppContext& app, std::vector<ScreenLabel>& labels, const float vp[16]);
std::vector<CardLine> cometCard(const AppContext& app, int k);
void cometPeriodLines(const AppContext& app, int k, std::string& period, std::string& next);

void farLines(AppContext& app, OverlayVertex* out, uint32_t& used, const float vp[16]);
void farLabels(AppContext& app, std::vector<ScreenLabel>& labels, const float vp[16]);
bool voyagerState(const AppContext& app, int v, float pos[3], double& au);
std::vector<CardLine> voyagerCard(const AppContext& app, int v);
bool sxProject(const float m[16], const float p[3], float& x, float& y);
void sxLine(OverlayVertex* out, uint32_t& used, float x0, float y0, float x1, float y1, const float c0[3], const float c1[3]);
