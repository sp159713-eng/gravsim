#pragma once
#include <vulkan/vulkan.h>

struct FsGal {
    float az, el, lnR, tilt, q, disc, ell, tide, bright;
};

struct FsParams {
    FsGal a, b;
    float band, warp, burst, sun, lum;
};

struct FutureSkyState {
    bool ready = false;
    bool drag = false;
    bool playing = false;
    bool mousePrev = false;
    float t = 0.0f;
    float clock = 0.0f;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

constexpr float FS_MAX_GYR = 7.0f;
constexpr int FS_STAGES = 7;
constexpr float FS_STAGE_T[FS_STAGES] = {0.0f, 2.0f, 3.75f, 3.85f, 4.0f, 5.1f, 7.0f};

void fsEval(float t, FsParams& out);
int fsStage(float t);
const char* fsStageHead(int i);
const char* fsStageDetail(int i);

inline void fsSlider(float W, float H, float& x0, float& x1, float& y) {
    x0 = 0.08f * W;
    x1 = 0.92f * W;
    y = H - 34.0f * (H / 720.0f);
}

struct AppContext;
struct OverlayVertex;
void createFutureSky(AppContext& app);
void destroyFutureSky(AppContext& app);
void fsEnter(AppContext& app);
bool fsUpdate(AppContext& app, float dt, bool play, int step, bool fast, bool mouse);
void drawFutureSky(AppContext& app, VkCommandBuffer cmd, float aspect);
void emitFutureSky(AppContext& app, OverlayVertex* out, uint32_t& used);
