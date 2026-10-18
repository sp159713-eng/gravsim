#include "vk_app.h"
#include "gpu.h"
#include "universe.h"
#include "redshift.h"
#include <cstddef>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

std::vector<char> readSpvFile(const std::string& path);
VkShaderModule createSM(VkDevice dev, const std::vector<char>& code);
uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags properties);

namespace {

struct UniPush {
    float viewProj[16];
    float params[4];
};

struct CmbPush {
    float eye[4], fwd[4], right[4], up[4], params[4];
};

struct UniState {
    VkPipelineLayout ptLayout = VK_NULL_HANDLE, cmbLayout = VK_NULL_HANDLE;
    VkPipeline ptPipe = VK_NULL_HANDLE, cmbPipe = VK_NULL_HANDLE;
    VkBuffer buf = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    VkBuffer rzBuf = VK_NULL_HANDLE;
    VkDeviceMemory rzMem = VK_NULL_HANDLE;
    uint32_t count = 0, mwCount = 0;
    std::vector<UniLabel> labels;
    std::vector<UniGalaxy> gals;
    VkBuffer sbuf = VK_NULL_HANDLE;
    VkDeviceMemory smem = VK_NULL_HANDLE;
    UniPoint* smap = nullptr;
    uint32_t scount = 0;
    int hiddenPt = -1;
    float hiddenVals[2] = {0.0f, 0.0f};
    VkBuffer dmBuf = VK_NULL_HANDLE;
    VkDeviceMemory dmMem = VK_NULL_HANDLE;
    uint32_t dmCount = 0;
    VkBuffer hlBuf = VK_NULL_HANDLE;
    VkDeviceMemory hlMem = VK_NULL_HANDLE;
    uint32_t hlCount = 0;
};

constexpr uint32_t STANDIN_MAX = 8192;
constexpr float PICK_PX = 8.0f;

UniState g;

float smooth01(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

void eyeMly(const AppContext& app, float out[3]) {
    float e[3];
    galaxyEye(app, e);
    for (int a = 0; a < 3; a++) out[a] = (float)(app.gal.originMly[a] + (double)e[a] * 1e-6);
}

float centreDistLy(const AppContext& app) {
    float e[3];
    eyeMly(app, e);
    double d[3] = {e[0] - app.gal.centreMly[0], e[1] - app.gal.centreMly[1], e[2] - app.gal.centreMly[2]};
    return (float)(std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]) * 1e6 / app.gal.scale);
}

void camBasis(const Camera& c, float f[3], float r[3], float u[3]) {
    float cp = std::cos(c.pitch), sp = std::sin(c.pitch), sy = std::sin(c.yaw), cy = std::cos(c.yaw);
    f[0] = -cp * sy; f[1] = -sp; f[2] = -cp * cy;
    float rl = std::sqrt(f[2] * f[2] + f[0] * f[0]);
    r[0] = rl > 1e-6f ? -f[2] / rl : 0.0f; r[1] = 0.0f; r[2] = rl > 1e-6f ? f[0] / rl : 0.0f;
    u[0] = r[1] * f[2] - r[2] * f[1];
    u[1] = r[2] * f[0] - r[0] * f[2];
    u[2] = r[0] * f[1] - r[1] * f[0];
}

void viewProjMly(const AppContext& app, float aspect, float out[16]) {
    float e[3];
    eyeMly(app, e);
    cameraViewProjEx(app.camera, aspect, e, 1e-4f, 4e5f, out);
}

VkPipeline makePipeline(AppContext& app, VkPipelineLayout layout, const char* vs, const char* fs, bool points) {
    VkShaderModule vertSM = createSM(app.device, readSpvFile(shaderPath(vs)));
    VkShaderModule fragSM = createSM(app.device, readSpvFile(shaderPath(fs)));
    VkPipelineShaderStageCreateInfo stages[2]{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertSM, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragSM, "main", nullptr}
    };
    VkVertexInputBindingDescription bind{0, sizeof(UniPoint), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[3]{
        {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16},
        {2, 0, VK_FORMAT_R32_SFLOAT, 32}
    };
    VkPipelineVertexInputStateCreateInfo vertInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    if (points) { vertInput.vertexBindingDescriptionCount = 1; vertInput.pVertexBindingDescriptions = &bind; vertInput.vertexAttributeDescriptionCount = 3; vertInput.pVertexAttributeDescriptions = attrs; }
    VkPipelineInputAssemblyStateCreateInfo inputAss{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, points ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};
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
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, stages, &vertInput, &inputAss, nullptr, &vpState, &raster, &multi, nullptr, &blend, &dyn, layout, app.renderPass, 0, VK_NULL_HANDLE, -1};
    VkPipeline pipe;
    if (vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipe) != VK_SUCCESS)
        throw std::runtime_error("failed universe pipeline!");
    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
    return pipe;
}

}

