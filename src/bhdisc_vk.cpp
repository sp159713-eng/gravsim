#include "vk_app.h"
#include "gpu.h"
#include "bhdisc.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

std::vector<char> readSpvFile(const std::string& path);
VkShaderModule createSM(VkDevice dev, const std::vector<char>& code);
uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags properties);

namespace {

BdState S;

constexpr VkDeviceSize PART_BYTES = (VkDeviceSize)BD_N * 32;
constexpr VkDeviceSize GRID_BYTES = (VkDeviceSize)128 * 8 * 128 * 5 * 4;
constexpr VkDeviceSize RAD_BYTES = 256 * 4;
constexpr VkDeviceSize STAT_BYTES = 64;

void check(VkResult r, const char* what) {
    if (r != VK_SUCCESS) throw std::runtime_error(what);
}

void makeBuffer(AppContext& app, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buf, VkDeviceMemory& mem) {
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateBuffer(app.device, &bi, nullptr, &buf), "bd buffer");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(app.device, buf, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits, props);
    check(vkAllocateMemory(app.device, &ai, nullptr, &mem), "bd memory");
    vkBindBufferMemory(app.device, buf, mem, 0);
}

VkPipelineShaderStageCreateInfo stage(VkShaderStageFlagBits s, VkShaderModule m) {
    return {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, s, m, "main", nullptr};
}

void barrier(VkCommandBuffer cmd, VkPipelineStageFlags dst, VkAccessFlags dstAcc) {
    VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, dstAcc};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, dst, 0, 1, &mb, 0, nullptr, 0, nullptr);
}

const VkAccessFlags CT_ACC = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
const VkPipelineStageFlags CT_STAGE = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT;

void pushStep(VkCommandBuffer cmd, BdStepPush& p) {
    vkCmdPushConstants(cmd, S.cLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BdStepPush), &p);
}

void dispatchAll(VkCommandBuffer cmd) {
    vkCmdDispatch(cmd, BD_N / 256, 1, 1);
}

}

BdState& bdState() { return S; }

