#include "vk_app.h"
#include "gpu.h"
#include <algorithm>
#include <stdexcept>
#include <string>

std::vector<char> readSpvFile(const std::string& path);
VkShaderModule createSM(VkDevice dev, const std::vector<char>& code);
uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags properties);

namespace {

struct BloomPush {
    float a, b, c, d;
};

const float THRESHOLD = 0.45f;
const float STRENGTH = 1.0f;

VkRenderPass makePass(AppContext& app, VkAttachmentLoadOp load) {
    VkAttachmentDescription att{};
    att.format = app.swapchainFormat;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = load;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ref;
    VkSubpassDependency deps[2]{};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    info.attachmentCount = 1;
    info.pAttachments = &att;
    info.subpassCount = 1;
    info.pSubpasses = &sub;
    info.dependencyCount = 2;
    info.pDependencies = deps;
    VkRenderPass pass;
    if (vkCreateRenderPass(app.device, &info, nullptr, &pass) != VK_SUCCESS) throw std::runtime_error("failed bloom pass!");
    return pass;
}

VkPipeline makePipeline(AppContext& app, VkRenderPass pass, const char* frag) {
    VkShaderModule vertSM = createSM(app.device, readSpvFile(shaderPath("sky.vert")));
    VkShaderModule fragSM = createSM(app.device, readSpvFile(shaderPath(frag)));
    VkPipelineShaderStageCreateInfo stages[2]{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertSM, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragSM, "main", nullptr}
    };
    VkPipelineVertexInputStateCreateInfo vertInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkPipelineInputAssemblyStateCreateInfo inputAss{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};
    VkPipelineViewportStateCreateInfo vpState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0, 0, 0, 1.0f};
    VkPipelineMultisampleStateCreateInfo multi{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0, nullptr, VK_FALSE, VK_FALSE};
    VkPipelineColorBlendAttachmentState attBlend{};
    attBlend.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY, 1, &attBlend, {0, 0, 0, 0}};
    VkDynamicState dyns[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, dyns};
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, stages, &vertInput, &inputAss, nullptr, &vpState, &raster, &multi, nullptr, &blend, &dyn, app.bloom.layout, pass, 0, VK_NULL_HANDLE, -1};
    VkPipeline pipeline;
    if (vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("failed bloom pipeline!");
    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
    return pipeline;
}

void makeImage(AppContext& app, BloomImg& img, VkRenderPass pass, uint32_t w, uint32_t h) {
    img.ext = {std::max(1u, w), std::max(1u, h)};
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = app.swapchainFormat;
    info.extent = {img.ext.width, img.ext.height, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(app.device, &info, nullptr, &img.image) != VK_SUCCESS) throw std::runtime_error("failed bloom image!");
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(app.device, img.image, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(app.device, &alloc, nullptr, &img.memory) != VK_SUCCESS) throw std::runtime_error("failed bloom memory!");
    vkBindImageMemory(app.device, img.image, img.memory, 0);
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = img.image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = app.swapchainFormat;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(app.device, &view, nullptr, &img.view) != VK_SUCCESS) throw std::runtime_error("failed bloom view!");
    VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fb.renderPass = pass;
    fb.attachmentCount = 1;
    fb.pAttachments = &img.view;
    fb.width = img.ext.width;
    fb.height = img.ext.height;
    fb.layers = 1;
    if (vkCreateFramebuffer(app.device, &fb, nullptr, &img.fb) != VK_SUCCESS) throw std::runtime_error("failed bloom framebuffer!");
}

void freeImage(AppContext& app, BloomImg& img) {
    if (img.fb) vkDestroyFramebuffer(app.device, img.fb, nullptr);
    if (img.view) vkDestroyImageView(app.device, img.view, nullptr);
    if (img.image) vkDestroyImage(app.device, img.image, nullptr);
    if (img.memory) vkFreeMemory(app.device, img.memory, nullptr);
    img = BloomImg{};
}

