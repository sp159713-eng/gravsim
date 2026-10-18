#include "vk_app.h"
#include "expansion.h"
#include "bhdisc.h"
#include "redshift.h"
#include "gpu.h"
#include "solarx.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

std::vector<char> readSpvFile(const std::string& path);
VkShaderModule createSM(VkDevice dev, const std::vector<char>& code);
uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags properties);

namespace {

const uint32_t COMET = 14;
const size_t TRAIL_MAX = 900;

std::string withCommas(long long v) {
    std::string s = std::to_string(v);
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ",");
    return s;
}

bool project(const float m[16], const float p[3], float& x, float& y) {
    float clip[4];
    for (int row = 0; row < 4; row++)
        clip[row] = m[row] * p[0] + m[row + 4] * p[1] + m[row + 8] * p[2] + m[row + 12];
    if (clip[3] <= 1e-9f) return false;
    x = clip[0] / clip[3];
    y = clip[1] / clip[3];
    return true;
}

void makeBuffer(AppContext& app, VkDeviceSize size, VkBufferUsageFlags usage,
                VkBuffer& buffer, VkDeviceMemory& memory, void*& mapped) {
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(app.device, &info, nullptr, &buffer) != VK_SUCCESS)
        throw std::runtime_error("failed overlay buffer!");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(app.device, buffer, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(app.device, &alloc, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("failed overlay memory!");
    vkBindBufferMemory(app.device, buffer, memory, 0);
    vkMapMemory(app.device, memory, 0, size, 0, &mapped);
    std::memset(mapped, 0, size);
}

VkPipeline makePipeline(AppContext& app, VkPrimitiveTopology topology, bool dim = false) {
    VkShaderModule vertSM = createSM(app.device, readSpvFile(shaderPath("overlay.vert")));
    VkShaderModule fragSM = createSM(app.device, readSpvFile(shaderPath("grid.frag")));
    VkPipelineShaderStageCreateInfo stages[2]{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertSM, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragSM, "main", nullptr}
    };
    VkVertexInputBindingDescription bind{0, sizeof(OverlayVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[2]{
        {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 8}
    };
    VkPipelineVertexInputStateCreateInfo vertInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0, 1, &bind, 2, attrs};
    VkPipelineInputAssemblyStateCreateInfo inputAss{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, topology, VK_FALSE};
    VkPipelineViewportStateCreateInfo vpState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0, 0, 0, 1.0f};
    VkPipelineMultisampleStateCreateInfo multi{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0, nullptr, VK_FALSE, VK_FALSE};
    VkPipelineColorBlendAttachmentState attBlend{};
    attBlend.colorWriteMask = 0xF;
    attBlend.blendEnable = VK_TRUE;
    attBlend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    attBlend.dstColorBlendFactor = dim ? VK_BLEND_FACTOR_CONSTANT_COLOR : VK_BLEND_FACTOR_ONE;
    attBlend.colorBlendOp = VK_BLEND_OP_ADD;
    attBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attBlend.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY, 1, &attBlend, {0.15f, 0.15f, 0.15f, 0.15f}};
    VkDynamicState dyns[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, dyns};
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, stages, &vertInput, &inputAss, nullptr, &vpState, &raster, &multi, nullptr, &blend, &dyn, app.overlayLayout, app.renderPass, 0, VK_NULL_HANDLE, -1};
    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("failed overlay pipeline!");
    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
    return pipeline;
}

void addTrailPoint(std::vector<std::array<float, 3>>& trail, const float p[3], size_t fixedCap, float spacing) {
    if (!trail.empty()) {
        const auto& last = trail.back();
        float dx = p[0] - last[0], dy = p[1] - last[1], dz = p[2] - last[2];
        if (dx * dx + dy * dy + dz * dz < spacing * spacing) return;
    }
    float r = std::sqrt(p[0] * p[0] + p[2] * p[2]);
    size_t cap = (size_t)(0.9f * 2.0f * 3.14159265f * r / spacing);
    if (fixedCap) cap = fixedCap;
    if (cap > TRAIL_MAX) cap = TRAIL_MAX;
    trail.push_back({p[0], p[1], p[2]});
    while (trail.size() > cap && !trail.empty()) trail.erase(trail.begin());
}

}

