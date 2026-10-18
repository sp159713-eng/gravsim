#include "vk_app.h"
#include "gpu.h"
#include "earthmoon_geom.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<char> readSpvFile(const std::string& path);
VkShaderModule createSM(VkDevice dev, const std::vector<char>& code);

namespace {

struct PushData {
    float a[4], b[4], c[4], d[4], e[4];
};

void setCamera(AppContext& app) {
    Camera& c = app.camera;
    c = Camera{};
    c.free = false;
    c.yaw = 0.3f;
    c.pitch = 0.5f;
    c.dist = 12.5f;
    c.minDist = 2.2f;
    c.nearPlane = 0.1f;
    c.target[0] = c.target[1] = c.target[2] = 0.0f;
}

void tideLevels(float days, float& level, float& lo, float& hi, float& slope) {
    em::V3 m = em::moonDir(em::phiOf(days));
    float theta = em::TWO_PI * days;
    lo = 1e9f;
    hi = -1e9f;
    for (int i = 0; i < 24; i++) {
        float h = em::tideAt(em::cityNormal(em::TWO_PI * (float)i / 24.0f), m);
        lo = std::min(lo, h);
        hi = std::max(hi, h);
    }
    level = em::tideAt(em::cityNormal(theta), m);
    slope = em::tideAt(em::cityNormal(theta + 0.05f), m) - level;
}

}

void createEarthMoon(AppContext& app) {
    EarthMoonState& S = app.em;
    VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushData)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &push};
    if (vkCreatePipelineLayout(app.device, &layoutInfo, nullptr, &S.layout) != VK_SUCCESS)
        throw std::runtime_error("failed earth moon layout!");
    VkShaderModule vertSM = createSM(app.device, readSpvFile(shaderPath("sky.vert")));
    VkShaderModule fragSM = createSM(app.device, readSpvFile(shaderPath("earthmoon.frag")));
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
        throw std::runtime_error("failed earth moon pipeline!");
    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
}

void destroyEarthMoon(AppContext& app) {
    vkDestroyPipeline(app.device, app.em.pipeline, nullptr);
    vkDestroyPipelineLayout(app.device, app.em.layout, nullptr);
}

void emEnter(AppContext& app) {
    EarthMoonState& S = app.em;
    S.ready = true;
    S.days = 0.0f;
    S.clock = 0.0f;
    S.highCount = 0;
    S.wasHigh = false;
    S.tideState = 0;
    app.showSky = false;
    setCamera(app);
}

void emRestart(AppContext& app) {
    emEnter(app);
}

void emUpdate(AppContext& app, float dt) {
    EarthMoonState& S = app.em;
    app.camera.zoomImpulse = 0.0f;
    dt = std::min(dt, 0.1f);
    S.clock += dt;
    if (!S.paused) S.days += dt * 2.0f * S.speed;
    float level, lo, hi, slope;
    tideLevels(S.days, level, lo, hi, slope);
    float f = hi > lo ? (level - lo) / (hi - lo) : 0.5f;
    bool high = S.wasHigh ? f > 0.55f : f > 0.75f;
    if (high && !S.wasHigh) S.highCount++;
    S.wasHigh = high;
    if (high) S.tideState = 2;
    else if (f < 0.25f) S.tideState = 0;
    else S.tideState = slope > 0.0f ? 1 : 3;
}

void drawEarthMoon(AppContext& app, VkCommandBuffer cmd, float aspect) {
    const EarthMoonState& S = app.em;
    const Camera& c = app.camera;
    float eye[3];
    cameraEye(c, eye);
    float cp = std::cos(c.pitch), sp = std::sin(c.pitch), sy = std::sin(c.yaw), cy = std::cos(c.yaw);
    float f[3] = {-cp * sy, -sp, -cp * cy};
    float rl = std::sqrt(f[2] * f[2] + f[0] * f[0]);
    float r[3] = {rl > 1e-6f ? -f[2] / rl : 0.0f, 0.0f, rl > 1e-6f ? f[0] / rl : 0.0f};
    float u[3] = {r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0]};
    float H = (float)app.swapchainExtent.height, W = (float)app.swapchainExtent.width;
    float k = H / 720.0f;
    float rad = 80.0f * k;
    float cxPx = 1.22f * rad + 20.0f * k;
    float cyPx = H - 270.0f * k;
    PushData p{};
    p.a[0] = eye[0]; p.a[1] = eye[1]; p.a[2] = eye[2]; p.a[3] = aspect;
    p.b[0] = f[0]; p.b[1] = f[1]; p.b[2] = f[2]; p.b[3] = std::tan(30.0f * 3.14159265f / 180.0f);
    p.c[0] = r[0]; p.c[1] = r[1]; p.c[2] = r[2]; p.c[3] = em::phiOf(S.days);
    p.d[0] = u[0]; p.d[1] = u[1]; p.d[2] = u[2]; p.d[3] = em::TWO_PI * S.days;
    p.e[0] = 2.0f * cxPx / W - 1.0f;
    p.e[1] = 2.0f * cyPx / H - 1.0f;
    p.e[2] = 2.0f * rad / H;
    p.e[3] = S.clock;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.pipeline);
    vkCmdPushConstants(cmd, S.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushData), &p);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}
