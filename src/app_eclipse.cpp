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

int g_ecView = 0;
float g_ecPhase = -1.0f;
bool g_ecApogee = false;
int g_ecSeason = 0;

namespace {

struct PushData {
    float a[4], b[4], c[4], d[4], e[4], f[4], g[4], h[4];
};

bool keyDown(AppContext& app, int k) { return glfwGetKey(app.window, k) == GLFW_PRESS; }

bool edge(EclipseState& S, int idx, bool down) {
    bool fire = down && !S.keyPrev[idx];
    S.keyPrev[idx] = down;
    return fire;
}

float stepRate(int view) { return view == 0 ? 0.25f : 0.16f; }

}

void createEclipse(AppContext& app) {
    EclipseState& S = app.ec;
    VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushData)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &push};
    if (vkCreatePipelineLayout(app.device, &layoutInfo, nullptr, &S.layout) != VK_SUCCESS)
        throw std::runtime_error("failed eclipse layout!");
    VkShaderModule vertSM = createSM(app.device, readSpvFile(shaderPath("sky.vert")));
    VkShaderModule fragSM = createSM(app.device, readSpvFile(shaderPath("eclipse.frag")));
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
        throw std::runtime_error("failed eclipse pipeline!");
    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
}

void destroyEclipse(AppContext& app) {
    vkDestroyPipeline(app.device, app.ec.pipeline, nullptr);
    vkDestroyPipelineLayout(app.device, app.ec.layout, nullptr);
}

void ecEnter(AppContext& app) {
    EclipseState& S = app.ec;
    S.ready = true;
    S.view = std::max(0, std::min(2, g_ecView));
    S.ph[0] = 0.0f;
    S.ph[1] = 0.25f;
    S.ph[2] = 0.5f;
    if (g_ecPhase >= 0.0f) S.ph[S.view] = std::min(1.0f, g_ecPhase);
    g_ecView = 0;
    g_ecPhase = -1.0f;
    S.playing = false;
    S.apogee = g_ecApogee;
    S.swap = false;
    S.season = std::max(0, std::min(2, g_ecSeason));
    g_ecApogee = false;
    g_ecSeason = 0;
    S.sigma = EC_SIGMAS[S.season];
    S.az = 0.0f;
    S.el = 0.72f;
    S.dist = 22.0f;
    S.mousePrev = false;
    S.clock = 0.0f;
    S.aspect = (float)app.swapchainExtent.width / (float)app.swapchainExtent.height;
    ecCamera(S);
    for (int i = 0; i < 16; i++) {
        S.keyPrev[i] = true;
        S.keyHold[i] = 0.0f;
    }
    app.showSky = false;
    app.camera.zoomImpulse = 0.0f;
    app.camera.wheelHold = true;
    app.camera.wheelAcc = 0.0f;
}

void ecLeave(AppContext& app) {
    app.camera.wheelHold = false;
    app.camera.wheelAcc = 0.0f;
}

void ecUpdate(AppContext& app, float dt, bool mouse) {
    EclipseState& S = app.ec;
    dt = std::min(dt, 0.1f);
    S.clock += dt;
    app.camera.zoomImpulse = 0.0f;
    app.camera.wheelHold = true;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    S.aspect = W / H;

    float wheel = std::max(-8.0f, std::min(8.0f, app.camera.wheelAcc));
    app.camera.wheelAcc = 0.0f;
    if (wheel != 0.0f && S.view == 0) S.dist = std::max(8.0f, std::min(44.0f, S.dist * std::pow(0.9f, wheel)));

    bool shift = keyDown(app, GLFW_KEY_LEFT_SHIFT) || keyDown(app, GLFW_KEY_RIGHT_SHIFT);
    if (edge(S, 0, keyDown(app, GLFW_KEY_TAB))) S.view = (S.view + 1) % 3;
    if (edge(S, 1, keyDown(app, GLFW_KEY_SPACE))) S.playing = !S.playing;
    if (edge(S, 2, keyDown(app, GLFW_KEY_M))) S.apogee = !S.apogee;
    if (edge(S, 3, keyDown(app, GLFW_KEY_N))) S.season = (S.season + 1) % 3;
    if (edge(S, 4, keyDown(app, GLFW_KEY_W))) S.swap = !S.swap;

    int v = S.view;
    float stepSz = v == 0 ? 1.0f / 16.0f : 0.02f;
    bool left = keyDown(app, GLFW_KEY_LEFT), right = keyDown(app, GLFW_KEY_RIGHT);
    int dir = (right ? 1 : 0) - (left ? 1 : 0);
    if (dir != 0) {
        bool fresh = dir > 0 ? !S.keyPrev[5] : !S.keyPrev[6];
        if (fresh) {
            S.ph[v] += dir * stepSz * (shift ? 4.0f : 1.0f);
            S.keyHold[5] = 0.0f;
            S.playing = false;
        } else {
            S.keyHold[5] += dt;
            if (S.keyHold[5] > 0.3f) S.ph[v] += dir * stepRate(v) * (shift ? 4.0f : 1.0f) * dt;
        }
    }
    S.keyPrev[5] = right;
    S.keyPrev[6] = left;

    if (S.playing) {
        if (v == 0) S.ph[0] += dt / 18.0f;
        else if (v == 1) S.ph[1] += dt / 26.0f;
        else S.ph[2] += dt / 22.0f;
    }
    if (v == 0) S.ph[0] -= std::floor(S.ph[0]);
    else if (S.ph[v] > 1.0f) S.ph[v] = S.playing ? 0.0f : 1.0f;
    else if (S.ph[v] < 0.0f) S.ph[v] = 0.0f;

    S.sigma += (EC_SIGMAS[S.season] - S.sigma) * (1.0f - std::exp(-4.0f * dt));

    double cx = app.cursorFb[0], cy = app.cursorFb[1];
    if (mouse && app.cursorOk && v == 0) {
        if (S.mousePrev) {
            S.az -= ((float)cx - S.lastX) * 0.006f;
            S.el = std::max(0.08f, std::min(1.45f, S.el + ((float)cy - S.lastY) * 0.006f));
        }
        S.lastX = (float)cx;
        S.lastY = (float)cy;
        S.mousePrev = true;
    } else {
        S.mousePrev = false;
    }
    ecCamera(S);
}

void drawEclipse(AppContext& app, VkCommandBuffer cmd, float aspect) {
    EclipseState& S = app.ec;
    S.aspect = aspect;
    ecCamera(S);
    PushData p{};
    p.a[0] = (float)S.view;
    p.a[1] = aspect;
    p.a[2] = S.clock;
    p.a[3] = S.swap ? 1.0f : 0.0f;
    for (int i = 0; i < 3; i++) {
        p.b[i] = S.camPos[i];
        p.c[i] = S.F[i];
        p.d[i] = S.R[i];
        p.e[i] = S.U[i];
    }
    p.b[3] = S.tanf;
    p.c[3] = S.ph[0];
    p.d[3] = S.sigma;
    EcGround g = ecGround(S);
    p.f[0] = g.sepX;
    p.f[1] = g.sepY;
    p.f[2] = g.rs;
    p.f[3] = g.rm;
    p.g[0] = 1.3f;
    p.g[1] = g.exposed;
    EcLunar l = ecLunar(S);
    p.g[2] = l.xs;
    p.g[3] = l.ys;
    p.h[0] = EC_LUN_RU;
    p.h[1] = EC_LUN_RP;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.pipeline);
    vkCmdPushConstants(cmd, S.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushData), &p);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}