void createOverlay(AppContext& app) {
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    if (vkCreatePipelineLayout(app.device, &layoutInfo, nullptr, &app.overlayLayout) != VK_SUCCESS)
        throw std::runtime_error("failed overlay layout!");
    app.overlayLines = makePipeline(app, VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
    app.overlayPoints = makePipeline(app, VK_PRIMITIVE_TOPOLOGY_POINT_LIST);
    app.overlayTris = makePipeline(app, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, true);
    for (int i = 0; i < 2; i++) {
        makeBuffer(app, MAX_VERTS * sizeof(OverlayVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                   app.overlayBuffers[i], app.overlayMemories[i], app.overlayMapped[i]);
        makeBuffer(app, std::max<size_t>(BODIES, app.n) * sizeof(Particle), VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                   app.readBuffers[i], app.readMemories[i], app.readMapped[i]);
    }
}

void destroyOverlay(AppContext& app) {
    for (int i = 0; i < 2; i++) {
        vkDestroyBuffer(app.device, app.overlayBuffers[i], nullptr);
        vkFreeMemory(app.device, app.overlayMemories[i], nullptr);
        vkDestroyBuffer(app.device, app.readBuffers[i], nullptr);
        vkFreeMemory(app.device, app.readMemories[i], nullptr);
    }
    vkDestroyPipeline(app.device, app.overlayLines, nullptr);
    vkDestroyPipeline(app.device, app.overlayPoints, nullptr);
    vkDestroyPipeline(app.device, app.overlayTris, nullptr);
    vkDestroyPipelineLayout(app.device, app.overlayLayout, nullptr);
}

uint32_t readbackCount(const AppContext& app) {
    uint32_t extra = throwReadEnd(app);
    if (app.scene == Scene::SOLAR) return std::max(app.n < BODIES ? app.n : BODIES, extra);
    if (app.scene == Scene::GALAXY && app.showDark) return app.n;
    uint32_t holes = blackHoleCount(app);
    if (app.scene == Scene::ANDROMEDA) return std::min(app.n, std::max(holes, ANDROMEDA_SUN + 1));
    return std::max(holes, extra);
}

void recordReadback(AppContext& app, VkCommandBuffer cmd, VkBuffer latest) {
    uint32_t count = readbackCount(app);
    if (count == 0) return;
    VkBufferCopy region{0, 0, count * sizeof(Particle)};
    vkCmdCopyBuffer(cmd, latest, app.readBuffers[app.currentFrame], 1, &region);
    VkMemoryBarrier toHost{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &toHost, 0, nullptr, 0, nullptr);
    app.readValid[app.currentFrame] = true;
}

namespace {

void bodyAcc(const Particle* cur, uint32_t count, uint32_t i, float G, float soft2, float bodySoft2, float* acc) {
    const Particle& p = cur[i];
    acc[0] = acc[1] = acc[2] = 0.0f;
    for (uint32_t j = 0; j < count; j++) {
        if (i == j) continue;
        float d[3] = {cur[j].pos[0] - p.pos[0], cur[j].pos[1] - p.pos[1], cur[j].pos[2] - p.pos[2]};
        float distSqr = d[0] * d[0] + d[1] * d[1] + d[2] * d[2] + (i < BODY_PAIRS && j < BODY_PAIRS ? bodySoft2 : soft2);
        float invDist = 1.0f / std::sqrt(distSqr);
        float invDist3 = invDist * invDist * invDist;
        float f = G * cur[j].pos[3] * invDist3;
        for (int k = 0; k < 3; k++) acc[k] += f * d[k];
    }
}

void replayStep(Particle* cur, Particle* nxt, uint32_t count, float dt, float prevDt, float G, float soft) {
    const float soft2 = soft * soft;
    const float bodySoft2 = soft2 * BODY_SOFT_RATIO * BODY_SOFT_RATIO;
    for (uint32_t i = 0; i < count; i++) {
        const Particle& p = cur[i];
        Particle& o = nxt[i];
        if (p.vel[3] > 0.5f) {
            uint32_t par = (uint32_t)p.vel[3] - 1u;
            if (par >= count) { o = p; continue; }
            float a = p.vel[1] + p.vel[2] * dt;
            float c[3];
            for (int k = 0; k < 3; k++) c[k] = cur[par].pos[k] + cur[par].vel[k] * dt;
            float off[3] = {std::cos(a) * p.vel[0], 0.0f, std::sin(a) * p.vel[0]};
            if ((uint32_t)p.vel[3] == 7u) {
                float z = off[2];
                off[1] = z * std::sin(0.47f);
                off[2] = z * std::cos(0.47f);
            }
            for (int k = 0; k < 3; k++) o.pos[k] = c[k] + off[k];
            o.pos[3] = p.pos[3];
            o.vel[0] = p.vel[0];
            o.vel[1] = a - 6.2831853f * std::floor(a / 6.2831853f);
            o.vel[2] = p.vel[2];
            o.vel[3] = p.vel[3];
            continue;
        }
        float acc[3];
        bodyAcc(cur, count, i, G, soft2, bodySoft2, acc);
        const float kick = 0.5f * (prevDt + dt);
        for (int k = 0; k < 3; k++) {
            float v = p.vel[k] + acc[k] * kick;
            o.vel[k] = v;
            o.pos[k] = p.pos[k] + v * dt;
        }
        o.pos[3] = p.pos[3];
        o.vel[3] = 0.0f;
    }
}

}

void buildDisplayBodies(AppContext& app) {
    app.displayValid = app.scene == Scene::SOLAR && app.readValid[app.currentFrame];
    if (!app.displayValid) return;
    uint32_t count = app.n < BODIES ? app.n : BODIES;
    Particle bufB[BODIES];
    Particle* cur = app.displayBodies;
    Particle* nxt = bufB;
    std::memcpy(cur, app.readMapped[app.currentFrame], count * sizeof(Particle));
    const int order[2] = {1 - app.currentFrame, app.currentFrame};
    for (int b = 0; b < 2; b++)
        for (uint32_t s = 0; s < app.batchSteps[order[b]]; s++) {
            replayStep(cur, nxt, count, app.batchDt[order[b]], s == 0 ? app.batchPrev[order[b]] : app.batchDt[order[b]], app.simG, app.simSoft);
            std::swap(cur, nxt);
        }
    if (cur != app.displayBodies) std::memcpy(app.displayBodies, cur, count * sizeof(Particle));
    if (!app.leapReset) {
        const float soft2 = app.simSoft * app.simSoft;
        const float bodySoft2 = soft2 * BODY_SOFT_RATIO * BODY_SOFT_RATIO;
        for (uint32_t i = 0; i < count; i++) {
            if (cur[i].vel[3] > 0.5f) continue;
            float acc[3];
            bodyAcc(cur, count, i, app.simG, soft2, bodySoft2, acc);
            for (int k = 0; k < 3; k++) app.displayBodies[i].vel[k] += 0.5f * app.lastStepDt * acc[k];
        }
    }
}

void updateOverlay(AppContext& app) {
    app.overlayLineVerts[app.currentFrame] = 0;
    app.overlayPointVerts[app.currentFrame] = 0;
    app.overlayTriVerts[app.currentFrame] = 0;
    app.overlayUiVerts[app.currentFrame] = 0;
    OverlayVertex* out = static_cast<OverlayVertex*>(app.overlayMapped[app.currentFrame]);
    uint32_t used = 0;
    if (app.scene == Scene::LIFECYCLE) {
        emitLifecycle(app, out, used);
        app.overlayPointVerts[app.currentFrame] = used;
        emitHelpUi(app, out, used);
        return;
    }
    if (app.scene == Scene::SANDBOX) {
        emitSandbox(app, out, used);
        return;
    }
    if (app.scene == Scene::EARTHMOON) {
        emitEarthMoon(app, out, used);
        emitHelpUi(app, out, used);
        return;
    }
    if (app.scene == Scene::TIMEDIL) {
        emitTimeDilation(app, out, used);
        emitHelpUi(app, out, used);
        return;
    }
    if (app.scene == Scene::FUTURESKY) {
        emitFutureSky(app, out, used);
        emitHelpUi(app, out, used);
        return;
    }
    if (app.scene == Scene::NIGHTSKY) {
        emitNightSky(app, out, used);
        emitHelpUi(app, out, used);
        return;
    }
    if (app.scene == Scene::IMPACT) {
        emitImpact(app, out, used);
        emitHelpUi(app, out, used);
        return;
    }
    if (app.scene == Scene::ECLIPSE) {
        emitEclipse(app, out, used);
        emitHelpUi(app, out, used);
        return;
    }
    if (app.scene == Scene::EXPANSION) {
        emitExpansion(app, out, used);
        emitHelpUi(app, out, used);
        return;
    }
    if (app.scene == Scene::BHDISC) {
        emitBhDisc(app, out, used);
        emitHelpUi(app, out, used);
        return;
    }
    if (app.scene == Scene::LIGHTRIDE) {
        emitLightRide(app, out, used);
        emitHelpUi(app, out, used);
        return;
    }
    if (app.scene == Scene::LAGRANGE) {
        emitLagrange(app, out, used);
        emitHelpUi(app, out, used);
        return;
    }
    if (app.scene == Scene::STARSIZES) {
        emitStarSizes(app, out, used);
        emitHelpUi(app, out, used);
        return;
    }
    if (app.scene == Scene::BHSIZES) {
        emitBhSizes(app, out, used);
        emitHelpUi(app, out, used);
        return;
    }
    if (tourMicroCovers(app)) {
        emitTourLines(app, out, used);
        uint32_t lv = used;
        emitTourText(app, out, used);
        app.overlayLineVerts[app.currentFrame] = lv;
        app.overlayPointVerts[app.currentFrame] = used - lv;
        emitHelpUi(app, out, used);
        return;
    }
    if (!app.displayValid) {
        emitDarkLines(app, out, used);
        emitThrowLines(app, out, used);
        uint32_t lv = used;
        emitBlackHoleText(app, out, used);
        gwEmitText(app, out, used);
        andromedaEmit(app, out, used);
        emitDarkText(app, out, used);
        app.overlayLineVerts[app.currentFrame] = lv;
        app.overlayPointVerts[app.currentFrame] = used - lv;
        emitHelpUiThrow(app, out, used);
        return;
    }

    uint32_t count = app.n < BODIES ? app.n : BODIES;
    const Particle* bodies = app.displayBodies;
    const float planetSpacing = app.trueScale ? 0.4f * solarUnitsPerAu(true) / 8.0f : 0.4f;
    for (uint32_t i = 1; i < count; i++)
        if (app.bodyActive[i] && app.bodyParent[i] < 0) addTrailPoint(app.trails[i], bodies[i].pos, i == COMET ? 230 : 0, i == COMET ? 0.4f : planetSpacing);
    if (app.follow >= 0 && (uint32_t)app.follow < count && !app.camera.free) {
        float p[3];
        bodyPosition(app, bodies, (uint32_t)app.follow, p);
        for (int a = 0; a < 3; a++) app.camera.target[a] = p[a];
    }

    float aspect = (float)app.swapchainExtent.width / (float)app.swapchainExtent.height;
    float viewProj[16];
    cameraViewProj(app.camera, aspect, viewProj);
    float eyeNow[3], sunNow[3];
    cameraEye(app.camera, eyeNow);
    sunPosition(app, sunNow);
    float sunDist = std::sqrt((eyeNow[0] - sunNow[0]) * (eyeNow[0] - sunNow[0]) + (eyeNow[1] - sunNow[1]) * (eyeNow[1] - sunNow[1]) + (eyeNow[2] - sunNow[2]) * (eyeNow[2] - sunNow[2]));
    bool bodiesFar = sunDist > BODY_FAR_UNITS || (app.gal.active && !app.visit.active);

    for (uint32_t i = 1; i < count; i++) {
        if (!app.bodyActive[i] || app.bodyParent[i] >= 0) continue;
        const auto& trail = app.trails[i];
        for (size_t k = 1; !bodiesFar && k < trail.size() && used + 2 <= MAX_VERTS; k++) {
            float x0, y0, x1, y1;
            if (!project(viewProj, trail[k - 1].data(), x0, y0) || !project(viewProj, trail[k].data(), x1, y1))
                continue;
            float fade = 0.5f * (float)k / (float)trail.size();
            const float* c = app.bodyColors[i];
            out[used++] = {x0, y0, c[0] * fade, c[1] * fade, c[2] * fade};
            out[used++] = {x1, y1, c[0] * fade, c[1] * fade, c[2] * fade};
        }
    }
    if (!bodiesFar) solarxLines(app, out, used, viewProj);
    if (!bodiesFar && !(app.gal.active && !app.visit.active)) emitOrbitLines(app, out, used, viewProj);
    float px = 2.0f / (float)app.swapchainExtent.width;
    float py = 2.0f / (float)app.swapchainExtent.height;
    const float scale = 2.0f;
    double moment[3] = {0.0, 0.0, 0.0}, total = 0.0;
    for (uint32_t i = 0; i < count; i++) {
        if (bodies[i].vel[3] > 0.5f) continue;
        total += bodies[i].pos[3];
        for (int a = 0; a < 3; a++) moment[a] += (double)bodies[i].pos[3] * bodies[i].pos[a];
    }
    float bary[3] = {(float)(moment[0] / total), (float)(moment[1] / total), (float)(moment[2] / total)};
    float bx = 0.0f, by = 0.0f;
    bool baryVisible = total > 0.0 && !bodiesFar && project(viewProj, bary, bx, by);
    if (baryVisible && used + 4 <= MAX_VERTS) {
        out[used++] = {bx - 7.0f * px, by, 1.0f, 0.35f, 0.35f};
        out[used++] = {bx + 7.0f * px, by, 1.0f, 0.35f, 0.35f};
        out[used++] = {bx, by - 7.0f * py, 1.0f, 0.35f, 0.35f};
        out[used++] = {bx, by + 7.0f * py, 1.0f, 0.35f, 0.35f};
    }
    emitVisitLines(app, out, used, viewProj, bodiesFar);
    std::vector<ScreenLabel> labels;
    if (baryVisible) {
        ScreenLabel com{bx, by, "CENTRE OF MASS", {1.0f, 0.45f, 0.45f}};
        com.prio = 3.0f;
        com.oy = 6.0f;
        labels.push_back(com);
    }
    float sunX = 0.0f, sunY = 0.0f;
    bool sunProjected = project(viewProj, sunNow, sunX, sunY);
    for (uint32_t i = 0; i < count && !bodiesFar; i++) {
        if (!app.bodyActive[i] || (app.bodyParent[i] >= 0 && app.bodyParent[i] != app.follow && !(app.tour.mode != 0 && i == 9))) continue;
        float x, y;
        float bp[3];
        bodyPosition(app, bodies, i, bp);
        if (!project(viewProj, bp, x, y)) continue;
        if (i > 0 && sunProjected && std::hypot((x - sunX) / px, (y - sunY) / py) < 16.0f) continue;
        ScreenLabel name{x, y, app.bodyNames[i].c_str(), {0.85f, 0.85f, 0.9f}};
        name.prio = 0.5f + 0.01f * (float)i;
        labels.push_back(name);
        if (!app.bodyNotes[i].empty()) {
            ScreenLabel note{x, y, app.bodyNotes[i].c_str(), {0.35f, 1.0f, 0.5f}};
            note.follow = true;
            note.oy = 3.0f;
            labels.push_back(note);
        }
    }
    if (!bodiesFar) solarxLabels(app, labels, viewProj);
    galaxyScreenLabels(app, labels);
    universeScreenLabels(app, labels);
    declutterLabels(labels, (float)app.swapchainExtent.width, (float)app.swapchainExtent.height);
    emitLabelRings(labels, out, used, (float)app.swapchainExtent.width, (float)app.swapchainExtent.height);
    emitTourLines(app, out, used);
    emitThrowLines(app, out, used);
    emitRedshiftLines(app, out, used);
    uint32_t lineVerts = used;
    if (!closeupActive(app)) {
        const double kmPerUnit = 149.6e6 / solarUnitsPerAu(app.trueScale);
        double refDist = app.camera.free ? app.refDist : app.camera.dist;
        double unitsPerPx = 2.0 * refDist * std::tan(30.0 * 3.14159265 / 180.0) / app.swapchainExtent.height;
        double kmPerPx = unitsPerPx * kmPerUnit;
        double mag = std::pow(10.0, std::floor(std::log10(kmPerPx)) - 2.0);
        long long rounded = (long long)(std::round(kmPerPx / mag) * mag);
        double lyPerPx = kmPerPx / 9.4607e12;
        std::string text = "1 PX = " + withCommas(rounded) + " KM";
        if (lyPerPx >= 0.1) {
            const char* unit = lyPerPx >= 1e9 ? "GLY" : lyPerPx >= 1e6 ? "MLY" : "LY";
            double div = lyPerPx >= 1e9 ? 1e9 : lyPerPx >= 1e6 ? 1e6 : 1.0;
            char buf[48];
            std::snprintf(buf, sizeof(buf), lyPerPx / div >= 100.0 ? "%.0f" : lyPerPx / div >= 10.0 ? "%.1f" : "%.2f", lyPerPx / div);
            text = std::string("1 PX = ") + buf + " " + unit;
        }
        emitText(out, used, text.c_str(), -1.0f + 12.0f * px, -1.0f + 12.0f * py, px, py, scale, 0.9f, 0.9f, 0.95f);
        emitText(out, used, app.trueScale ? "TRUE SCALE" : "ENLARGED", -1.0f + 12.0f * px, -1.0f + 34.0f * py, px, py, scale, 0.6f, 0.8f, 1.0f);
    }
    emitLabelTexts(labels, out, used, (float)app.swapchainExtent.width, (float)app.swapchainExtent.height);
    emitRedshiftText(app, out, used);
    emitVisitText(app, out, used, px, py, scale);
    emitTourText(app, out, used);
    emitTimeText(app, out, used, px, py);
    solarxText(app, out, used, px, py);
    emitDarkText(app, out, used);
    app.overlayLineVerts[app.currentFrame] = lineVerts;
    app.overlayPointVerts[app.currentFrame] = used - lineVerts;
    emitHelpUiThrow(app, out, used);
}

void drawOverlay(AppContext& app, VkCommandBuffer cmd) {
    uint32_t lines = app.overlayLineVerts[app.currentFrame];
    uint32_t points = app.overlayPointVerts[app.currentFrame];
    uint32_t tris = app.overlayTriVerts[app.currentFrame];
    uint32_t ui = app.overlayUiVerts[app.currentFrame];
    if (lines + points + tris + ui == 0) return;
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &app.overlayBuffers[app.currentFrame], &offset);
    if (lines) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.overlayLines);
        vkCmdDraw(cmd, lines, 1, 0, 0);
    }
    if (points && app.showLabels) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.overlayPoints);
        vkCmdDraw(cmd, points, 1, lines, 0);
    }
    if (tris) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.overlayTris);
        vkCmdDraw(cmd, tris, 1, lines + points, 0);
    }
    if (ui) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.overlayPoints);
        vkCmdDraw(cmd, ui, 1, lines + points + tris, 0);
    }
}