void writeSet(AppContext& app, VkDescriptorSet set, VkImageView v0, VkImageView v1, VkImageView v2) {
    VkImageView views[3] = {v0, v1, v2};
    VkDescriptorImageInfo infos[3];
    VkWriteDescriptorSet writes[3];
    for (int i = 0; i < 3; i++) {
        infos[i] = {app.bloom.sampler, views[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        writes[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, set, (uint32_t)i, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &infos[i], nullptr, nullptr};
    }
    vkUpdateDescriptorSets(app.device, 3, writes, 0, nullptr);
}

void setViewport(VkCommandBuffer cmd, VkExtent2D ext) {
    VkViewport viewport{0.0f, 0.0f, (float)ext.width, (float)ext.height, 0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor{{0, 0}, ext};
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

void runPass(AppContext& app, VkCommandBuffer cmd, const BloomImg& dst, VkPipeline pipeline, VkDescriptorSet set, BloomPush push) {
    VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    begin.renderPass = app.bloom.passRP;
    begin.framebuffer = dst.fb;
    begin.renderArea = {{0, 0}, dst.ext};
    vkCmdBeginRenderPass(cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);
    setViewport(cmd, dst.ext);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.bloom.layout, 0, 1, &set, 0, nullptr);
    vkCmdPushConstants(cmd, app.bloom.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BloomPush), &push);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);
}

}

bool bloomActive(const AppContext& app) {
    return app.showBloom && app.bloom.ready && app.scene != Scene::STARSIZES && app.scene != Scene::SANDBOX && app.scene != Scene::EARTHMOON && app.scene != Scene::FUTURESKY && app.scene != Scene::NIGHTSKY && app.scene != Scene::IMPACT && app.scene != Scene::ECLIPSE;
}

void createBloom(AppContext& app) {
    BloomState& b = app.bloom;
    VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.minFilter = VK_FILTER_LINEAR;
    sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (vkCreateSampler(app.device, &sampler, nullptr, &b.sampler) != VK_SUCCESS) throw std::runtime_error("failed bloom sampler!");
    VkDescriptorSetLayoutBinding bindings[3];
    for (uint32_t i = 0; i < 3; i++) bindings[i] = {i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo dsl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 3, bindings};
    if (vkCreateDescriptorSetLayout(app.device, &dsl, nullptr, &b.dsl) != VK_SUCCESS) throw std::runtime_error("failed bloom layout!");
    VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 15};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0, 5, 1, &size};
    if (vkCreateDescriptorPool(app.device, &pool, nullptr, &b.pool) != VK_SUCCESS) throw std::runtime_error("failed bloom pool!");
    VkDescriptorSetLayout layouts[5] = {b.dsl, b.dsl, b.dsl, b.dsl, b.dsl};
    VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, b.pool, 5, layouts};
    if (vkAllocateDescriptorSets(app.device, &alloc, b.sets) != VK_SUCCESS) throw std::runtime_error("failed bloom sets!");
    VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BloomPush)};
    VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &b.dsl, 1, &push};
    if (vkCreatePipelineLayout(app.device, &layout, nullptr, &b.layout) != VK_SUCCESS) throw std::runtime_error("failed bloom pipeline layout!");
    b.sceneRP = makePass(app, VK_ATTACHMENT_LOAD_OP_CLEAR);
    b.passRP = makePass(app, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    b.bright = makePipeline(app, b.passRP, "bloom_bright.frag");
    b.blur = makePipeline(app, b.passRP, "bloom_blur.frag");
    b.comp = makePipeline(app, app.renderPass, "bloom_comp.frag");
    b.ready = true;
    bloomCreateTargets(app);
}

