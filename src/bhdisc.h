#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>

struct AppContext;
struct OverlayVertex;

constexpr uint32_t BD_N = 131072;
constexpr uint32_t BD_JET = 16000;
constexpr double BD_DT = 0.5;
constexpr uint32_t BD_SNAP_EVERY = 2400;
constexpr int BD_SNAPS = 21;
constexpr int BD_CAP = 24;
constexpr int BD_CAP_SEEK = 128;
constexpr int BD_BINS = 420;
constexpr float BD_BIN_S = 0.5f;

struct BdStepPush {
    float com[4];
    float comv[4];
    float src[4];
    float srcv[4];
    uint32_t mode, n, chunk, scen;
    float dt, last, spawnP, visc;
};

struct BdDrawPush {
    float viewProj[16];
    float eye[4];
    float prm[4];
    float prm2[4];
};

struct BdCom {
    double x[3] = {0, 0, 0};
    double v[3] = {0, 0, 0};
    double k = 0.0;
    double fade = 0.0;
    bool fading = false;
};

struct BdOp {
    int kind;
    int idx;
    BdStepPush push;
};

struct BdSnap {
    bool valid = false;
    BdCom com;
    uint64_t eaten = 0;
    float peak = 0.0f;
};

struct BdSlot {
    bool pending = false;
    int epoch = 0;
    float tsEnd = 0.0f;
    bool hasLast = false;
};

struct BdBins {
    float eaten[BD_BINS] = {};
    float disc[BD_BINS] = {};
    float lum[BD_BINS] = {};
    float shock[BD_BINS] = {};
    float cnt[BD_BINS] = {};
    void clearFrom(int b) {
        for (int i = b < 0 ? 0 : b; i < BD_BINS; i++) eaten[i] = disc[i] = lum[i] = shock[i] = cnt[i] = 0.0f;
    }
};

struct BdState {
    bool created = false;
    bool active = false;
    VkBuffer parts = VK_NULL_HANDLE, grid = VK_NULL_HANDLE, rad = VK_NULL_HANDLE;
    VkBuffer stats[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    VkBuffer snapBuf[BD_SNAPS] = {};
    VkDeviceMemory memParts = VK_NULL_HANDLE, memGrid = VK_NULL_HANDLE, memRad = VK_NULL_HANDLE, memSnap = VK_NULL_HANDLE;
    VkDeviceMemory memStats[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    uint32_t* statMap[2] = {nullptr, nullptr};
    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet ds[2] = {};
    VkPipelineLayout cLayout = VK_NULL_HANDLE, gLayout = VK_NULL_HANDLE;
    VkPipeline cPipe = VK_NULL_HANDLE, gPipe = VK_NULL_HANDLE;
    uint32_t snapCount = BD_SNAPS;

    int scen = 0;
    int epoch = 0;
    double ts = 0.0;
    double tw = 0.0;
    bool seek = false;
    bool bigCap = false;
    bool atHold = false;
    uint32_t chunk = 0;
    bool paused = false;
    bool backScrub = false;
    bool keyPrev[10] = {};
    double clock = 0.0;
    BdCom com;
    uint64_t eaten = 0;
    uint64_t escaped = 0;
    float discNow = 0.0f;
    float peak = 0.0f;
    float discPeakAll = 0.0f;
    float jet = 0.0f;
    float logUser = 0.0f;
    float camDist = 250.0f;
    float lumNow = 0.0f;
    float lumPeak = 1e-6f;
    BdBins bins;
    BdSnap snap[BD_SNAPS];
    std::vector<BdOp> ops;
    BdSlot slot[2];
    float feedFlash = 0.0f;
    double fps = 0.0;
    float lastDt = 0.0f;
};

extern float g_bdAtSec;
extern int g_bdScen;

BdState& bdState();
double bdPhi(double ts);
double bdPhiInv(double chunks);
float bdRate(double ts, float win);
void bdComInit(BdCom& c, int scen);
void bdComStep(BdCom& c, int scen);
void bdFillStep(BdStepPush& p, const BdCom& c, int scen, uint32_t mode, uint32_t chunk, bool last);

void bdCreateVk(AppContext& app);
void bdDestroyVk(AppContext& app);
void createBhDisc(AppContext& app);
void destroyBhDisc(AppContext& app);
void bdEnter(AppContext& app);
void bdLeave(AppContext& app);
void bdUpdate(AppContext& app, float dt);
void bdRecord(AppContext& app, VkCommandBuffer cmd);
void drawBhDisc(AppContext& app, VkCommandBuffer cmd, float aspect, const float viewProj[16]);
void emitBhDisc(AppContext& app, OverlayVertex* out, uint32_t& used);
void drawBhDiscHole(AppContext& app, VkCommandBuffer cmd, const float viewProj[16], float radius, float timeSec);
