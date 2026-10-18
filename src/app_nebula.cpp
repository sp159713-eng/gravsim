#include "vk_app.h"
#include "gpu.h"
#include "nebulae.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

std::vector<char> readSpvFile(const std::string& path);
VkShaderModule createSM(VkDevice dev, const std::vector<char>& code);
uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags properties);

namespace {

struct NebPush {
    float viewProj[16];
    float params[4];
};

constexpr float PX_SCALE = 0.866f;

float nebulaGain(const AppContext& app) {
    float d = galaxyDistanceLy(app);
    float g = std::clamp((d - 60.0f) / 120.0f, 0.0f, 1.0f);
    return g * galaxyLayerFade(app);
}

bool project(const float vp[16], const float p[3], float& x, float& y, float& w) {
    float clip[4];
    for (int row = 0; row < 4; row++)
        clip[row] = vp[row] * p[0] + vp[row + 4] * p[1] + vp[row + 8] * p[2] + vp[row + 12];
    w = clip[3];
    if (w <= 1e-6f) return false;
    x = clip[0] / w;
    y = clip[1] / w;
    return true;
}

std::string commas(long long v) {
    std::string s = std::to_string(v);
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ",");
    return s;
}

}

void createNebulae(AppContext& app) {
    std::vector<NebulaVertex> pts = makeNebulae();
    app.nebCount = (uint32_t)pts.size();
    VkDeviceSize size = pts.size() * sizeof(NebulaVertex);
    VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    binfo.size = size;
    binfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    binfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(app.device, &binfo, nullptr, &app.nebBuffer) != VK_SUCCESS) throw std::runtime_error("failed nebula buffer!");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(app.device, app.nebBuffer, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(app.device, &alloc, nullptr, &app.nebMemory) != VK_SUCCESS) throw std::runtime_error("failed nebula memory!");
    vkBindBufferMemory(app.device, app.nebBuffer, app.nebMemory, 0);
    void* data;
    vkMapMemory(app.device, app.nebMemory, 0, size, 0, &data);
    std::memcpy(data, pts.data(), (size_t)size);
    vkUnmapMemory(app.device, app.nebMemory);

    VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(NebPush)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &push};
    if (vkCreatePipelineLayout(app.device, &layoutInfo, nullptr, &app.nebLayout) != VK_SUCCESS) throw std::runtime_error("failed nebula layout!");
    VkShaderModule vertSM = createSM(app.device, readSpvFile(shaderPath("nebula.vert")));
    VkShaderModule fragSM = createSM(app.device, readSpvFile(shaderPath("nebula.frag")));
    VkPipelineShaderStageCreateInfo stages[2]{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertSM, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragSM, "main", nullptr}
    };
    VkVertexInputBindingDescription bind{0, sizeof(NebulaVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[2]{
        {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16}
    };
    VkPipelineVertexInputStateCreateInfo vertInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0, 1, &bind, 2, attrs};
    VkPipelineInputAssemblyStateCreateInfo inputAss{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_FALSE};
    VkPipelineViewportStateCreateInfo vpState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0, 0, 0, 1.0f};
    VkPipelineMultisampleStateCreateInfo multi{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0, nullptr, VK_FALSE, VK_FALSE};
    VkPipelineColorBlendAttachmentState attBlend{};
    attBlend.colorWriteMask = 0xF;
    attBlend.blendEnable = VK_TRUE;
    attBlend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    attBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    attBlend.colorBlendOp = VK_BLEND_OP_ADD;
    attBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attBlend.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY, 1, &attBlend, {0, 0, 0, 0}};
    VkDynamicState dyns[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, dyns};
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, stages, &vertInput, &inputAss, nullptr, &vpState, &raster, &multi, nullptr, &blend, &dyn, app.nebLayout, app.renderPass, 0, VK_NULL_HANDLE, -1};
    if (vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &info, nullptr, &app.nebPipeline) != VK_SUCCESS) throw std::runtime_error("failed nebula pipeline!");
    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
    createCloseup(app);
}

