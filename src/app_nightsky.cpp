#include "vk_app.h"
#include "gpu.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<char> readSpvFile(const std::string& path);
VkShaderModule createSM(VkDevice dev, const std::vector<char>& code);

std::string g_nsFrom;
bool g_nsLookSet = false;
float g_nsLook[2] = {0.0f, 0.0f};
float g_nsLat = -999.0f;
float g_nsLst = -1.0f;
float g_nsFov = -1.0f;
bool g_nsLines = false;
std::string g_nsGo;
std::string g_nsHover;

namespace {

const float PI = 3.14159265f;

struct PushData {
    float F[4], R[4], U[4], Z[4], N[4], E[4], q[4];
};

float dot3(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

void unit(float* v) {
    float l = std::sqrt(dot3(v, v));
    if (l > 1e-9f) { v[0] /= l; v[1] /= l; v[2] /= l; }
}

void setLat(NightSkyState& S, int place) {
    S.place = place;
    static const float lats[NS_PLACES] = {28.6f, 0.0f, 51.5f, -33.9f};
    S.lat = lats[place % NS_PLACES];
}

void altAz(const NightSkyState& S, float& alt, float& az) {
    alt = std::asin(std::max(-1.0f, std::min(1.0f, dot3(S.L, S.Z))));
    az = std::atan2(dot3(S.L, S.E), dot3(S.L, S.N));
}

void fromAltAz(NightSkyState& S, float alt, float az) {
    float c = std::cos(alt);
    for (int i = 0; i < 3; i++)
        S.L[i] = c * (std::cos(az) * S.N[i] + std::sin(az) * S.E[i]) + std::sin(alt) * S.Z[i];
    unit(S.L);
}

bool keyDown(AppContext& app, int k) { return glfwGetKey(app.window, k) == GLFW_PRESS; }

void startMove(NightSkyState& S, int next) {
    for (int i = 0; i < 3; i++) S.fromPos[i] = S.obs[i];
    S.fromView = S.view;
    S.toView = next;
    S.view = next;
    S.moveT = 0.0f;
}

}

void createNightSky(AppContext& app) {
    NightSkyState& S = app.ns;
    VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushData)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &push};
    if (vkCreatePipelineLayout(app.device, &layoutInfo, nullptr, &S.layout) != VK_SUCCESS)
        throw std::runtime_error("failed night sky layout!");
    VkShaderModule vertSM = createSM(app.device, readSpvFile(shaderPath("sky.vert")));
    VkShaderModule fragSM = createSM(app.device, readSpvFile(shaderPath("nightsky.frag")));
    VkPipelineShaderStageCreateInfo stages[2]{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertSM, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragSM, "main", nullptr}
    };
    VkPipelineVertexInputStateCreateInfo vertInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo inputAss{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};
    VkPipelineViewportStateCreateInfo vpState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0, 0, 0, 1.0f};
    VkPipelineMultisampleStateCreateInfo multi{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0, nullptr, VK_FALSE, VK_FALSE};
    VkPipelineColorBlendAttachmentState attBlend{};
    attBlend.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY, 1, &attBlend, {0, 0, 0, 0}};
    VkDynamicState dyns[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, dyns};
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, stages, &vertInput, &inputAss, nullptr, &vpState, &raster, &multi, nullptr, &blend, &dyn, S.layout, app.renderPass, 0, VK_NULL_HANDLE, -1};
    if (vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &info, nullptr, &S.pipeline) != VK_SUCCESS)
        throw std::runtime_error("failed night sky pipeline!");
    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
    nsBuildCatalogue(S);
}

void destroyNightSky(AppContext& app) {
    vkDestroyPipeline(app.device, app.ns.pipeline, nullptr);
    vkDestroyPipelineLayout(app.device, app.ns.layout, nullptr);
}

