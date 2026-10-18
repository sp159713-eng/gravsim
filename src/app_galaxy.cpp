#include "vk_app.h"
#include "gpu.h"
#include "milkyway.h"
#include "realstars.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

std::vector<char> readSpvFile(const std::string& path);
VkShaderModule createSM(VkDevice dev, const std::vector<char>& code);
uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags properties);

namespace {

constexpr unsigned STAR_COUNT = 150000;
constexpr float HAZE_GAIN = 0.43f;
constexpr size_t HAZE_STRIDE = 35;

struct GalaxyPush {
    float viewProj[16];
    float params[4];
    float params2[4];
};

void createHaze(AppContext& app, const std::vector<GalaxyPoint>& all) {
    std::vector<GalaxyPoint> haze;
    for (size_t i = 0; i < all.size(); i += HAZE_STRIDE) haze.push_back(all[i]);
    app.hazeCount = (uint32_t)haze.size();
    VkDeviceSize size = haze.size() * sizeof(GalaxyPoint);
    VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    binfo.size = size;
    binfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    binfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(app.device, &binfo, nullptr, &app.hazeBuffer) != VK_SUCCESS)
        throw std::runtime_error("failed haze buffer!");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(app.device, app.hazeBuffer, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(app.device, &alloc, nullptr, &app.hazeMemory) != VK_SUCCESS)
        throw std::runtime_error("failed haze memory!");
    vkBindBufferMemory(app.device, app.hazeBuffer, app.hazeMemory, 0);
    void* data;
    vkMapMemory(app.device, app.hazeMemory, 0, size, 0, &data);
    std::memcpy(data, haze.data(), (size_t)size);
    vkUnmapMemory(app.device, app.hazeMemory);
}

void eyeLy(const AppContext& app, float out[3]) {
    float eye[3], sun[3];
    cameraEye(app.camera, eye);
    sunPosition(app, sun);
    float k = LY_PER_AU_INV / solarUnitsPerAu(app.trueScale);
    for (int a = 0; a < 3; a++) out[a] = (eye[a] - sun[a]) * k + app.visit.offLy[a];
}

}

void galaxyEye(const AppContext& app, float out[3]) {
    eyeLy(app, out);
}

void sunPosition(const AppContext& app, float out[3]) {
    for (int a = 0; a < 3; a++) out[a] = app.displayValid ? app.displayBodies[0].pos[a] : 0.0f;
}

float galaxyDistanceLy(const AppContext& app) {
    float e[3];
    eyeLy(app, e);
    return std::sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
}

bool galaxyActive(const AppContext& app) {
    return app.scene == Scene::SOLAR && app.showGalaxy;
}

void galaxyViewProj(const AppContext& app, float aspect, float out[16]) {
    float e[3];
    eyeLy(app, e);
    cameraViewProjEx(app.camera, aspect, e, 1e-3f, 1e6f, out);
}

void createGalaxy(AppContext& app) {
    std::vector<GalaxyPoint> pts = makeMilkyWay(STAR_COUNT, 12345);
    app.galaxyCount = (uint32_t)pts.size();
    app.galaxyPts = pts;
    VkDeviceSize size = pts.size() * sizeof(GalaxyPoint);
    VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    binfo.size = size;
    binfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    binfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(app.device, &binfo, nullptr, &app.galaxyBuffer) != VK_SUCCESS)
        throw std::runtime_error("failed galaxy buffer!");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(app.device, app.galaxyBuffer, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(app.device, &alloc, nullptr, &app.galaxyMemory) != VK_SUCCESS)
        throw std::runtime_error("failed galaxy memory!");
    vkBindBufferMemory(app.device, app.galaxyBuffer, app.galaxyMemory, 0);
    void* data;
    vkMapMemory(app.device, app.galaxyMemory, 0, size, 0, &data);
    std::memcpy(data, pts.data(), (size_t)size);
    app.galaxyMapped = static_cast<GalaxyPoint*>(data);

    VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GalaxyPush)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &push};
    if (vkCreatePipelineLayout(app.device, &layoutInfo, nullptr, &app.galaxyLayout) != VK_SUCCESS)
        throw std::runtime_error("failed galaxy layout!");

    VkShaderModule vertSM = createSM(app.device, readSpvFile(shaderPath("galaxy.vert")));
    VkShaderModule fragSM = createSM(app.device, readSpvFile(shaderPath("galaxy.frag")));
    VkPipelineShaderStageCreateInfo stages[2]{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertSM, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragSM, "main", nullptr}
    };
    VkVertexInputBindingDescription bind{0, sizeof(GalaxyPoint), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[2]{
        {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 16}
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
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY, 1, &attBlend, {0,0,0,0}};
    VkDynamicState dyns[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, dyns};
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, stages, &vertInput, &inputAss, nullptr, &vpState, &raster, &multi, nullptr, &blend, &dyn, app.galaxyLayout, app.renderPass, 0, VK_NULL_HANDLE, -1};
    if (vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &info, nullptr, &app.galaxyPipeline) != VK_SUCCESS)
        throw std::runtime_error("failed galaxy pipeline!");
    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
    createHaze(app, pts);
    app.galaxyLabels = milkyWayLabels();
    galFrameHome(app);
}