void drawNebulae(AppContext& app, VkCommandBuffer cmd, float aspect) {
    if (!galaxyActive(app) || app.gal.active) return;
    float gain = nebulaGain(app);
    if (gain <= 0.003f) return;
    NebPush push;
    galaxyViewProj(app, aspect, push.viewProj);
    push.params[0] = PX_SCALE * (float)app.swapchainExtent.height;
    push.params[1] = 0.9f * gain;
    push.params[2] = 7.0f;
    push.params[3] = 220.0f;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.nebPipeline);
    vkCmdPushConstants(cmd, app.nebLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(NebPush), &push);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &app.nebBuffer, &offset);
    vkCmdDraw(cmd, app.nebCount, 1, 0, 0);
}

void destroyNebulae(AppContext& app) {
    destroyCloseup(app);
    vkDestroyPipeline(app.device, app.nebPipeline, nullptr);
    vkDestroyPipelineLayout(app.device, app.nebLayout, nullptr);
    vkDestroyBuffer(app.device, app.nebBuffer, nullptr);
    vkFreeMemory(app.device, app.nebMemory, nullptr);
}

void nebulaLabels(AppContext& app, std::vector<ScreenLabel>& out) {
    if (!galaxyActive(app) || app.gal.active || nebulaGain(app) < 0.3f) return;
    float aspect = (float)app.swapchainExtent.width / (float)app.swapchainExtent.height;
    float vp[16];
    galaxyViewProj(app, aspect, vp);
    float f = nebulaGain(app);
    for (const NebulaInfo& n : nebulaInfos()) {
        float x, y, w;
        if (!project(vp, n.pos, x, y, w)) continue;
        ScreenLabel s{x, y, n.label.c_str(), {n.color[0] * f, n.color[1] * f, n.color[2] * f}};
        s.prio = 2.2f;
        out.push_back(s);
    }
}

int nebulaPick(const AppContext& app, double fx, double fy) {
    if (!galaxyActive(app) || app.gal.active || nebulaGain(app) < 0.15f) return -1;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float vp[16];
    galaxyViewProj(app, W / H, vp);
    int best = -1;
    float bestD = 1e18f;
    const std::vector<NebulaInfo>& infos = nebulaInfos();
    for (size_t i = 0; i < infos.size(); i++) {
        float x, y, w;
        if (!project(vp, infos[i].pos, x, y, w)) continue;
        float sx = (x + 1.0f) * 0.5f * W - (float)fx, sy = (y + 1.0f) * 0.5f * H - (float)fy;
        float rpx = std::clamp(0.5f * infos[i].diameterLy * PX_SCALE * H / w, 16.0f, 200.0f);
        float d2 = sx * sx + sy * sy;
        if (d2 < rpx * rpx && d2 < bestD) { bestD = d2; best = (int)i; }
    }
    return best;
}

std::vector<CardLine> nebulaCard(const AppContext& app) {
    std::vector<CardLine> lines;
    const std::vector<NebulaInfo>& infos = nebulaInfos();
    if (app.nebSel < 0 || app.nebSel >= (int)infos.size()) return lines;
    const NebulaInfo& n = infos[app.nebSel];
    const float white[3] = {1.0f, 1.0f, 1.0f}, text[3] = {0.85f, 0.87f, 0.92f}, fact[3] = {0.7f, 0.85f, 1.0f}, hint[3] = {0.6f, 0.65f, 0.75f};
    lines.push_back({n.name, {white[0], white[1], white[2]}});
    lines.push_back({n.type, {n.color[0], n.color[1], n.color[2]}});
    lines.push_back({"DISTANCE " + commas((long long)n.distLy) + " LY", {text[0], text[1], text[2]}});
    lines.push_back({"SIZE ~" + commas((long long)std::llround(n.diameterLy)) + " LY ACROSS", {text[0], text[1], text[2]}});
    lines.push_back({"LIGHT LEFT IT ~" + commas((long long)n.distLy) + " YEARS AGO", {text[0], text[1], text[2]}});
    lines.push_back({n.fact1, {fact[0], fact[1], fact[2]}});
    lines.push_back({n.fact2, {fact[0], fact[1], fact[2]}});
    lines.push_back({"CLICK ELSEWHERE TO CLOSE", {hint[0], hint[1], hint[2]}});
    return lines;
}