VkPipeline darkPipeline(AppContext& app, VkPipelineLayout layout) {
    return makePipeline(app, layout, "cmb.vert", "dark.frag", false);
}

float universeFade(const AppContext& app) {
    if (app.scene != Scene::SOLAR || !app.showUniverse) return 0.0f;
    float d = centreDistLy(app);
    return smooth01(std::log10(std::max(d, 1.0f) / 90000.0f) / 0.46f);
}

float galaxyLayerFade(const AppContext& app) {
    if (!app.showUniverse) return 1.0f;
    return (1.0f - smooth01(std::log10(std::max(galaxyDistanceLy(app) / app.gal.scale, 1.0f) / 250000.0f) / 0.9f)) * app.gal.fadeIn;
}

void createUniverse(AppContext& app) {
    UniverseData data = makeUniverse();
    g.count = (uint32_t)data.pts.size();
    g.mwCount = data.mwCount;
    g.labels = universeLabels();
    g.gals = std::move(data.gals);
    VkDeviceSize size = data.pts.size() * sizeof(UniPoint);
    VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    binfo.size = size;
    binfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    binfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(app.device, &binfo, nullptr, &g.buf) != VK_SUCCESS)
        throw std::runtime_error("failed universe buffer!");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(app.device, g.buf, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(app.device, &alloc, nullptr, &g.mem) != VK_SUCCESS)
        throw std::runtime_error("failed universe memory!");
    vkBindBufferMemory(app.device, g.buf, g.mem, 0);
    void* ptr;
    vkMapMemory(app.device, g.mem, 0, size, 0, &ptr);
    std::memcpy(ptr, data.pts.data(), (size_t)size);
    vkUnmapMemory(app.device, g.mem);

    redshiftCreateBuffer(app, redshiftPoints(data.pts, data.mwCount), g.rzBuf, g.rzMem);

    std::vector<UniPoint> dm;
    for (size_t i = data.mwCount; i < data.pts.size(); i += 4) {
        UniPoint q = data.pts[i];
        q.radius *= 4.0f;
        q.color[0] = 0.5f; q.color[1] = 0.2f; q.color[2] = 1.0f;
        q.bright *= 0.7f;
        dm.push_back(q);
    }
    g.dmCount = (uint32_t)dm.size();
    VkBufferCreateInfo dinfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    dinfo.size = dm.size() * sizeof(UniPoint);
    dinfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    dinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(app.device, &dinfo, nullptr, &g.dmBuf) != VK_SUCCESS)
        throw std::runtime_error("failed dark web buffer!");
    vkGetBufferMemoryRequirements(app.device, g.dmBuf, &req);
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(app.device, &alloc, nullptr, &g.dmMem) != VK_SUCCESS)
        throw std::runtime_error("failed dark web memory!");
    vkBindBufferMemory(app.device, g.dmBuf, g.dmMem, 0);
    vkMapMemory(app.device, g.dmMem, 0, dinfo.size, 0, &ptr);
    std::memcpy(ptr, dm.data(), (size_t)dinfo.size);
    vkUnmapMemory(app.device, g.dmMem);

    std::vector<UniPoint> hl(data.pts.begin() + data.hercFirst, data.pts.begin() + data.hercFirst + data.hercCount);
    for (UniPoint& q : hl) {
        q.radius = 70.0f;
        q.color[0] = 0.45f; q.color[1] = 0.8f; q.color[2] = 1.0f;
        q.bright *= 6.0f;
    }
    g.hlCount = (uint32_t)hl.size();
    VkBufferCreateInfo hinfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    hinfo.size = hl.size() * sizeof(UniPoint);
    hinfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    hinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(app.device, &hinfo, nullptr, &g.hlBuf) != VK_SUCCESS)
        throw std::runtime_error("failed highlight buffer!");
    vkGetBufferMemoryRequirements(app.device, g.hlBuf, &req);
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(app.device, &alloc, nullptr, &g.hlMem) != VK_SUCCESS)
        throw std::runtime_error("failed highlight memory!");
    vkBindBufferMemory(app.device, g.hlBuf, g.hlMem, 0);
    vkMapMemory(app.device, g.hlMem, 0, hinfo.size, 0, &ptr);
    std::memcpy(ptr, hl.data(), (size_t)hinfo.size);
    vkUnmapMemory(app.device, g.hlMem);

    VkPushConstantRange pushV{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UniPush)};
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &pushV};
    if (vkCreatePipelineLayout(app.device, &li, nullptr, &g.ptLayout) != VK_SUCCESS)
        throw std::runtime_error("failed universe layout!");
    VkPushConstantRange pushF{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(CmbPush)};
    VkPipelineLayoutCreateInfo lc{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &pushF};
    if (vkCreatePipelineLayout(app.device, &lc, nullptr, &g.cmbLayout) != VK_SUCCESS)
        throw std::runtime_error("failed cmb layout!");
    VkBufferCreateInfo sinfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    sinfo.size = STANDIN_MAX * sizeof(UniPoint);
    sinfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    sinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(app.device, &sinfo, nullptr, &g.sbuf) != VK_SUCCESS)
        throw std::runtime_error("failed standin buffer!");
    vkGetBufferMemoryRequirements(app.device, g.sbuf, &req);
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(app.device, &alloc, nullptr, &g.smem) != VK_SUCCESS)
        throw std::runtime_error("failed standin memory!");
    vkBindBufferMemory(app.device, g.sbuf, g.smem, 0);
    void* sp;
    vkMapMemory(app.device, g.smem, 0, sinfo.size, 0, &sp);
    g.smap = static_cast<UniPoint*>(sp);
    g.ptPipe = makePipeline(app, g.ptLayout, "universe.vert", "universe.frag", true);
    g.cmbPipe = makePipeline(app, g.cmbLayout, "cmb.vert", "cmb.frag", false);
}