void galaxyUpload(AppContext& app, const std::vector<GalaxyPoint>& pts, const std::vector<GalaxyLabel>& labels) {
    vkDeviceWaitIdle(app.device);
    app.galaxyPts = pts;
    app.galaxyCount = (uint32_t)pts.size();
    std::memcpy(app.galaxyMapped, pts.data(), pts.size() * sizeof(GalaxyPoint));
    std::vector<GalaxyPoint> haze;
    for (size_t i = 0; i < pts.size(); i += HAZE_STRIDE) haze.push_back(pts[i]);
    app.hazeCount = (uint32_t)haze.size();
    void* data;
    vkMapMemory(app.device, app.hazeMemory, 0, haze.size() * sizeof(GalaxyPoint), 0, &data);
    std::memcpy(data, haze.data(), haze.size() * sizeof(GalaxyPoint));
    vkUnmapMemory(app.device, app.hazeMemory);
    app.galaxyLabels = labels;
}

void drawGalaxy(AppContext& app, VkCommandBuffer cmd, float aspect) {
    if (!galaxyActive(app)) return;
    float gf = galaxyLayerFade(app);
    gf *= 1.0f - 0.85f * tourOortAlpha(app);
    if (gf <= 0.003f) return;
    GalaxyPush push;
    galaxyViewProj(app, aspect, push.viewProj);
    float sc = app.gal.scale;
    float dn = galaxyDistanceLy(app) / sc;
    float t = std::clamp(std::log10(std::max(dn, 1.0f) / 3000.0f) / 1.5f + 0.35f, 0.0f, 1.0f);
    push.params[0] = (0.2f * (1.0f - t) + 0.028f * t) * gf;
    push.params[1] = 6.0f;
    push.params[2] = 30000.0f * sc;
    push.params[3] = 18.0f;
    push.params2[0] = 16.0f;
    push.params2[1] = push.params2[2] = push.params2[3] = 0.0f;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.galaxyPipeline);
    vkCmdPushConstants(cmd, app.galaxyLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GalaxyPush), &push);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &app.galaxyBuffer, &offset);
    vkCmdDraw(cmd, app.galaxyCount, 1, 0, 0);
    float near01 = 1.0f - std::clamp(std::log10(std::max(dn, 1.0f) / 4000.0f) / 0.8f, 0.0f, 1.0f);
    near01 = near01 * near01 * (3.0f - 2.0f * near01);
    if (near01 > 0.004f) {
        push.params[0] = HAZE_GAIN * near01 * gf;
        push.params[1] = 70.0f;
        push.params[2] = 0.0f;
        push.params[3] = 70.0f;
        push.params2[0] = 140.0f;
        push.params2[1] = 1.0f;
        vkCmdPushConstants(cmd, app.galaxyLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GalaxyPush), &push);
        vkCmdBindVertexBuffers(cmd, 0, 1, &app.hazeBuffer, &offset);
        vkCmdDraw(cmd, app.hazeCount, 1, 0, 0);
    }
}

void destroyGalaxy(AppContext& app) {
    vkDestroyPipeline(app.device, app.galaxyPipeline, nullptr);
    vkDestroyPipelineLayout(app.device, app.galaxyLayout, nullptr);
    vkDestroyBuffer(app.device, app.galaxyBuffer, nullptr);
    vkFreeMemory(app.device, app.galaxyMemory, nullptr);
    vkDestroyBuffer(app.device, app.hazeBuffer, nullptr);
    vkFreeMemory(app.device, app.hazeMemory, nullptr);
}