void bloomCreateTargets(AppContext& app) {
    BloomState& b = app.bloom;
    if (!b.ready) return;
    uint32_t w = app.swapchainExtent.width, h = app.swapchainExtent.height;
    makeImage(app, b.scene, b.sceneRP, w, h);
    makeImage(app, b.a0, b.passRP, w / 4, h / 4);
    makeImage(app, b.b0, b.passRP, w / 4, h / 4);
    makeImage(app, b.a1, b.passRP, w / 8, h / 8);
    makeImage(app, b.b1, b.passRP, w / 8, h / 8);
    writeSet(app, b.sets[0], b.scene.view, b.scene.view, b.scene.view);
    writeSet(app, b.sets[1], b.a0.view, b.a0.view, b.a0.view);
    writeSet(app, b.sets[2], b.b0.view, b.b0.view, b.b0.view);
    writeSet(app, b.sets[3], b.b1.view, b.b1.view, b.b1.view);
    writeSet(app, b.sets[4], b.scene.view, b.a0.view, b.a1.view);
}

void bloomDestroyTargets(AppContext& app) {
    BloomState& b = app.bloom;
    freeImage(app, b.scene);
    freeImage(app, b.a0);
    freeImage(app, b.b0);
    freeImage(app, b.a1);
    freeImage(app, b.b1);
}

void destroyBloom(AppContext& app) {
    BloomState& b = app.bloom;
    if (!b.ready) return;
    bloomDestroyTargets(app);
    vkDestroyPipeline(app.device, b.bright, nullptr);
    vkDestroyPipeline(app.device, b.blur, nullptr);
    vkDestroyPipeline(app.device, b.comp, nullptr);
    vkDestroyPipelineLayout(app.device, b.layout, nullptr);
    vkDestroyDescriptorPool(app.device, b.pool, nullptr);
    vkDestroyDescriptorSetLayout(app.device, b.dsl, nullptr);
    vkDestroySampler(app.device, b.sampler, nullptr);
    vkDestroyRenderPass(app.device, b.sceneRP, nullptr);
    vkDestroyRenderPass(app.device, b.passRP, nullptr);
    b.ready = false;
}

void bloomBeginScene(AppContext& app, VkCommandBuffer cmd) {
    VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    begin.renderPass = app.bloom.sceneRP;
    begin.framebuffer = app.bloom.scene.fb;
    begin.renderArea = {{0, 0}, app.bloom.scene.ext};
    VkClearValue clear = {{{0.02f, 0.02f, 0.04f, 1.0f}}};
    begin.clearValueCount = 1;
    begin.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &begin, VK_SUBPASS_CONTENTS_INLINE);
    setViewport(cmd, app.bloom.scene.ext);
}

void bloomEndScene(AppContext& app, VkCommandBuffer cmd) {
    BloomState& b = app.bloom;
    vkCmdEndRenderPass(cmd);
    runPass(app, cmd, b.a0, b.bright, b.sets[0], {1.0f / b.scene.ext.width, 1.0f / b.scene.ext.height, THRESHOLD, 0.0f});
    runPass(app, cmd, b.b0, b.blur, b.sets[1], {1.0f / b.a0.ext.width, 0.0f, 0.0f, 0.0f});
    runPass(app, cmd, b.a0, b.blur, b.sets[2], {0.0f, 1.0f / b.b0.ext.height, 0.0f, 0.0f});
    runPass(app, cmd, b.b1, b.blur, b.sets[1], {2.0f / b.a0.ext.width, 0.0f, 0.0f, 0.0f});
    runPass(app, cmd, b.a1, b.blur, b.sets[3], {0.0f, 1.0f / b.b1.ext.height, 0.0f, 0.0f});
}

void bloomComposite(AppContext& app, VkCommandBuffer cmd) {
    BloomState& b = app.bloom;
    BloomPush push{0.0f, 0.0f, THRESHOLD, STRENGTH};
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.comp);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.layout, 0, 1, &b.sets[4], 0, nullptr);
    vkCmdPushConstants(cmd, b.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BloomPush), &push);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}