void drawUniverse(AppContext& app, VkCommandBuffer cmd, float aspect, float viewH) {
    float fade = universeFade(app);
    if (fade <= 0.002f) return;
    float e[3], f[3], r[3], u[3];
    eyeMly(app, e);
    camBasis(app.camera, f, r, u);
    const float tanH = std::tan(30.0f * 3.14159265f / 180.0f);
    CmbPush cp{};
    for (int a = 0; a < 3; a++) { cp.eye[a] = e[a]; cp.fwd[a] = f[a]; cp.right[a] = r[a]; cp.up[a] = u[a]; }
    cp.params[0] = 0.045f * fade;
    cp.params[1] = aspect * tanH;
    cp.params[2] = tanH;
    cp.params[3] = CMB_RADIUS_MLY;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g.cmbPipe);
    vkCmdPushConstants(cmd, g.cmbLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(CmbPush), &cp);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    UniPush push;
    viewProjMly(app, aspect, push.viewProj);
    push.params[1] = viewH * 0.5f / tanH;
    push.params[2] = 1.5f;
    push.params[3] = 20.0f;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g.ptPipe);
    VkDeviceSize offset = 0;
    VkBuffer mainBuf = g_redshift ? g.rzBuf : g.buf;
    vkCmdBindVertexBuffers(cmd, 0, 1, &mainBuf, &offset);
    float el = centreDistLy(app) * 1e-6f;
    float far01 = smooth01((std::log10(std::max(el, 1.0f)) - 2.7f) / 2.0f);
    float gain = 0.55f * (1.0f - 0.75f * far01);
    push.params[0] = gain * fade;
    vkCmdPushConstants(cmd, g.ptLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UniPush), &push);
    vkCmdDraw(cmd, g.count - g.mwCount, 1, g.mwCount, 0);
    float mwA = app.gal.active ? 1.0f : 1.0f - galaxyLayerFade(app);
    if (mwA > 0.01f) {
        push.params[0] = gain * fade * mwA;
        vkCmdPushConstants(cmd, g.ptLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UniPush), &push);
        vkCmdDraw(cmd, g.mwCount, 1, 0, 0);
    }
    if (app.showDark && !app.gal.active) {
        UniPush dp = push;
        dp.params[0] = 1.6f * gain * fade;
        dp.params[3] = 24.0f;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g.ptPipe);
        vkCmdPushConstants(cmd, g.ptLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UniPush), &dp);
        vkCmdBindVertexBuffers(cmd, 0, 1, &g.dmBuf, &offset);
        vkCmdDraw(cmd, g.dmCount, 1, 0, 0);
        const float zero[3] = {0.0f, 0.0f, 0.0f};
        darkHalo(app, cmd, e, zero, 0.03f, 0.5f, 0.4f * fade, aspect);
        for (const UniGalaxy& gl : g.gals) {
            if (gl.named == 1) darkHalo(app, cmd, e, gl.pos, 0.05f, 0.85f, 0.4f * fade, aspect);
            if (gl.named == 2) darkHalo(app, cmd, e, gl.pos, 0.018f, 0.3f, 0.4f * fade, aspect);
        }
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g.ptPipe);
        vkCmdBindVertexBuffers(cmd, 0, 1, &mainBuf, &offset);
    }
    float glow = tourWallGlow(app);
    if (glow > 0.01f && g.hlCount > 0) {
        UniPush hp = push;
        hp.params[0] = 0.6f * fade * glow;
        hp.params[2] = 2.5f;
        hp.params[3] = 6.0f;
        vkCmdPushConstants(cmd, g.ptLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UniPush), &hp);
        vkCmdBindVertexBuffers(cmd, 0, 1, &g.hlBuf, &offset);
        vkCmdDraw(cmd, g.hlCount, 1, 0, 0);
        vkCmdBindVertexBuffers(cmd, 0, 1, &mainBuf, &offset);
    }
    float saA = app.gal.active ? 1.0f - galaxyLayerFade(app) : 0.0f;
    if (saA > 0.01f && g.scount > 0) {
        push.params[0] = gain * fade * saA;
        vkCmdPushConstants(cmd, g.ptLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(UniPush), &push);
        vkCmdBindVertexBuffers(cmd, 0, 1, &g.sbuf, &offset);
        vkCmdDraw(cmd, g.scount, 1, 0, 0);
    }
}