int galaxyRealStar(const AppContext& app, int idx) {
    if (app.gal.active) return -1;
    int base = (int)app.galaxyPts.size() - realStarCount();
    return base >= 0 && idx >= base && idx < (int)app.galaxyPts.size() ? idx - base : -1;
}

static void realStarLabels(AppContext& app, std::vector<ScreenLabel>& out) {
    if (app.gal.active || galaxyLayerFade(app) < 0.3f || tourOortAlpha(app) > 0.01f) return;
    float eye[3];
    galaxyEye(app, eye);
    float local = 0.0f;
    for (int a = 0; a < 3; a++) local += (eye[a] - app.visit.offLy[a]) * (eye[a] - app.visit.offLy[a]);
    if (std::sqrt(local) < 0.05f) return;
    float aspect = (float)app.swapchainExtent.width / (float)app.swapchainExtent.height;
    float vp[16];
    galaxyViewProj(app, aspect, vp);
    int base = (int)app.galaxyPts.size() - realStarCount();
    if (base < 0) return;
    for (int i = 0; i < realStarCount(); i++) {
        if (app.visit.active && (uint32_t)(base + i) == app.visit.star) continue;
        const GalaxyPoint& g = app.galaxyPts[base + i];
        float d2 = 0.0f;
        for (int a = 0; a < 3; a++) d2 += (g.pos[a] - eye[a]) * (g.pos[a] - eye[a]);
        float d = std::sqrt(d2), lum = realStarAt(i).lum;
        float range = 45.0f + (lum > 500.0f ? 3000.0f : lum > 5.0f ? 120.0f : 0.0f);
        if (d > range || d < 0.3f) continue;
        float clip[4];
        for (int row = 0; row < 4; row++)
            clip[row] = vp[row] * g.pos[0] + vp[row + 4] * g.pos[1] + vp[row + 8] * g.pos[2] + vp[row + 12];
        if (clip[3] <= 1e-9f) continue;
        float f = galaxyLayerFade(app);
        ScreenLabel s{clip[0] / clip[3], clip[1] / clip[3], realStarAt(i).name, {g.color[0] * f, g.color[1] * f, g.color[2] * f}};
        s.prio = 2.0f + (realStarAt(i).planetCount ? 0.0f : 0.5f);
        out.push_back(s);
    }
}

void galaxyScreenLabels(AppContext& app, std::vector<ScreenLabel>& out) {
    if (closeupActive(app)) return;
    if (galaxyActive(app)) {
        realStarLabels(app, out);
        nebulaLabels(app, out);
        closeupLabels(app, out);
    }
    if (!galaxyActive(app) || galaxyDistanceLy(app) < GALAXY_LABEL_LY) return;
    float gf = galaxyLayerFade(app);
    if (gf < 0.3f) return;
    float aspect = (float)app.swapchainExtent.width / (float)app.swapchainExtent.height;
    float vp[16];
    galaxyViewProj(app, aspect, vp);
    for (const GalaxyLabel& l : app.galaxyLabels) {
        float clip[4];
        for (int row = 0; row < 4; row++)
            clip[row] = vp[row] * l.pos[0] + vp[row + 4] * l.pos[1] + vp[row + 8] * l.pos[2] + vp[row + 12];
        if (clip[3] <= 1e-9f) continue;
        if (app.visit.active && std::strcmp(l.text, "SUN - YOU ARE HERE") == 0) continue;
        if (std::strcmp(l.text, "ORION SPUR") == 0 && galaxyDistanceLy(app) > 30000.0f) continue;
        bool sun = !app.gal.active && &l == &app.galaxyLabels[0];
        float nx = clip[0] / clip[3], ny = clip[1] / clip[3];
        ScreenLabel s{nx, ny, l.text, {l.color[0] * gf, l.color[1] * gf, l.color[2] * gf}};
        s.prio = sun ? 0.0f : &l == &app.galaxyLabels[app.gal.active ? 0 : 1] ? 1.0f : 3.0f;
        out.push_back(s);
        if (sun) {
            ScreenLabel ring{nx, ny, nullptr, {1.0f * gf, 0.78f * gf, 0.3f * gf}};
            ring.ring = 7.0f;
            out.push_back(ring);
        }
    }
}
