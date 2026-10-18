#include "vk_app.h"
#include "gpu.h"
#include "milkyway.h"
#include "bhdisc.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

std::vector<char> readSpvFile(const std::string& path);
VkShaderModule createSM(VkDevice dev, const std::vector<char>& code);
uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags properties);

namespace {

struct HolePush {
    float viewProj[16];
    float right[4];
    float up[4];
    float back[4];
    float proj[4];
};

struct HoleBase {
    Scene scene = Scene::SOLAR;
    uint32_t n = 0;
    float centreMass = 1.0f;
    float starMass = 1.0f;
};

const HoleBase& holeBase(const AppContext& app) {
    static HoleBase base;
    if (base.scene != app.scene || base.n != app.n) {
        std::vector<Particle> p = makeScene(app.scene, app.n, 7, app.trueScale);
        base.scene = app.scene;
        base.n = app.n;
        base.centreMass = p.empty() ? 1.0f : p[0].pos[3];
        base.starMass = p.size() > 2 ? p[2].pos[3] : 1.0f;
        if (app.scene == Scene::GALAXY && p.size() > 1) base.starMass = p[1].pos[3];
    }
    return base;
}

void cameraBasis(const Camera& c, float r[3], float u[3], float b[3]) {
    float fx = -std::cos(c.pitch) * std::sin(c.yaw);
    float fy = -std::sin(c.pitch);
    float fz = -std::cos(c.pitch) * std::cos(c.yaw);
    float rl = std::sqrt(fz * fz + fx * fx);
    r[0] = rl > 1e-6f ? -fz / rl : 0.0f;
    r[1] = 0.0f;
    r[2] = rl > 1e-6f ? fx / rl : 0.0f;
    u[0] = r[1] * fz - r[2] * fy;
    u[1] = r[2] * fx - r[0] * fz;
    u[2] = r[0] * fy - r[1] * fx;
    b[0] = -fx; b[1] = -fy; b[2] = -fz;
}

void fillHolePush(const AppContext& app, const float viewProj[16], HolePush& push, const Camera* cam = nullptr) {
    std::memcpy(push.viewProj, viewProj, sizeof(push.viewProj));
    float r[3], u[3], b[3];
    cameraBasis(cam ? *cam : app.camera, r, u, b);
    float height = (float)app.swapchainExtent.height;
    float aspect = (float)app.swapchainExtent.width / height;
    float h = 1.0f / std::tan(30.0f * 3.14159265f / 180.0f);
    for (int a = 0; a < 3; a++) {
        push.right[a] = r[a];
        push.up[a] = u[a];
        push.back[a] = b[a];
    }
    push.right[3] = (float)app.simTime;
    push.up[3] = 0.5f * height * h;
    push.proj[0] = h / aspect;
    push.proj[1] = -h;
}

}

uint32_t blackHoleCount(const AppContext& app) {
    if (app.scene == Scene::GALAXY) return app.n >= 1 ? 1 : 0;
    if (collisionLike(app.scene)) return app.n >= 4 ? 2 : (app.n >= 1 ? 1 : 0);
    return 0;
}

void createBlackHoles(AppContext& app) {
    VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(HolePush)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &push};
    if (vkCreatePipelineLayout(app.device, &layoutInfo, nullptr, &app.holeLayout) != VK_SUCCESS)
        throw std::runtime_error("failed hole layout!");

    VkShaderModule vertSM = createSM(app.device, readSpvFile(shaderPath("blackhole.vert")));
    VkShaderModule fragSM = createSM(app.device, readSpvFile(shaderPath("blackhole.frag")));
    VkPipelineShaderStageCreateInfo stages[2]{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertSM, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragSM, "main", nullptr}
    };
    VkVertexInputBindingDescription bind{0, sizeof(Particle), VK_VERTEX_INPUT_RATE_INSTANCE};
    VkVertexInputAttributeDescription attrs[2]{
        {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16}
    };
    VkPipelineVertexInputStateCreateInfo vertInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0, 1, &bind, 2, attrs};
    VkPipelineInputAssemblyStateCreateInfo inputAss{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};
    VkPipelineViewportStateCreateInfo vpState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0, 0, 0, 1.0f};
    VkPipelineMultisampleStateCreateInfo multi{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0, nullptr, VK_FALSE, VK_FALSE};
    VkPipelineColorBlendAttachmentState attBlend{};
    attBlend.colorWriteMask = 0xF;
    attBlend.blendEnable = VK_TRUE;
    attBlend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    attBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    attBlend.colorBlendOp = VK_BLEND_OP_ADD;
    attBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    attBlend.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY, 1, &attBlend, {0, 0, 0, 0}};
    VkDynamicState dyns[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, dyns};
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, stages, &vertInput, &inputAss, nullptr, &vpState, &raster, &multi, nullptr, &blend, &dyn, app.holeLayout, app.renderPass, 0, VK_NULL_HANDLE, -1};
    if (vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &info, nullptr, &app.holePipeline) != VK_SUCCESS)
        throw std::runtime_error("failed hole pipeline!");
    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
}