void destroyUniverse(AppContext& app) {
    vkDestroyPipeline(app.device, g.ptPipe, nullptr);
    vkDestroyPipeline(app.device, g.cmbPipe, nullptr);
    vkDestroyPipelineLayout(app.device, g.ptLayout, nullptr);
    vkDestroyPipelineLayout(app.device, g.cmbLayout, nullptr);
    vkDestroyBuffer(app.device, g.buf, nullptr);
    vkFreeMemory(app.device, g.mem, nullptr);
    vkDestroyBuffer(app.device, g.rzBuf, nullptr);
    vkFreeMemory(app.device, g.rzMem, nullptr);
    vkDestroyBuffer(app.device, g.dmBuf, nullptr);
    vkFreeMemory(app.device, g.dmMem, nullptr);
    vkDestroyBuffer(app.device, g.hlBuf, nullptr);
    vkFreeMemory(app.device, g.hlMem, nullptr);
    vkDestroyBuffer(app.device, g.sbuf, nullptr);
    vkFreeMemory(app.device, g.smem, nullptr);
}

void universeStandIn(const std::vector<UniPoint>& pts) {
    g.scount = (uint32_t)std::min<size_t>(pts.size(), STANDIN_MAX);
    if (g.scount) std::memcpy(g.smap, pts.data(), g.scount * sizeof(UniPoint));
}