void bdCreateVk(AppContext& app) {
    if (S.created) return;
    VkMemoryPropertyFlags dev = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkBufferUsageFlags st = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    makeBuffer(app, PART_BYTES, st, dev, S.parts, S.memParts);
    makeBuffer(app, GRID_BYTES, st, dev, S.grid, S.memGrid);
    makeBuffer(app, RAD_BYTES, st, dev, S.rad, S.memRad);
    for (int f = 0; f < 2; f++) {
        makeBuffer(app, STAT_BYTES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, S.stats[f], S.memStats[f]);
        void* p;
        vkMapMemory(app.device, S.memStats[f], 0, STAT_BYTES, 0, &p);
        S.statMap[f] = (uint32_t*)p;
        std::memset(p, 0, STAT_BYTES);
    }
    {
        VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = PART_BYTES;
        bi.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkMemoryRequirements req{};
        VkBuffer probe;
        check(vkCreateBuffer(app.device, &bi, nullptr, &probe), "bd snap");
        vkGetBufferMemoryRequirements(app.device, probe, &req);
        vkDestroyBuffer(app.device, probe, nullptr);
        VkDeviceSize each = (req.size + req.alignment - 1) / req.alignment * req.alignment;
        VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        ai.allocationSize = each * BD_SNAPS;
        ai.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits, dev);
        check(vkAllocateMemory(app.device, &ai, nullptr, &S.memSnap), "bd snap memory");
        for (int i = 0; i < BD_SNAPS; i++) {
            check(vkCreateBuffer(app.device, &bi, nullptr, &S.snapBuf[i]), "bd snap buffer");
            vkBindBufferMemory(app.device, S.snapBuf[i], S.memSnap, each * i);
        }
    }

    VkDescriptorSetLayoutBinding binds[4];
    for (uint32_t i = 0; i < 4; i++)
        binds[i] = {i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo dli{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 4, binds};
    check(vkCreateDescriptorSetLayout(app.device, &dli, nullptr, &S.dsl), "bd dsl");
    VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8};
    VkDescriptorPoolCreateInfo dpi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0, 2, 1, &ps};
    check(vkCreateDescriptorPool(app.device, &dpi, nullptr, &S.pool), "bd pool");
    VkDescriptorSetLayout ls[2] = {S.dsl, S.dsl};
    VkDescriptorSetAllocateInfo dai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, S.pool, 2, ls};
    check(vkAllocateDescriptorSets(app.device, &dai, S.ds), "bd sets");
    for (int f = 0; f < 2; f++) {
        VkDescriptorBufferInfo bi[4] = {{S.parts, 0, PART_BYTES}, {S.grid, 0, GRID_BYTES}, {S.rad, 0, RAD_BYTES}, {S.stats[f], 0, STAT_BYTES}};
        VkWriteDescriptorSet w[4];
        for (uint32_t i = 0; i < 4; i++)
            w[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, S.ds[f], i, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bi[i], nullptr};
        vkUpdateDescriptorSets(app.device, 4, w, 0, nullptr);
    }

    VkPushConstantRange cpr{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BdStepPush)};
    VkPipelineLayoutCreateInfo cli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &S.dsl, 1, &cpr};
    check(vkCreatePipelineLayout(app.device, &cli, nullptr, &S.cLayout), "bd clayout");
    VkShaderModule cs = createSM(app.device, readSpvFile(shaderPath("bhdisc_step.comp")));
    VkComputePipelineCreateInfo cpi{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, nullptr, 0, stage(VK_SHADER_STAGE_COMPUTE_BIT, cs), S.cLayout, VK_NULL_HANDLE, -1};
    check(vkCreateComputePipelines(app.device, VK_NULL_HANDLE, 1, &cpi, nullptr, &S.cPipe), "bd cpipe");
    vkDestroyShaderModule(app.device, cs, nullptr);

    VkPushConstantRange gpr{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(BdDrawPush)};
    VkPipelineLayoutCreateInfo gli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &S.dsl, 1, &gpr};
    check(vkCreatePipelineLayout(app.device, &gli, nullptr, &S.gLayout), "bd glayout");
    VkShaderModule vs = createSM(app.device, readSpvFile(shaderPath("bhdisc.vert")));
    VkShaderModule fs = createSM(app.device, readSpvFile(shaderPath("bhdisc.frag")));
    VkPipelineShaderStageCreateInfo stages[2] = {stage(VK_SHADER_STAGE_VERTEX_BIT, vs), stage(VK_SHADER_STAGE_FRAGMENT_BIT, fs)};
    VkPipelineVertexInputStateCreateInfo vin{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_FALSE};
    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0, 0, 0, 1.0f};
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0, nullptr, VK_FALSE, VK_FALSE};
    VkPipelineColorBlendAttachmentState ab{};
    ab.colorWriteMask = 0xF;
    ab.blendEnable = VK_TRUE;
    ab.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    ab.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    ab.colorBlendOp = VK_BLEND_OP_ADD;
    ab.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    ab.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    ab.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY, 1, &ab, {0, 0, 0, 0}};
    VkDynamicState dyns[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, dyns};
    VkGraphicsPipelineCreateInfo gi{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, stages, &vin, &ia, nullptr, &vp, &rs, &ms, nullptr, &cb, &dyn, S.gLayout, app.renderPass, 0, VK_NULL_HANDLE, -1};
    check(vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &gi, nullptr, &S.gPipe), "bd gpipe");
    vkDestroyShaderModule(app.device, fs, nullptr);
    vkDestroyShaderModule(app.device, vs, nullptr);
    S.created = true;
}

void bdDestroyVk(AppContext& app) {
    if (!S.created) return;
    vkDeviceWaitIdle(app.device);
    vkDestroyPipeline(app.device, S.gPipe, nullptr);
    vkDestroyPipeline(app.device, S.cPipe, nullptr);
    vkDestroyPipelineLayout(app.device, S.gLayout, nullptr);
    vkDestroyPipelineLayout(app.device, S.cLayout, nullptr);
    vkDestroyDescriptorPool(app.device, S.pool, nullptr);
    vkDestroyDescriptorSetLayout(app.device, S.dsl, nullptr);
    for (int i = 0; i < BD_SNAPS; i++) vkDestroyBuffer(app.device, S.snapBuf[i], nullptr);
    vkFreeMemory(app.device, S.memSnap, nullptr);
    for (int f = 0; f < 2; f++) {
        vkUnmapMemory(app.device, S.memStats[f]);
        vkDestroyBuffer(app.device, S.stats[f], nullptr);
        vkFreeMemory(app.device, S.memStats[f], nullptr);
    }
    vkDestroyBuffer(app.device, S.rad, nullptr);
    vkFreeMemory(app.device, S.memRad, nullptr);
    vkDestroyBuffer(app.device, S.grid, nullptr);
    vkFreeMemory(app.device, S.memGrid, nullptr);
    vkDestroyBuffer(app.device, S.parts, nullptr);
    vkFreeMemory(app.device, S.memParts, nullptr);
    S.created = false;
}