void nsEnter(AppContext& app) {
    NightSkyState& S = app.ns;
    S.ready = true;
    S.mousePrev = false;
    S.keyC = S.keyL = S.keyT = false;
    S.clock = 0.0f;
    S.lst = g_nsLst >= 0.0f ? g_nsLst : 5.5f;
    setLat(S, 0);
    if (g_nsLat > -900.0f) {
        S.lat = g_nsLat;
        S.place = 0;
        if (std::fabs(g_nsLat + 33.9f) < 0.2f) S.place = 3;
        else if (std::fabs(g_nsLat - 51.5f) < 0.2f) S.place = 2;
        else if (std::fabs(g_nsLat) < 0.2f) S.place = 1;
    }
    S.fov = g_nsFov > 0.0f ? g_nsFov : 75.0f;
    S.showLines = g_nsLines;
    S.lineFade = g_nsLines ? 1.0f : 0.0f;
    nsSetView(S, 0);
    int v = 0;
    if (!g_nsFrom.empty()) {
        int id = nsFindStar(S, g_nsFrom.c_str());
        for (int i = 1; i < NS_VIEWS; i++)
            if (S.viewStar[i] == id && id >= 0) v = i;
        if (v == 0 && id >= 0) {
            S.viewStar[NS_VIEWS - 1] = id;
            v = NS_VIEWS - 1;
        }
        nsSetView(S, v);
    }
    nsFrame(S);
    float al = 32.0f * PI / 180.0f, az = PI;
    fromAltAz(S, al, az);
    if (g_nsLookSet) nsSetLook(S, g_nsLook[0], g_nsLook[1]);
    g_nsFrom.clear();
    g_nsLookSet = false;
    app.showSky = false;
    app.camera.zoomImpulse = 0.0f;
    app.camera.wheelHold = true;
    app.camera.wheelAcc = 0.0f;
    nsFrame(S);
}

void nsLeave(AppContext& app) {
    g_nsHover.clear();
    app.camera.wheelHold = false;
    app.camera.wheelAcc = 0.0f;
}