void drawBlackHoles(AppContext& app, VkCommandBuffer cmd, const float viewProj[16], VkBuffer latest) {
    uint32_t count = app.scene == Scene::BHSIZES ? bhHoleCount(app) : blackHoleCount(app);
    if (count == 0) return;
    HolePush push{};
    fillHolePush(app, viewProj, push);
    push.back[3] = collisionLike(app.scene) ? 1.0f : app.scene == Scene::BHSIZES ? 2.0f : 0.0f;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.holePipeline);
    vkCmdPushConstants(cmd, app.holeLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(HolePush), &push);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &latest, &offset);
    vkCmdDraw(cmd, 6, count, 0, 0);
}

float g_sgrPos[3] = {0.0f, 0.0f, 0.0f};
float g_sgrMass = 4.0e6f;

float sgrRadiusLy(const AppContext& app) {
    float e[3];
    galaxyEye(app, e);
    float dx = e[0] - g_sgrPos[0], dy = e[1] - g_sgrPos[1], dz = e[2] - g_sgrPos[2];
    float d = std::sqrt(dx * dx + dy * dy + dz * dz);
    return std::min(150.0f * app.gal.scale, std::max(1.3e-6f * std::max(g_sgrMass, 1000.0f) / 4.0e6f, 0.01f * d));
}

void sgrCentre(float out[3]) {
    for (int a = 0; a < 3; a++) out[a] = g_sgrPos[a];
}

void sgrSet(AppContext& app, const float pos[3], const float normal[3], float bhMsun) {
    Particle p{};
    for (int a = 0; a < 3; a++) {
        p.pos[a] = pos[a];
        p.vel[a] = normal[a];
        g_sgrPos[a] = pos[a];
    }
    p.pos[3] = bhMsun > 0.0f ? 3.7e10f : 0.0f;
    g_sgrMass = bhMsun;
    void* data;
    vkMapMemory(app.device, app.sgrMemory, 0, sizeof(Particle), 0, &data);
    std::memcpy(data, &p, sizeof(Particle));
    vkUnmapMemory(app.device, app.sgrMemory);
}

void createTourHole(AppContext& app) {
    VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    binfo.size = sizeof(Particle);
    binfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    binfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(app.device, &binfo, nullptr, &app.tourHoleBuffer) != VK_SUCCESS)
        throw std::runtime_error("failed tour hole buffer!");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(app.device, app.tourHoleBuffer, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(app.device, &alloc, nullptr, &app.tourHoleMemory) != VK_SUCCESS)
        throw std::runtime_error("failed tour hole memory!");
    vkBindBufferMemory(app.device, app.tourHoleBuffer, app.tourHoleMemory, 0);
}

void createSgrA(AppContext& app) {
    createTourHole(app);
    float n[3];
    galaxyNormal(n);
    std::vector<GalaxyLabel> labels = milkyWayLabels();
    Particle p{};
    for (int a = 0; a < 3; a++) {
        p.pos[a] = labels[1].pos[a];
        p.vel[a] = n[a];
        g_sgrPos[a] = p.pos[a];
    }
    p.pos[3] = 3.7e10f;
    VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    binfo.size = sizeof(Particle);
    binfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    binfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(app.device, &binfo, nullptr, &app.sgrBuffer) != VK_SUCCESS)
        throw std::runtime_error("failed sgr buffer!");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(app.device, app.sgrBuffer, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(app.device, &alloc, nullptr, &app.sgrMemory) != VK_SUCCESS)
        throw std::runtime_error("failed sgr memory!");
    vkBindBufferMemory(app.device, app.sgrBuffer, app.sgrMemory, 0);
    void* data;
    vkMapMemory(app.device, app.sgrMemory, 0, sizeof(Particle), 0, &data);
    std::memcpy(data, &p, sizeof(Particle));
    vkUnmapMemory(app.device, app.sgrMemory);
}

void drawSgrA(AppContext& app, VkCommandBuffer cmd, float aspect) {
    if (!galaxyActive(app)) return; if (galaxyLayerFade(app) <= 0.003f) return;
    float viewProj[16];
    galaxyViewProj(app, aspect, viewProj);
    HolePush push{};
    fillHolePush(app, viewProj, push);
    push.proj[2] = 1.0f;
    push.proj[3] = sgrRadiusLy(app);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.holePipeline);
    vkCmdPushConstants(cmd, app.holeLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(HolePush), &push);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &app.sgrBuffer, &offset);
    vkCmdDraw(cmd, 6, 1, 0, 0);
}