bool currentBodies(AppContext& app, const Particle*& bodies, uint32_t& count) {
    if (!app.displayValid) return false;
    count = app.n < BODIES ? app.n : BODIES;
    bodies = app.displayBodies;
    return true;
}

void bodyPosition(const AppContext& app, const Particle* bodies, uint32_t i, float out[3]) {
    (void)app;
    for (int a = 0; a < 3; a++) out[a] = bodies[i].pos[a];
}

int pickBody(AppContext& app, double mouseX, double mouseY) {
    const Particle* bodies;
    uint32_t count;
    if (!currentBodies(app, bodies, count) || (app.gal.active && !app.visit.active)) return -1;
    float aspect = (float)app.swapchainExtent.width / (float)app.swapchainExtent.height;
    float viewProj[16];
    cameraViewProj(app.camera, aspect, viewProj);
    const double pxPerUnit = 0.5 * app.swapchainExtent.height / std::tan(30.0 * 3.14159265 / 180.0);
    int best = -1;
    double bestEdge = 20.0, bestDepth = 1e30;
    for (uint32_t i = 0; i < count; i++) {
        if (!app.bodyActive[i] || app.bodyParent[i] >= 0) continue;
        float p[3];
        bodyPosition(app, bodies, i, p);
        float x, y;
        if (!project(viewProj, p, x, y)) continue;
        float w = viewProj[3] * p[0] + viewProj[7] * p[1] + viewProj[11] * p[2] + viewProj[15];
        double sx = (x + 1.0) * 0.5 * app.swapchainExtent.width;
        double sy = (y + 1.0) * 0.5 * app.swapchainExtent.height;
        double center = std::sqrt((sx - mouseX) * (sx - mouseX) + (sy - mouseY) * (sy - mouseY));
        double floorPx = i == 0 ? 8.0 : 3.0;
        double pxRadius = std::fmax(floorPx, bodyRadius(app.trueScale, (int)i) / w * pxPerUnit);
        double edge = std::fmax(0.0, center - pxRadius);
        if (edge < bestEdge || (edge == bestEdge && w < bestDepth && best >= 0)) {
            bestEdge = edge;
            bestDepth = w;
            best = (int)i;
        }
    }
    return best;
}
