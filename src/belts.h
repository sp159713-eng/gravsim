#pragma once
#include "vk_app.h"
#include <string>
#include <vector>

struct BeltVertex {
    float a[4];
    float b[4];
    float c[4];
    float d[4];
};

struct BeltNamed {
    const char* name;
    const char* kind;
    double a, e, inc, node, arg, m0;
    double diameterKm;
    float col[3];
    double labelAu;
    bool far;
    bool card;
    const char* disc;
    const char* fact;
};

constexpr int BELT_NAMED = 11;
constexpr int BELT_SEL_BASE = 4;

struct BeltState {
    bool on = true;
    bool keyDown = false;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    uint32_t count = 0;
};

extern BeltState g_belts;

std::vector<BeltVertex> beltsGenerate();
const BeltNamed& beltNamed(int i);
double beltsLamJ(double tDays);
double beltsLamN(double tDays);
double beltsRingAu(int k);

void createBelts(AppContext& app);
void destroyBelts(AppContext& app);
void drawBelts(AppContext& app, VkCommandBuffer cmd, const float viewProj[16]);
void beltsStep(AppContext& app);
bool beltsActive(const AppContext& app);
bool beltsObjPos(const AppContext& app, int j, float pos[3]);
void beltsLabels(AppContext& app, std::vector<ScreenLabel>& labels, const float vp[16]);
void beltsText(AppContext& app, OverlayVertex* out, uint32_t& used);
std::vector<CardLine> beltsCard(const AppContext& app, int j);
bool beltsLabelShown(const AppContext& app, int j);
