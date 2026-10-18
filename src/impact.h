#pragma once
#include <vulkan/vulkan.h>
#include <string>

constexpr int IM_TYPES = 3;
constexpr int IM_PRESETS_N = 5;
constexpr float IM_EARTH_KM = 6371.0f;
constexpr float IM_T_IN = 6.0f;
constexpr float IM_T_MISS = 14.0f;

struct ImpactParams {
    float diaM = 100.0f;
    float speedKms = 20.0f;
    float angleDeg = 45.0f;
    int type = 0;
};

struct ImpactResult {
    float massKg = 0.0f;
    double energyJ = 0.0;
    double energyMt = 0.0;
    double hiroshimas = 0.0;
    double tsars = 0.0;
    bool airburst = false;
    float burstKm = 0.0f;
    float transientKm = 0.0f;
    float finalKm = 0.0f;
    bool complexCrater = false;
    float fireballKm = 0.0f;
    float r5Km = 0.0f;
    float r1Km = 0.0f;
    double recurYears = 0.0;
    float veil = 0.0f;
    bool extinction = false;
};

struct ImpactPreset {
    const char* name;
    const char* note;
    float diaM, speedKms, angleDeg;
    int type;
    bool miss;
};

extern const ImpactPreset IM_PRESETS[IM_PRESETS_N];

float imDensity(int type);
const char* imTypeName(int type);
ImpactResult imCompute(const ImpactParams& p);
std::string imNum(double v);
std::string imRecur(double years);
std::string imKm(float km);

struct ImpactState {
    bool ready = false;
    ImpactParams p;
    ImpactResult r;
    int preset = -1;
    bool miss = false;
    bool running = false;
    float t = 0.0f;
    float lat = 28.61f, lon = 77.21f;
    float nv[3] = {0, 0, 1};
    float zoom = 1.0f;
    bool recenter = false;
    float camAz = 0.0f;
    float camD = 3.2f;
    float camTilt = 0.5f;
    float camPos[3] = {0, 0, 0};
    float F[3] = {0, 0, -1}, R[3] = {1, 0, 0}, U[3] = {0, 1, 0};
    float I[3] = {0, 0, 1};
    float A[3] = {0, 0, 0}, B[3] = {0, 0, 0};
    float aspect = 1.78f;
    float panelEdge = 0.36f;
    float tanf = 0.4663f;
    bool mousePrev = false;
    float downX = 0.0f, downY = 0.0f, lastX = 0.0f, lastY = 0.0f;
    bool dragged = false;
    bool keyPrev[16] = {};
    float keyHold[16] = {};
    int hoverRing = -1;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

struct AppContext;
struct OverlayVertex;
void createImpact(AppContext& app);
void destroyImpact(AppContext& app);
void imEnter(AppContext& app);
void imLeave(AppContext& app);
void imUpdate(AppContext& app, float dt, bool mouse);
void drawImpact(AppContext& app, VkCommandBuffer cmd, float aspect);
void emitImpact(AppContext& app, OverlayVertex* out, uint32_t& used);

void imLoadPreset(ImpactState& S, int idx);
void imRefresh(ImpactState& S);
void imGeometry(ImpactState& S);
void imCamera(ImpactState& S, float dt);
bool imProject(const ImpactState& S, const float* P, float& x, float& y);
void imSurfacePoint(const ImpactState& S, float angKm, float bearing, float* out);
float imHitT(const ImpactState& S);

extern int g_imPreset;
extern float g_imAt;
