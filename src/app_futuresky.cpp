#include "vk_app.h"
#include "gpu.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<char> readSpvFile(const std::string& path);
VkShaderModule createSM(VkDevice dev, const std::vector<char>& code);

double g_fsStartAt = -1.0;

namespace {

struct PushData {
    float a[4], b[4], c[4], d[4], e[4], f[4], g[4];
};

}

void createFutureSky(AppContext& app) {
    FutureSkyState& S = app.fs;
    VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushData)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &push};
    if (vkCreatePipelineLayout(app.device, &layoutInfo, nullptr, &S.layout) != VK_SUCCESS)
        throw std::runtime_error("failed future sky layout!");
    VkShaderModule vertSM = createSM(app.device, readSpvFile(shaderPath("sky.vert")));
    VkShaderModule fragSM = createSM(app.device, readSpvFile(shaderPath("futuresky.frag")));
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
        throw std::runtime_error("failed future sky pipeline!");
    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
}

void destroyFutureSky(AppContext& app) {
    vkDestroyPipeline(app.device, app.fs.pipeline, nullptr);
    vkDestroyPipelineLayout(app.device, app.fs.layout, nullptr);
}

void fsEnter(AppContext& app) {
    FutureSkyState& S = app.fs;
    S.ready = true;
    S.drag = false;
    S.mousePrev = false;
    S.clock = 0.0f;
    S.t = 0.0f;
    if (g_fsStartAt >= 0.0) {
        S.t = std::min(FS_MAX_GYR, (float)g_fsStartAt);
        g_fsStartAt = -1.0;
    }
    app.showSky = false;
    app.camera.zoomImpulse = 0.0f;
}

bool fsUpdate(AppContext& app, float dt, bool play, int step, bool fast, bool mouse) {
    FutureSkyState& S = app.fs;
    app.camera.zoomImpulse = 0.0f;
    dt = std::min(dt, 0.1f);
    S.clock += dt;
    S.playing = play;
    if (play && S.t >= FS_MAX_GYR - 1e-4f) S.t = 0.0f;
    if (step != 0) S.t += (float)step * dt * (fast ? 3.0f : 0.8f);
    bool ended = false;
    if (play && step == 0 && !S.drag) {
        S.t += dt / 8.0f;
        if (S.t >= FS_MAX_GYR) ended = true;
    }
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float x0, x1, y;
    fsSlider(W, H, x0, x1, y);
    float k = H / 720.0f;
    double cx = app.cursorFb[0], cy = app.cursorFb[1];
    if (mouse && !S.mousePrev && app.cursorOk && cx >= x0 - 14.0f * k && cx <= x1 + 14.0f * k && std::fabs(cy - y) < 26.0f * k) S.drag = true;
    if (!mouse) S.drag = false;
    if (S.drag) S.t = FS_MAX_GYR * (float)std::min(1.0, std::max(0.0, (cx - x0) / (x1 - x0)));
    S.mousePrev = mouse;
    S.t = std::min(FS_MAX_GYR, std::max(0.0f, S.t));
    return ended;
}

void drawFutureSky(AppContext& app, VkCommandBuffer cmd, float aspect) {
    const FutureSkyState& S = app.fs;
    FsParams P;
    fsEval(S.t, P);
    PushData p{};
    p.a[0] = S.t; p.a[1] = aspect; p.a[2] = S.clock; p.a[3] = P.sun;
    p.b[0] = P.band; p.b[1] = P.warp; p.b[2] = P.burst;
    p.c[0] = P.a.az; p.c[1] = P.a.el; p.c[2] = std::exp(P.a.lnR); p.c[3] = P.a.tilt;
    p.d[0] = P.a.q; p.d[1] = P.a.disc; p.d[2] = P.a.ell; p.d[3] = P.a.tide;
    p.e[0] = P.b.az; p.e[1] = P.b.el; p.e[2] = std::exp(P.b.lnR); p.e[3] = P.b.tilt;
    p.f[0] = P.b.q; p.f[1] = P.b.disc; p.f[2] = P.b.ell; p.f[3] = P.b.tide;
    p.g[0] = P.a.bright; p.g[1] = P.b.bright;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.pipeline);
    vkCmdPushConstants(cmd, S.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushData), &p);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}