void drawTourHole(AppContext& app, VkCommandBuffer cmd, float aspect) {
    float pos[3], radius;
    if (!tourHole(app, pos, radius)) {
        drawFreeHoles(app, cmd, aspect);
        return;
    }
    Particle p{};
    const float n[3] = {0.28f, 0.88f, 0.38f};
    for (int a = 0; a < 3; a++) {
        p.pos[a] = pos[a];
        p.vel[a] = n[a];
    }
    p.pos[3] = 3.7e10f;
    void* data;
    vkMapMemory(app.device, app.tourHoleMemory, 0, sizeof(Particle), 0, &data);
    std::memcpy(data, &p, sizeof(Particle));
    vkUnmapMemory(app.device, app.tourHoleMemory);
    float viewProj[16];
    universeViewProj(app, aspect, viewProj);
    HolePush push{};
    fillHolePush(app, viewProj, push);
    push.proj[2] = 1.0f;
    push.proj[3] = radius;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.holePipeline);
    vkCmdPushConstants(cmd, app.holeLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(HolePush), &push);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &app.tourHoleBuffer, &offset);
    vkCmdDraw(cmd, 6, 1, 0, 0);
}

void drawBhDiscHole(AppContext& app, VkCommandBuffer cmd, const float viewProj[16], float radius, float timeSec) {
    Particle p{};
    p.pos[3] = radius;
    p.vel[1] = 1.0f;
    void* data;
    vkMapMemory(app.device, app.tourHoleMemory, 0, sizeof(Particle), 0, &data);
    std::memcpy(data, &p, sizeof(Particle));
    vkUnmapMemory(app.device, app.tourHoleMemory);
    HolePush push{};
    fillHolePush(app, viewProj, push);
    push.right[3] = timeSec;
    push.proj[2] = 2.0f;
    push.back[3] = 2.0f;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.holePipeline);
    vkCmdPushConstants(cmd, app.holeLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(HolePush), &push);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &app.tourHoleBuffer, &offset);
    vkCmdDraw(cmd, 6, 1, 0, 0);
}

void destroyBlackHoles(AppContext& app) {
    vkDestroyBuffer(app.device, app.tourHoleBuffer, nullptr);
    vkFreeMemory(app.device, app.tourHoleMemory, nullptr);
    vkDestroyBuffer(app.device, app.sgrBuffer, nullptr);
    vkFreeMemory(app.device, app.sgrMemory, nullptr);
    vkDestroyPipeline(app.device, app.holePipeline, nullptr);
    vkDestroyPipelineLayout(app.device, app.holeLayout, nullptr);
}

void emitBlackHoleText(AppContext& app, OverlayVertex* out, uint32_t& used) {
    uint32_t count = blackHoleCount(app);
    if (count == 0 || !app.readValid[app.currentFrame]) return;
    const Particle* p = static_cast<const Particle*>(app.readMapped[app.currentFrame]);
    const HoleBase& base = holeBase(app);
    float px = 2.0f / (float)app.swapchainExtent.width;
    float py = 2.0f / (float)app.swapchainExtent.height;
    float row = 12.0f;
    float mass[2] = {p[0].pos[3], count > 1 ? p[1].pos[3] : 0.0f};
    bool merged = count > 1 && mass[1] <= 0.0f;
    float floorMass = merged ? 2.0f * base.centreMass : base.centreMass;
    for (uint32_t c = 0; c < count; c++) {
        if (mass[c] <= 0.0f) continue;
        if (mass[c] < 0.5f * base.centreMass) return;
        double swallowed = std::round((mass[c] - floorMass) / base.starMass);
        if (swallowed < 0.0) swallowed = 0.0;
        char text[96];
        std::snprintf(text, sizeof(text), "BLACK HOLE MASS %.0f  SWALLOWED %.0f STARS", (double)mass[c], swallowed);
        emitText(out, used, text, -1.0f + 12.0f * px, -1.0f + row * py, px, py, 2.0f, 1.0f, 0.75f, 0.4f);
        row += 22.0f;
    }
}

void drawHoleCustom(AppContext& app, VkCommandBuffer cmd, VkBuffer buf, uint32_t count, const float viewProj[16], const Camera& cam, float radius, float galaxyMode) {
    HolePush push{};
    fillHolePush(app, viewProj, push, &cam);
    push.proj[2] = galaxyMode;
    push.proj[3] = radius;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.holePipeline);
    vkCmdPushConstants(cmd, app.holeLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(HolePush), &push);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &buf, &offset);
    vkCmdDraw(cmd, 6, count, 0, 0);
}
