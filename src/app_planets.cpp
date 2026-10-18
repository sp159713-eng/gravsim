#include "vk_app.h"
#include "gpu.h"
#include "solarx.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>

std::vector<char> readSpvFile(const std::string& path);
VkShaderModule createSM(VkDevice dev, const std::vector<char>& code);
uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags properties);

namespace {

struct PlanetPush {
    float viewProj[16];
    float camPos[4];
    float time;
    float pxPerUnit;
    float trueMode;
    float sunT;
    float auPerUnit;
    float pad[3];
};

void uploadBuffer(AppContext& app, const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
                  VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = usage;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(app.device, &info, nullptr, &buffer) != VK_SUCCESS)
        throw std::runtime_error("failed sphere buffer!");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(app.device, buffer, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(app.device, &alloc, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("failed sphere memory!");
    vkBindBufferMemory(app.device, buffer, memory, 0);
    void* mapped;
    vkMapMemory(app.device, memory, 0, size, 0, &mapped);
    std::memcpy(mapped, data, size);
    vkUnmapMemory(app.device, memory);
}

}

void createPlanets(AppContext& app) {
    const int stacks = 32, slices = 64;
    std::vector<float> verts;
    std::vector<uint16_t> indices;
    for (int i = 0; i <= stacks; i++) {
        float phi = 3.14159265f * i / stacks;
        for (int j = 0; j <= slices; j++) {
            float theta = 2.0f * 3.14159265f * j / slices;
            verts.push_back(std::sin(phi) * std::cos(theta));
            verts.push_back(std::cos(phi));
            verts.push_back(std::sin(phi) * std::sin(theta));
        }
    }
    for (int i = 0; i < stacks; i++)
        for (int j = 0; j < slices; j++) {
            uint16_t a = (uint16_t)(i * (slices + 1) + j), b = (uint16_t)(a + slices + 1);
            indices.insert(indices.end(), {a, b, (uint16_t)(a + 1), (uint16_t)(a + 1), b, (uint16_t)(b + 1)});
        }
    app.sphereIndexCount = (uint32_t)indices.size();
    float styles[9][8];
    homeStyles(styles);
    uploadBuffer(app, styles, sizeof(styles), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, app.styleBuffer, app.styleMemory);
    vkMapMemory(app.device, app.styleMemory, 0, sizeof(styles), 0, reinterpret_cast<void**>(&app.styleMapped));
    uploadBuffer(app, verts.data(), verts.size() * sizeof(float), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 app.sphereVerts, app.sphereVertMemory);
    uploadBuffer(app, indices.data(), indices.size() * sizeof(uint16_t), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 app.sphereIndices, app.sphereIndexMemory);

    VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PlanetPush)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &push};
    if (vkCreatePipelineLayout(app.device, &layoutInfo, nullptr, &app.planetLayout) != VK_SUCCESS)
        throw std::runtime_error("failed planet layout!");

    VkShaderModule vertSM = createSM(app.device, readSpvFile(shaderPath("planet.vert")));
    VkShaderModule fragSM = createSM(app.device, readSpvFile(shaderPath("planet.frag")));
    VkPipelineShaderStageCreateInfo stages[2]{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertSM, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragSM, "main", nullptr}
    };
    VkVertexInputBindingDescription binds[3]{
        {0, 12, VK_VERTEX_INPUT_RATE_VERTEX},
        {1, sizeof(Particle), VK_VERTEX_INPUT_RATE_INSTANCE},
        {2, 32, VK_VERTEX_INPUT_RATE_INSTANCE}
    };
    VkVertexInputAttributeDescription attrs[4]{
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
        {2, 2, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
        {3, 2, VK_FORMAT_R32G32B32A32_SFLOAT, 16}
    };
    VkPipelineVertexInputStateCreateInfo vertInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0, 3, binds, 4, attrs};
    VkPipelineInputAssemblyStateCreateInfo inputAss{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};
    VkPipelineViewportStateCreateInfo vpState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0, 0, 0, 1.0f};
    VkPipelineMultisampleStateCreateInfo multi{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0, nullptr, VK_FALSE, VK_FALSE};
    VkPipelineColorBlendAttachmentState attBlend{};
    attBlend.colorWriteMask = 0xF;
    attBlend.blendEnable = VK_FALSE;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY, 1, &attBlend, {0,0,0,0}};
    VkDynamicState dyns[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, dyns};
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, stages, &vertInput, &inputAss, nullptr, &vpState, &raster, &multi, nullptr, &blend, &dyn, app.planetLayout, app.renderPass, 0, VK_NULL_HANDLE, -1};
    if (vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &info, nullptr, &app.planetPipeline) != VK_SUCCESS)
        throw std::runtime_error("failed planet pipeline!");
    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
}

void drawPlanets(AppContext& app, VkCommandBuffer cmd, const float viewProj[16], VkBuffer latest) {
    if (app.scene != Scene::SOLAR || app.n < 9) return;
    PlanetPush push{};
    for (int i = 0; i < 16; i++) push.viewProj[i] = viewProj[i];
    const Camera& c = app.camera;
    cameraEye(c, push.camPos);
    push.trueMode = app.trueScale ? 1.0f : 0.0f;
    push.sunT = solarxPlanetSunT(app);
    push.auPerUnit = 1.0f / solarUnitsPerAu(app.trueScale);
    push.time = (float)app.simTime;
    push.pxPerUnit = std::fabs(viewProj[5]) > 0.0f
        ? 0.5f * (float)app.swapchainExtent.height / std::tan(30.0f * 3.14159265f / 180.0f) : 1.0f;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.planetPipeline);
    vkCmdPushConstants(cmd, app.planetLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PlanetPush), &push);
    VkBuffer buffers[3] = {app.sphereVerts, latest, app.styleBuffer};
    VkDeviceSize offsets[3] = {0, 0, 0};
    vkCmdBindVertexBuffers(cmd, 0, 3, buffers, offsets);
    vkCmdBindIndexBuffer(cmd, app.sphereIndices, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(cmd, app.sphereIndexCount, 9, 0, 0, 0);
}

void destroyPlanets(AppContext& app) {
    vkDestroyPipeline(app.device, app.planetPipeline, nullptr);
    vkDestroyPipelineLayout(app.device, app.planetLayout, nullptr);
    vkDestroyBuffer(app.device, app.styleBuffer, nullptr);
    vkFreeMemory(app.device, app.styleMemory, nullptr);
    vkDestroyBuffer(app.device, app.sphereVerts, nullptr);
    vkFreeMemory(app.device, app.sphereVertMemory, nullptr);
    vkDestroyBuffer(app.device, app.sphereIndices, nullptr);
    vkFreeMemory(app.device, app.sphereIndexMemory, nullptr);
}
