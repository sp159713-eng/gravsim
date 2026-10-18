#pragma once
#include <vulkan/vulkan.h>

constexpr float EC_EARTH_R = 1.0f;
constexpr float EC_MOON_R = 0.27f;
constexpr float EC_ORBIT_R = 9.0f;
constexpr float EC_INCL = 0.2618f;
constexpr float EC_SUN_DIST = 16.0f;
constexpr float EC_SUN_R = 1.8f;
constexpr float EC_E_UMBRA_LEN = 22.0f;
constexpr float EC_M_UMBRA_LEN = 11.5f;
constexpr float EC_SUN_RAD_DEG = 0.2656f;
constexpr float EC_MOON_PERI_DEG = 0.2800f;
constexpr float EC_MOON_APO_DEG = 0.2450f;
constexpr float EC_REL_SPEED = 0.0092f;
constexpr float EC_SPAN_MIN = 62.0f;
constexpr float EC_LUN_RU = 2.65f;
constexpr float EC_LUN_RP = 4.6f;
constexpr float EC_LUN_B = 0.45f;
constexpr float EC_SIGMAS[3] = {0.10f, 0.60f, 1.45f};

struct EclipseState {
    bool ready = false;
    int view = 0;
    float ph[3] = {0.0f, 0.25f, 0.5f};
    bool playing = false;
    bool apogee = false;
    int season = 0;
    float sigma = 0.10f;
    bool swap = false;
    float az = 0.0f, el = 0.72f, dist = 22.0f, tanf = 0.46f;
    float camPos[3] = {0, 0, 0};
    float F[3] = {0, 0, -1}, R[3] = {1, 0, 0}, U[3] = {0, 1, 0};
    float aspect = 1.78f;
    float clock = 0.0f;
    bool mousePrev = false;
    float lastX = 0.0f, lastY = 0.0f;
    bool keyPrev[16] = {};
    float keyHold[16] = {};
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

struct EcGround {
    float t;
    float sepX, sepY, d;
    float rs, rm;
    float exposed;
    int stage;
};

struct EcSpace {
    float moon[3];
    float height;
    int solar;
    int lunar;
};

struct EcLunar {
    float xs, ys, dd;
    int stage;
};

struct AppContext;
struct OverlayVertex;
void createEclipse(AppContext& app);
void destroyEclipse(AppContext& app);
void ecEnter(AppContext& app);
void ecLeave(AppContext& app);
void ecUpdate(AppContext& app, float dt, bool mouse);
void drawEclipse(AppContext& app, VkCommandBuffer cmd, float aspect);
void emitEclipse(AppContext& app, OverlayVertex* out, uint32_t& used);

void ecNodeAxes(float sigma, float* e1, float* e2);
void ecMoonPos(float sigma, float phase, float* out);
EcSpace ecSpace(const EclipseState& S);
EcGround ecGround(const EclipseState& S);
EcLunar ecLunar(const EclipseState& S);
void ecCamera(EclipseState& S);
bool ecProject(const EclipseState& S, const float* P, float& x, float& y);

extern int g_ecView;
extern float g_ecPhase;
extern bool g_ecApogee;
extern int g_ecSeason;