void universeHide(AppContext& app, int point) {
    VkDeviceMemory mems[2] = {g.mem, g.rzMem};
    if (g.hiddenPt >= 0) {
        for (int m = 0; m < 2; m++) {
            void* p;
            vkMapMemory(app.device, mems[m], (VkDeviceSize)g.hiddenPt * sizeof(UniPoint) + offsetof(UniPoint, bright), sizeof(float), 0, &p);
            std::memcpy(p, &g.hiddenVals[m], sizeof(float));
            vkUnmapMemory(app.device, mems[m]);
        }
        g.hiddenPt = -1;
    }
    if (point < 0 || (uint32_t)point >= g.count) return;
    VkDeviceSize off = (VkDeviceSize)point * sizeof(UniPoint) + offsetof(UniPoint, bright);
    for (int m = 0; m < 2; m++) {
        void* p;
        vkMapMemory(app.device, mems[m], off, sizeof(float), 0, &p);
        std::memcpy(&g.hiddenVals[m], p, sizeof(float));
        float zero = 0.0f;
        std::memcpy(p, &zero, sizeof(float));
        vkUnmapMemory(app.device, mems[m]);
    }
    g.hiddenPt = point;
}

bool universeProject(const AppContext& app, const float posMly[3], float& nx, float& ny) {
    float vp[16];
    viewProjMly(app, (float)app.swapchainExtent.width / (float)app.swapchainExtent.height, vp);
    float clip[4];
    for (int row = 0; row < 4; row++)
        clip[row] = vp[row] * posMly[0] + vp[row + 4] * posMly[1] + vp[row + 8] * posMly[2] + vp[row + 12];
    if (clip[3] <= 1e-9f) return false;
    nx = clip[0] / clip[3];
    ny = clip[1] / clip[3];
    return true;
}

int universePick(const AppContext& app, double fx, double fy, float pos[3], int& named) {
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float vp[16];
    viewProjMly(app, W / H, vp);
    float best = PICK_PX * PICK_PX;
    int idx = -1;
    for (size_t i = 0; i < g.gals.size(); i++) {
        const UniGalaxy& q = g.gals[i];
        if ((int)q.point == g.hiddenPt) continue;
        const float* p = q.pos;
        float w = vp[3] * p[0] + vp[7] * p[1] + vp[11] * p[2] + vp[15];
        if (w < 1e-9f) continue;
        float sx = ((vp[0] * p[0] + vp[4] * p[1] + vp[8] * p[2] + vp[12]) / w + 1.0f) * 0.5f * W - (float)fx;
        if (sx > PICK_PX || sx < -PICK_PX) continue;
        float sy = ((vp[1] * p[0] + vp[5] * p[1] + vp[9] * p[2] + vp[13]) / w + 1.0f) * 0.5f * H - (float)fy;
        float d2 = sx * sx + sy * sy;
        if (d2 >= best) continue;
        if (q.fadeD < 1e8f && std::pow(w / q.fadeD, 3.0f) > 2.5f) continue;
        best = d2;
        idx = (int)i;
    }
    if (idx >= 0) {
        for (int a = 0; a < 3; a++) pos[a] = g.gals[idx].pos[a];
        named = g.gals[idx].named;
    }
    return idx;
}

int universeGalaxyPoint(int idx) {
    return idx >= 0 && (size_t)idx < g.gals.size() ? (int)g.gals[idx].point : -1;
}

