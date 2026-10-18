#include "vk_app.h"
#include "gpu.h"
#include "expansion.h"
#include "cosmo.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<char> readSpvFile(const std::string& path);
VkShaderModule createSM(VkDevice dev, const std::vector<char>& code);

float g_exAtGyr = -1.0f;

namespace {

ExpansionState S;

struct PushData {
    float col[4];
    float p[4];
};

bool keyDown(AppContext& app, int k) { return glfwGetKey(app.window, k) == GLFW_PRESS; }

bool edge(int idx, bool down) {
    bool fire = down && !S.keyPrev[idx];
    S.keyPrev[idx] = down;
    return fire;
}

void clampTime() {
    S.tSec = std::clamp(S.tSec, EX_TMIN, EX_TEND_GYR * cosmo::GYR_S);
}

}

ExpansionState& exState() { return S; }

void createExpansion(AppContext& app) {
    VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushData)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &push};
    if (vkCreatePipelineLayout(app.device, &layoutInfo, nullptr, &S.layout) != VK_SUCCESS)
        throw std::runtime_error("failed expansion layout!");
    VkShaderModule vertSM = createSM(app.device, readSpvFile(shaderPath("sky.vert")));
    VkShaderModule fragSM = createSM(app.device, readSpvFile(shaderPath("expansion.frag")));
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
        throw std::runtime_error("failed expansion pipeline!");
    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
}

void destroyExpansion(AppContext& app) {
    vkDestroyPipeline(app.device, S.pipeline, nullptr);
    vkDestroyPipelineLayout(app.device, S.layout, nullptr);
}

void exEnter(AppContext& app) {
    S.ready = true;
    S.playing = false;
    S.glideTo = -1.0;
    S.clock = 0.0f;
    double now = cosmo::nowSec();
    S.tSec = now;
    if (g_exAtGyr >= 0.0f) {
        double t = (double)g_exAtGyr * cosmo::GYR_S;
        S.tSec = std::fabs(t - now) < 0.05 * cosmo::GYR_S ? now : t;
    }
    g_exAtGyr = -1.0f;
    clampTime();
    for (bool& k : S.keyPrev) k = true;
    app.showSky = false;
    app.camera.zoomImpulse = 0.0f;
    app.camera.wheelHold = true;
    app.camera.wheelAcc = 0.0f;
}

void exLeave(AppContext& app) {
    app.camera.wheelHold = false;
    app.camera.wheelAcc = 0.0f;
    S.ready = false;
}

void exUpdate(AppContext& app, float dt) {
    dt = std::min(dt, 0.1f);
    S.clock += dt;
    app.camera.zoomImpulse = 0.0f;
    app.camera.wheelHold = true;
    app.camera.wheelAcc = 0.0f;
    bool shift = keyDown(app, GLFW_KEY_LEFT_SHIFT) || keyDown(app, GLFW_KEY_RIGHT_SHIFT);
    const double endT = EX_TEND_GYR * cosmo::GYR_S;

    if (edge(0, keyDown(app, GLFW_KEY_SPACE))) {
        if (!S.playing && S.tSec >= endT * 0.999) S.tSec = EX_TMIN;
        S.playing = !S.playing;
        S.glideTo = -1.0;
    }
    if (edge(1, keyDown(app, GLFW_KEY_N))) {
        S.glideTo = cosmo::nowSec();
        S.playing = false;
    }
    const std::vector<ExEra>& E = exEras();
    int cur = exEraAt(S.tSec);
    if (edge(2, keyDown(app, GLFW_KEY_DOWN))) {
        S.glideTo = E[std::min((int)E.size() - 1, cur + 1)].t0 * 1.0005;
        S.playing = false;
    }
    if (edge(3, keyDown(app, GLFW_KEY_UP))) {
        double st = E[cur].t0;
        int tgt = S.tSec > st * 1.01 && cur > 0 && exTimeToU(S.tSec) - exTimeToU(st) > 0.004 ? cur : std::max(0, cur - 1);
        S.glideTo = E[tgt].t0 * 1.0005;
        S.playing = false;
    }

    int dir = (keyDown(app, GLFW_KEY_RIGHT) ? 1 : 0) - (keyDown(app, GLFW_KEY_LEFT) ? 1 : 0);
    if (dir != 0) {
        S.playing = false;
        S.glideTo = -1.0;
        double u = exTimeToU(S.tSec) + dir * 0.07 * (shift ? 4.0 : 1.0) * dt;
        S.tSec = exUToTime(u);
    } else if (S.glideTo > 0.0) {
        double u = exTimeToU(S.tSec), ut = exTimeToU(S.glideTo);
        u += (ut - u) * (1.0 - std::exp(-7.0 * (double)dt));
        if (std::fabs(ut - u) < 2e-4) {
            S.tSec = S.glideTo;
            S.glideTo = -1.0;
        } else {
            S.tSec = exUToTime(u);
        }
    } else if (S.playing) {
        double m = shift ? 3.0 : 1.0;
        if (S.tSec < 1.0) {
            S.tSec = std::min(1.0, S.tSec * std::pow(10.0, 6.0 * m * dt));
        } else {
            double gyr = S.tSec / cosmo::GYR_S;
            double rate = gyr < 0.35 ? 4.0 * gyr : 1.4;
            S.tSec = (gyr + rate * m * dt) * cosmo::GYR_S;
        }
        if (S.tSec >= endT) {
            S.tSec = endT;
            S.playing = false;
        }
    }
    clampTime();
}

void drawExpansion(AppContext& app, VkCommandBuffer cmd, float aspect) {
    double a = cosmo::scaleAt(S.tSec);
    float rgb[3];
    exTempColor(cosmo::tempK(a), rgb);
    float H = (float)app.swapchainExtent.height;
    float k = H / 720.0f;
    PushData p{};
    p.col[0] = rgb[0];
    p.col[1] = rgb[1];
    p.col[2] = rgb[2];
    p.p[0] = 132.0f * k / H;
    p.p[1] = 1.0f - 262.0f * k / H;
    p.p[2] = 0.0f;
    p.p[3] = aspect;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.pipeline);
    vkCmdPushConstants(cmd, S.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushData), &p);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}
