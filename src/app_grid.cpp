#include "vk_app.h"
#include "gpu.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

std::vector<char> readSpvFile(const std::string& path);
VkShaderModule createSM(VkDevice dev, const std::vector<char>& code);

void createGridPipeline(AppContext& app) {
    VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GridPush)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &app.computeDSL, 1, &push};
    if (vkCreatePipelineLayout(app.device, &layoutInfo, nullptr, &app.gridLayout) != VK_SUCCESS)
        throw std::runtime_error("failed grid layout!");

    VkShaderModule vertSM = createSM(app.device, readSpvFile(shaderPath("grid.vert")));
    VkShaderModule fragSM = createSM(app.device, readSpvFile(shaderPath("grid.frag")));
    VkPipelineShaderStageCreateInfo stages[2]{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertSM, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragSM, "main", nullptr}
    };

    VkPipelineVertexInputStateCreateInfo vertInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo inputAss{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_FALSE};
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

    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, stages, &vertInput, &inputAss, nullptr, &vpState, &raster, &multi, nullptr, &blend, &dyn, app.gridLayout, app.renderPass, 0, VK_NULL_HANDLE, -1};
    if (vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &info, nullptr, &app.gridPipeline) != VK_SUCCESS)
        throw std::runtime_error("failed grid pipeline!");

    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
}

void drawGrid(AppContext& app, VkCommandBuffer cmd, uint32_t setIndex, const float viewProj[16]) {
    if (!app.showGrid) return;
    GridPush push{};
    for (int i = 0; i < 16; i++) push.viewProj[i] = viewProj[i];
    push.n = app.scene != Scene::SOLAR ? app.n : (app.n < 9 ? app.n : 9);
    push.stride = app.scene != Scene::SOLAR && app.n > 128 ? app.n / 128 : 1;
    push.solar = app.scene != Scene::SOLAR ? 0 : 1;
    if (app.scene == Scene::ANDROMEDA) {
        push.n = std::min(app.n, ANDROMEDA_N);
        push.stride = std::max(1u, push.n / 24u);
    }
    const bool collision = collisionLike(app.scene);
    float eye[3];
    cameraEye(app.camera, eye);
    const float pitch = app.camera.pitch, yaw = app.camera.yaw;
    const float fx = -std::cos(pitch) * std::sin(yaw), fy = -std::sin(pitch), fz = -std::cos(pitch) * std::cos(yaw);
    const bool solar = app.scene == Scene::SOLAR;
    const float baseDist = collision ? 100.0f : 50.0f;
    const float fh = std::sqrt(fx * fx + fz * fz);
    float cx = eye[0], cz = eye[2];
    int level = 0;
    for (int it = 0; it < 4; it++) {
        const float ws = solar ? std::ldexp(1.0f, level) : 1.0f;
        const float h = std::max(std::fabs(eye[1] + 3.0f * ws), 1e-6f);
        const float reach = fy < -0.02f ? std::min(h * fh / -fy, 4.0f * h) : 3.0f * h;
        const float rx = fh > 1e-6f ? fx / fh * reach : 0.0f, rz = fh > 1e-6f ? fz / fh * reach : 0.0f;
        cx = eye[0] + rx;
        cz = eye[2] + rz;
        const float dist = std::max(std::sqrt(rx * rx + h * h + rz * rz), 1e-6f);
        const int next = std::max(-24, std::min(14, (int)std::floor(std::log2(dist / baseDist))));
        if (next == level) break;
        level = next;
    }
    const float scale = std::ldexp(1.0f, level);
    const float baseExtent = collision ? 60.0f : 55.0f;
    push.extent = baseExtent * scale;
    push.scale = scale;
    const double cell = 2.0 * push.extent / 499.0;
    cx = (float)(std::round((double)cx / cell) * cell);
    cz = (float)(std::round((double)cz / cell) * cell);
    push.cx = cx;
    push.cz = cz;
    gwFill(app, push.gw);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.gridPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.gridLayout, 0, 1, &app.descriptorSets[setIndex], 0, nullptr);
    vkCmdPushConstants(cmd, app.gridLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GridPush), &push);
    vkCmdDraw(cmd, GRID_VERTS, 1, 0, 0);
}