void universeScreenLabels(AppContext& app, std::vector<ScreenLabel>& out) {
    float fade = universeFade(app);
    if (fade < 0.05f) return;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float vp[16], e[3], f[3], r[3], u[3];
    viewProjMly(app, W / H, vp);
    eyeMly(app, e);
    camBasis(app.camera, f, r, u);
    float el = std::sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
    for (const UniLabel& l : g.labels) {
        float p[3] = {l.pos[0], l.pos[1], l.pos[2]};
        if (l.dynamic) {
            float base[3];
            bool inside = el < CMB_RADIUS_MLY;
            float shell = l.pos[0] > 0.0f ? l.pos[0] : CMB_RADIUS_MLY, lift = l.pos[0] > 0.0f ? l.pos[1] : 0.62f;
            for (int a = 0; a < 3; a++) base[a] = inside ? f[a] * 0.8f + u[a] * 0.5f : e[a] / el * 0.78f + u[a] * lift;
            float bl = std::sqrt(base[0] * base[0] + base[1] * base[1] + base[2] * base[2]);
            for (int a = 0; a < 3; a++) p[a] = base[a] / bl * shell;
        }
        float dx = p[0] - e[0], dy = p[1] - e[1], dz = p[2] - e[2];
        float w = std::sqrt(dx * dx + dy * dy + dz * dz);
        float fi = std::clamp(std::log(w / l.lo) / std::log(2.5f), 0.0f, 1.0f);
        float fo = std::clamp(std::log(l.hi / w) / std::log(2.5f), 0.0f, 1.0f);
        float home = std::sqrt(l.pos[0] * l.pos[0] + l.pos[1] * l.pos[1] + l.pos[2] * l.pos[2]);
        float fe = std::clamp(std::log(el / std::max(0.15f * home, 0.01f)) / std::log(2.5f), 0.0f, 1.0f);
        float fl = std::min(std::min(fi, fo), fe) * fade;
        bool mw = l.text[0] == 'M' && l.text[1] == 'I';
        if (mw && app.gal.active) fl = std::min(fi, fo);
        if (app.gal.active && ((app.gal.spec.named == 1 && std::strcmp(l.text, "ANDROMEDA") == 0) || (app.gal.spec.named == 2 && std::strcmp(l.text, "TRIANGULUM") == 0))) continue;
        const char* shown = mw && app.gal.active ? galMwLabel(app) : l.text;
        if (mw && el > 1.0f && fl > 0.05f) {
            float rc[4];
            for (int row = 0; row < 4; row++)
                rc[row] = vp[row] * p[0] + vp[row + 4] * p[1] + vp[row + 8] * p[2] + vp[row + 12];
            if (rc[3] > 1e-9f && std::fabs(rc[0] / rc[3]) < 1.0f && std::fabs(rc[1] / rc[3]) < 1.0f) {
                ScreenLabel ring{rc[0] / rc[3], rc[1] / rc[3], nullptr, {fl, 0.78f * fl, 0.3f * fl}};
                ring.ring = 7.0f;
                out.push_back(ring);
            }
        }
        if (fl < 0.12f) continue;
        float clip[4];
        for (int row = 0; row < 4; row++)
            clip[row] = vp[row] * p[0] + vp[row + 4] * p[1] + vp[row + 8] * p[2] + vp[row + 12];
        if (clip[3] <= 1e-9f) continue;
        float nx = clip[0] / clip[3], ny = clip[1] / clip[3];
        if (std::fabs(nx) > 1.0f || std::fabs(ny) > 1.0f) continue;
        float sx = (nx + 1.0f) * 0.5f * W, sy = (ny + 1.0f) * 0.5f * H;
        size_t n1 = std::strlen(shown), n2 = l.sub ? std::strlen(l.sub) : 0;
        if (sx + 18.0f + 12.0f * (float)std::max(n1, n2) > W) continue;
        ScreenLabel main{nx, ny, shown, {l.color[0] * fl, l.color[1] * fl, l.color[2] * fl}};
        main.prio = mw ? 0.0f : 4.0f - 0.4f * std::log10(l.hi);
        out.push_back(main);
        if (l.sub) {
            ScreenLabel sub{nx, ny, l.sub, {0.55f * fl, 0.6f * fl, 0.7f * fl}};
            sub.follow = true;
            sub.oy = 7.0f;
            out.push_back(sub);
        }
    }
    if (app.gal.active) {
        float a = 1.0f - galaxyLayerFade(app);
        float p[3] = {(float)app.gal.centreMly[0], (float)app.gal.centreMly[1], (float)app.gal.centreMly[2]};
        float nx, ny;
        if (a > 0.12f && universeProject(app, p, nx, ny) && std::fabs(nx) < 1.0f && std::fabs(ny) < 1.0f) {
            ScreenLabel ring{nx, ny, nullptr, {a, 0.78f * a, 0.3f * a}};
            ring.ring = 7.0f;
            out.push_back(ring);
            ScreenLabel main{nx, ny, galTitle(app), {0.7f * a, 0.9f * a, a}};
            main.prio = 0.2f;
            out.push_back(main);
        }
    }
}

void universeViewProj(const AppContext& app, float aspect, float out[16]) {
    viewProjMly(app, aspect, out);
}
