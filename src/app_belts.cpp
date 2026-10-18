#include "belts.h"
#include "gpu.h"
#include "solarx.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

std::vector<char> readSpvFile(const std::string& path);
VkShaderModule createSM(VkDevice dev, const std::vector<char>& code);
uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags properties);

namespace {

struct BeltPush {
    float vp[16];
    float sun[4];
    float eye[4];
    float tm[4];
};

}

bool beltsActive(const AppContext& app) {
    return g_belts.on && app.scene == Scene::SOLAR && solarxHome(app) && app.trueScale;
}

void createBelts(AppContext& app) {
    std::vector<BeltVertex> pts = beltsGenerate();
    g_belts.count = (uint32_t)pts.size();
    VkDeviceSize size = pts.size() * sizeof(BeltVertex);
    VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    binfo.size = size;
    binfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    binfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(app.device, &binfo, nullptr, &g_belts.buffer) != VK_SUCCESS) throw std::runtime_error("failed belt buffer!");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(app.device, g_belts.buffer, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(app.device, &alloc, nullptr, &g_belts.memory) != VK_SUCCESS) throw std::runtime_error("failed belt memory!");
    vkBindBufferMemory(app.device, g_belts.buffer, g_belts.memory, 0);
    void* data;
    vkMapMemory(app.device, g_belts.memory, 0, size, 0, &data);
    std::memcpy(data, pts.data(), (size_t)size);
    vkUnmapMemory(app.device, g_belts.memory);

    VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(BeltPush)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &push};
    if (vkCreatePipelineLayout(app.device, &layoutInfo, nullptr, &g_belts.layout) != VK_SUCCESS) throw std::runtime_error("failed belt layout!");
    VkShaderModule vertSM = createSM(app.device, readSpvFile(shaderPath("belts.vert")));
    VkShaderModule fragSM = createSM(app.device, readSpvFile(shaderPath("belts.frag")));
    VkPipelineShaderStageCreateInfo stages[2]{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertSM, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragSM, "main", nullptr}
    };
    VkVertexInputBindingDescription bind{0, sizeof(BeltVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[4]{
        {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16},
        {2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 32},
        {3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 48}
    };
    VkPipelineVertexInputStateCreateInfo vertInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0, 1, &bind, 4, attrs};
    VkPipelineInputAssemblyStateCreateInfo inputAss{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_FALSE};
    VkPipelineViewportStateCreateInfo vpState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0, 0, 0, 1.0f};
    VkPipelineMultisampleStateCreateInfo multi{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0, nullptr, VK_FALSE, VK_FALSE};
    VkPipelineColorBlendAttachmentState att{};
    att.colorWriteMask = 0xF;
    att.blendEnable = VK_TRUE;
    att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    att.colorBlendOp = VK_BLEND_OP_ADD;
    att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    att.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY, 1, &att, {0, 0, 0, 0}};
    VkDynamicState dyns[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, dyns};
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, stages, &vertInput, &inputAss, nullptr, &vpState, &raster, &multi, nullptr, &blend, &dyn, g_belts.layout, app.renderPass, 0, VK_NULL_HANDLE, -1};
    if (vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &info, nullptr, &g_belts.pipeline) != VK_SUCCESS) throw std::runtime_error("failed belt pipeline!");
    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
}

void destroyBelts(AppContext& app) {
    vkDestroyPipeline(app.device, g_belts.pipeline, nullptr);
    vkDestroyPipelineLayout(app.device, g_belts.layout, nullptr);
    vkDestroyBuffer(app.device, g_belts.buffer, nullptr);
    vkFreeMemory(app.device, g_belts.memory, nullptr);
}

void drawBelts(AppContext& app, VkCommandBuffer cmd, const float viewProj[16]) {
    if (!beltsActive(app) || app.refDist > 2000.0f) return;
    BeltPush p;
    (void)viewProj;
    cameraViewProjEx(app.camera, (float)app.swapchainExtent.width / (float)app.swapchainExtent.height, nullptr, 1e-4f, 2000.0f, p.vp);
    const Particle& s = app.displayBodies[0];
    p.sun[0] = s.pos[0];
    p.sun[1] = s.pos[1];
    p.sun[2] = s.pos[2];
    p.sun[3] = solarUnitsPerAu(true);
    float eye[3];
    cameraEye(app.camera, eye);
    p.eye[0] = eye[0];
    p.eye[1] = eye[1];
    p.eye[2] = eye[2];
    p.eye[3] = 0.7f;
    double t = solarxJd(app) - 2451545.0;
    const double TWO_PI = 6.283185307179586;
    p.tm[0] = (float)t;
    p.tm[1] = (float)std::fmod(beltsLamJ(t), 2.0 * TWO_PI);
    p.tm[2] = (float)std::fmod(beltsLamN(t), 3.0 * TWO_PI);
    p.tm[3] = 0.5f * (float)app.swapchainExtent.height / std::tan(30.0f * 3.14159265f / 180.0f);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, g_belts.pipeline);
    vkCmdPushConstants(cmd, g_belts.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(BeltPush), &p);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &g_belts.buffer, &offset);
    vkCmdDraw(cmd, g_belts.count, 1, 0, 0);
}

void beltsStep(AppContext& app) {
    bool key = !timeTyping() && glfwGetKey(app.window, GLFW_KEY_Y) == GLFW_PRESS;
    if (key && !g_belts.keyDown && app.scene == Scene::SOLAR && solarxHome(app)) g_belts.on = !g_belts.on;
    g_belts.keyDown = key;
}
