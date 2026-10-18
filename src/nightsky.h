#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

struct NsStar {
    const char* name;
    float raH, decDeg, mag, bv, distLy;
};

struct NsConst {
    const char* name;
    const char* const* chain;
};

extern const NsStar NS_STARS_A[];
extern const int NS_STARS_A_N;
extern const NsStar NS_STARS_B[];
extern const int NS_STARS_B_N;
extern const NsConst NS_CONSTS[];
extern const int NS_CONSTS_N;

constexpr int NS_PLACES = 4;
constexpr int NS_VIEWS = 5;

struct NsPoint {
    int id;
    float x, y;
    float mag;
    float dist;
    float vis;
    float rgb[3];
};

struct NsCatStar {
    std::string name;
    float pos[3];
    float mag, dist, rgb[3];
    float dir[3];
    float amag, adist;
    float nx, ny, vis;
    bool ok;
};

struct NsLink {
    int a, b;
};

struct NsGroup {
    std::string name;
    std::vector<NsLink> links;
    std::vector<int> stars;
};

struct NightSkyState {
    bool ready = false;
    bool mousePrev = false;
    bool showLines = false;
    bool keyC = false, keyL = false, keyT = false;
    int place = 0;
    int view = 0;
    int fromView = 0;
    int toView = 0;
    float lst = 5.5f;
    float lat = 28.6f;
    float fov = 60.0f;
    float lineFade = 0.0f;
    float earthBlend = 1.0f;
    float moveT = 1.0f;
    float clock = 0.0f;
    float aspect = 1.78f;
    float L[3] = {0, 1, 0};
    float fromPos[3] = {0, 0, 0};
    float obs[3] = {0, 0, 0};
    float F[3] = {1, 0, 0}, R[3] = {0, 1, 0}, U[3] = {0, 0, 1};
    float Z[3] = {0, 0, 1}, N[3] = {1, 0, 0}, E[3] = {0, 1, 0};
    int hover = -1;
    float lastCx = 0.0f, lastCy = 0.0f;
    std::vector<NsCatStar> cat;
    std::vector<NsGroup> groups;
    std::vector<NsPoint> pts;
    int viewStar[NS_VIEWS] = {-1, -1, -1, -1, -1};
    int sunPt = -1;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

struct AppContext;
struct OverlayVertex;
void createNightSky(AppContext& app);
void destroyNightSky(AppContext& app);
void nsEnter(AppContext& app);
void nsUpdate(AppContext& app, float dt, bool mouse);
void nsLeave(AppContext& app);
void drawNightSky(AppContext& app, VkCommandBuffer cmd, float aspect);
void emitNightSky(AppContext& app, OverlayVertex* out, uint32_t& used);

void nsBuildCatalogue(NightSkyState& S);
void nsFrame(NightSkyState& S);
void nsProject(NightSkyState& S, float aspect);
float nsHillAlt(float az);
bool nsNdc(const NightSkyState& S, const float dir[3], float& x, float& y);
int nsFindStar(const NightSkyState& S, const char* name);
void nsSetLook(NightSkyState& S, float raH, float decDeg);
void nsSetView(NightSkyState& S, int v);
const float* nsViewPos(const NightSkyState& S, int v);
int nsYear();
const char* nsPlaceName(int i);
const char* nsViewName(int i);
