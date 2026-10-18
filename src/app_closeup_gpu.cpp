#include "closeup_view.h"
#include "gpu.h"
#include "universe.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

std::vector<char> readSpvFile(const std::string& path);
VkShaderModule createSM(VkDevice dev, const std::vector<char>& code);
uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags properties);

namespace {

VkPipelineLayout g_layout = VK_NULL_HANDLE;
VkPipeline g_pipe = VK_NULL_HANDLE;
VkBuffer g_buf[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
VkDeviceMemory g_mem[2] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
void* g_map[2] = {nullptr, nullptr};

void makeBuffer(AppContext& app, int i, uint32_t count) {
    VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    binfo.size = sizeof(Particle) * count;
    binfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    binfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(app.device, &binfo, nullptr, &g_buf[i]) != VK_SUCCESS) throw std::runtime_error("failed closeup buffer!");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(app.device, g_buf[i], &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(app.device, &alloc, nullptr, &g_mem[i]) != VK_SUCCESS) throw std::runtime_error("failed closeup memory!");
    vkBindBufferMemory(app.device, g_buf[i], g_mem[i], 0);
    vkMapMemory(app.device, g_mem[i], 0, binfo.size, 0, &g_map[i]);
    std::memset(g_map[i], 0, (size_t)binfo.size);
}

}

void createCloseup(AppContext& app) {
    VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, 128};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &push};
    if (vkCreatePipelineLayout(app.device, &layoutInfo, nullptr, &g_layout) != VK_SUCCESS) throw std::runtime_error("failed closeup layout!");
    VkShaderModule vertSM = createSM(app.device, readSpvFile(shaderPath("sky.vert")));
    VkShaderModule fragSM = createSM(app.device, readSpvFile(shaderPath("closeup.frag")));
    VkPipelineShaderStageCreateInfo stages[2]{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertSM, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragSM, "main", nullptr}
    };
    VkPipelineVertexInputStateCreateInfo vertInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo inputAss{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};
    VkPipelineViewportStateCreateInfo vpState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0, 0, 0, 1.0f};
    VkPipelineMultisampleStateCreateInfo multi{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0, nullptr, VK_FALSE, VK_FALSE};
    VkPipelineColorBlendAttachmentState att{};
    att.colorWriteMask = 0xF;
    att.blendEnable = VK_TRUE;
    att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att.colorBlendOp = VK_BLEND_OP_ADD;
    att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    att.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY, 1, &att, {0, 0, 0, 0}};
    VkDynamicState dyns[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, dyns};
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, stages, &vertInput, &inputAss, nullptr, &vpState, &raster, &multi, nullptr, &blend, &dyn, g_layout, app.renderPass, 0, VK_NULL_HANDLE, -1};
    if (vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &info, nullptr, &g_pipe) != VK_SUCCESS) throw std::runtime_error("failed closeup pipeline!");
    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
    makeBuffer(app, 0, 1);
    makeBuffer(app, 1, 2);
}

void destroyCloseup(AppContext& app) {
    vkDestroyPipeline(app.device, g_pipe, nullptr);
    vkDestroyPipelineLayout(app.device, g_layout, nullptr);
    for (int i = 0; i < 2; i++) {
        vkDestroyBuffer(app.device, g_buf[i], nullptr);
        vkFreeMemory(app.device, g_mem[i], nullptr);
    }
}

bool closeupDraw(AppContext& app, VkCommandBuffer cmd, float aspect) {
    CuDraw d;
    if (!closeupFill(app, aspect, d)) return false;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipe);
    vkCmdPushConstants(cmd, g_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 128, d.push);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    if (d.bh) {
        Particle p{};
        for (int a = 0; a < 3; a++) {
            p.pos[a] = d.bhPos[a];
            p.vel[a] = d.bhNormal[a];
        }
        p.pos[3] = 3.7e10f;
        std::memcpy(g_map[0], &p, sizeof(p));
        drawHoleCustom(app, cmd, g_buf[0], 1, d.vp, d.cam, d.bhRadius, d.galMode);
    }
    return true;
}

void drawFreeHoles(AppContext& app, VkCommandBuffer cmd, float aspect) {
    if (universeFade(app) < 0.05f || app.gal.active) return;
    static std::vector<UniLabel> labels = universeLabels();
    float ge[3];
    galaxyEye(app, ge);
    double eye[3];
    for (int a = 0; a < 3; a++) eye[a] = app.gal.originMly[a] + (double)ge[a] * 1e-6;
    Particle p[2]{};
    int shown = 0, slot = 0;
    for (const UniLabel& l : labels) {
        if (std::strcmp(l.text, "TON 618") != 0 && std::strcmp(l.text, "PHOENIX CLUSTER") != 0) continue;
        double dx = l.pos[0] - eye[0], dy = l.pos[1] - eye[1], dz = l.pos[2] - eye[2];
        double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (slot < 2) {
            const float n[3] = {0.28f, 0.88f, 0.38f};
            for (int a = 0; a < 3; a++) {
                p[slot].pos[a] = l.pos[a];
                p[slot].vel[a] = n[a];
            }
            p[slot].pos[3] = dist < 40.0 ? 3.7e10f : 0.0f;
            if (dist < 40.0) shown++;
            slot++;
        }
    }
    if (!shown) return;
    std::memcpy(g_map[1], p, sizeof(p));
    float vp[16];
    universeViewProj(app, aspect, vp);
    drawHoleCustom(app, cmd, g_buf[1], 2, vp, app.camera, 0.03f, 1.0f);
}