void nsUpdate(AppContext& app, float dt, bool mouse) {
    NightSkyState& S = app.ns;
    dt = std::min(dt, 0.1f);
    S.clock += dt;
    app.camera.zoomImpulse = 0.0f;
    app.camera.wheelHold = true;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    S.aspect = W / H;
    bool shift = keyDown(app, GLFW_KEY_LEFT_SHIFT) || keyDown(app, GLFW_KEY_RIGHT_SHIFT);

    float wheel = std::max(-8.0f, std::min(8.0f, app.camera.wheelAcc));
    app.camera.wheelAcc = 0.0f;
    if (wheel != 0.0f) S.fov = std::max(6.0f, std::min(100.0f, S.fov * std::pow(0.88f, wheel)));

    bool c = keyDown(app, GLFW_KEY_C), l = keyDown(app, GLFW_KEY_L), t = keyDown(app, GLFW_KEY_T);
    if (c && !S.keyC) S.showLines = !S.showLines;
    if (l && !S.keyL) setLat(S, (S.place + 1) % NS_PLACES);
    if (t && !S.keyT) {
        int next = (S.view + 1) % NS_VIEWS;
        if (S.viewStar[next] < 0 && next != 0) next = 0;
        startMove(S, next);
    }
    if (!g_nsGo.empty() && S.clock > 1.0f) {
        int id = nsFindStar(S, g_nsGo.c_str());
        for (int i = 1; i < NS_VIEWS; i++)
            if (id >= 0 && S.viewStar[i] == id) startMove(S, i);
        g_nsGo.clear();
    }
    S.keyC = c; S.keyL = l; S.keyT = t;
    S.lineFade += ((S.showLines ? 1.0f : 0.0f) - S.lineFade) * std::min(1.0f, dt * 3.5f);

    float dtime = 0.0f;
    if (keyDown(app, GLFW_KEY_EQUAL) || keyDown(app, GLFW_KEY_KP_ADD)) dtime += 1.0f;
    if (keyDown(app, GLFW_KEY_MINUS) || keyDown(app, GLFW_KEY_KP_SUBTRACT)) dtime -= 1.0f;
    bool earthLike = S.earthBlend > 0.5f;
    nsFrame(S);
    float holdAlt = 0.0f, holdAz = 0.0f;
    if (earthLike) altAz(S, holdAlt, holdAz);
    if (dtime != 0.0f) {
        S.lst += dtime * dt * (shift ? 6.0f : 1.0f);
        S.lst -= 24.0f * std::floor(S.lst / 24.0f);
    }
    nsFrame(S);
    if (earthLike) fromAltAz(S, holdAlt, holdAz);

    if (S.moveT < 1.0f) {
        S.moveT = std::min(1.0f, S.moveT + dt / 3.0f);
        float e = S.moveT * S.moveT * (3.0f - 2.0f * S.moveT);
        const float* to = nsViewPos(S, S.toView);
        for (int i = 0; i < 3; i++) S.obs[i] = S.fromPos[i] + (to[i] - S.fromPos[i]) * e;
        if (S.moveT >= 1.0f) for (int i = 0; i < 3; i++) S.obs[i] = to[i];
    }
    bool settled = S.moveT >= 1.0f;
    float target = (S.view == 0 && (settled || S.moveT > 0.7f)) ? 1.0f : 0.0f;
    float rate = dt * 1.6f;
    if (S.earthBlend < target) S.earthBlend = std::min(target, S.earthBlend + rate);
    else if (S.earthBlend > target) S.earthBlend = std::max(target, S.earthBlend - rate);

    double cx = app.cursorFb[0], cy = app.cursorFb[1];
    if (mouse && app.cursorOk) {
        if (S.mousePrev) {
            float th = std::tan(0.5f * S.fov * PI / 180.0f);
            float k = 2.0f * th / H;
            float dx = (float)(cx - S.lastCx) * k, dy = (float)(cy - S.lastCy) * k;
            if (S.earthBlend > 0.5f) {
                float alt, az;
                altAz(S, alt, az);
                az -= dx / std::max(0.25f, std::cos(alt));
                alt = std::max(-1.2f, std::min(1.5f, alt + dy));
                fromAltAz(S, alt, az);
            } else {
                float ra = std::atan2(S.L[1], S.L[0]);
                float dec = std::asin(std::max(-1.0f, std::min(1.0f, S.L[2])));
                ra += dx / std::max(0.25f, std::cos(dec));
                dec = std::max(-1.5f, std::min(1.5f, dec + dy));
                S.L[0] = std::cos(dec) * std::cos(ra);
                S.L[1] = std::cos(dec) * std::sin(ra);
                S.L[2] = std::sin(dec);
            }
        }
        S.mousePrev = true;
    } else {
        S.mousePrev = false;
    }
    S.lastCx = (float)cx;
    S.lastCy = (float)cy;
    nsFrame(S);
    nsProject(S, S.aspect);

    S.hover = -1;
    if (app.cursorOk && !mouse && S.moveT >= 1.0f) {
        float k = H / 720.0f;
        float best = 1e30f;
        for (size_t i = 0; i < S.pts.size(); i++) {
            const NsPoint& p = S.pts[i];
            float px = (p.x * 0.5f + 0.5f) * W, py = (p.y * 0.5f + 0.5f) * H;
            float d2 = (px - (float)cx) * (px - (float)cx) + (py - (float)cy) * (py - (float)cy);
            float reach = std::max(11.0f * k, 4.0f * k + 3.0f * std::max(0.0f, 4.0f - p.mag));
            if (d2 < reach * reach && d2 - 20.0f * p.mag < best) {
                best = d2 - 20.0f * p.mag;
                S.hover = (int)i;
            }
        }
    }
    if (S.hover < 0 && !g_nsHover.empty() && S.moveT >= 1.0f) {
        int id = nsFindStar(S, g_nsHover.c_str());
        for (size_t i = 0; id >= 0 && i < S.pts.size(); i++)
            if (S.pts[i].id == id) S.hover = (int)i;
    }
}

void drawNightSky(AppContext& app, VkCommandBuffer cmd, float aspect) {
    NightSkyState& S = app.ns;
    PushData p{};
    for (int i = 0; i < 3; i++) {
        p.F[i] = S.F[i]; p.R[i] = S.R[i]; p.U[i] = S.U[i];
        p.Z[i] = S.Z[i]; p.N[i] = S.N[i]; p.E[i] = S.E[i];
    }
    p.q[0] = std::tan(0.5f * S.fov * PI / 180.0f);
    p.q[1] = aspect;
    p.q[2] = S.earthBlend;
    p.q[3] = S.clock;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.pipeline);
    vkCmdPushConstants(cmd, S.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushData), &p);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}