void createBhDisc(AppContext&) {}

void destroyBhDisc(AppContext& app) { bdDestroyVk(app); }

void bdRecord(AppContext& app, VkCommandBuffer cmd) {
    (void)app;
    if (!S.created || S.ops.empty()) return;
    int f = app.currentFrame;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, S.cPipe);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, S.cLayout, 0, 1, &S.ds[f], 0, nullptr);
    barrier(cmd, CT_STAGE, CT_ACC);
    for (BdOp& op : S.ops) {
        if (op.kind == 0) {
            pushStep(cmd, op.push);
            dispatchAll(cmd);
            barrier(cmd, CT_STAGE, CT_ACC);
        } else if (op.kind == 1) {
            vkCmdFillBuffer(cmd, S.grid, 0, GRID_BYTES, 0);
            barrier(cmd, CT_STAGE, CT_ACC);
            op.push.mode = 0;
            pushStep(cmd, op.push);
            dispatchAll(cmd);
            barrier(cmd, CT_STAGE, CT_ACC);
            op.push.mode = 1;
            pushStep(cmd, op.push);
            dispatchAll(cmd);
            barrier(cmd, CT_STAGE, CT_ACC);
        } else if (op.kind == 2) {
            VkBufferCopy c{0, 0, PART_BYTES};
            vkCmdCopyBuffer(cmd, S.snapBuf[op.idx], S.parts, 1, &c);
            barrier(cmd, CT_STAGE, CT_ACC);
        } else if (op.kind == 3) {
            VkBufferCopy c{0, 0, PART_BYTES};
            vkCmdCopyBuffer(cmd, S.parts, S.snapBuf[op.idx], 1, &c);
            barrier(cmd, CT_STAGE, CT_ACC);
        }
    }
    barrier(cmd, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
    S.ops.clear();
}

void drawBhDisc(AppContext& app, VkCommandBuffer cmd, float aspect, const float viewProj[16]) {
    (void)aspect;
    if (!S.created) return;
    float eye[3];
    cameraEye(app.camera, eye);
    float H = (float)app.swapchainExtent.height;
    BdDrawPush p{};
    std::memcpy(p.viewProj, viewProj, 64);
    p.eye[0] = eye[0]; p.eye[1] = eye[1]; p.eye[2] = eye[2];
    float sm = std::clamp(((float)S.ts - 26.0f) / 26.0f, 0.0f, 1.0f);
    p.eye[3] = S.scen == 2 ? 3300.0f : 4700.0f + (2800.0f - 4700.0f) * sm * sm * (3.0f - 2.0f * sm);
    p.prm[0] = 0.5f * H * 1.7320508f;
    p.prm[2] = (float)S.clock;
    p.prm[3] = S.scen == 2 ? 1.0f : 0.0f;
    p.prm2[0] = (float)BD_N;
    p.prm2[1] = S.jet;
    p.prm2[2] = (float)BD_JET;
    p.prm2[3] = 0.13f * (S.scen == 2 ? 0.6f : 1.0f);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.gLayout, 0, 1, &S.ds[app.currentFrame], 0, nullptr);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.gPipe);
    p.prm[1] = 0.0f;
    vkCmdPushConstants(cmd, S.gLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(p), &p);
    vkCmdDraw(cmd, BD_N * 2, 1, 0, 0);
    drawBhDiscHole(app, cmd, viewProj, 4.7f, (float)S.clock);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.gLayout, 0, 1, &S.ds[app.currentFrame], 0, nullptr);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.gPipe);
    p.prm[1] = 1.0f;
    vkCmdPushConstants(cmd, S.gLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(p), &p);
    vkCmdDraw(cmd, BD_N + BD_JET + 1, 1, 0, 0);
}
