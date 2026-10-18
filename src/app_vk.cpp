#include "vk_app.h"
#include "gpu.h"
#include <fstream>
#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>
#include <optional>

std::vector<char> readSpvFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("failed to open file: " + path);
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    return buffer;
}

uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(phys, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("failed to find suitable memory type!");
}

void initVulkanBase(AppContext& app) {
    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, "gravsim", 1, "No Engine", 1, VK_API_VERSION_1_0};
    uint32_t glfwExtCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> exts(glfwExts, glfwExts + glfwExtCount);
#ifdef __APPLE__
    exts.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif
    VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = (uint32_t)exts.size();
    createInfo.ppEnabledExtensionNames = exts.data();
#ifdef __APPLE__
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATION_PORTABILITY_BIT_KHR;
#endif
    if (vkCreateInstance(&createInfo, nullptr, &app.instance) != VK_SUCCESS)
        throw std::runtime_error("failed to create instance!");

    if (glfwCreateWindowSurface(app.instance, app.window, nullptr, &app.surface) != VK_SUCCESS)
        throw std::runtime_error("failed to create surface!");

    uint32_t devCount = 0;
    vkEnumeratePhysicalDevices(app.instance, &devCount, nullptr);
    std::vector<VkPhysicalDevice> devices(devCount);
    vkEnumeratePhysicalDevices(app.instance, &devCount, devices.data());

    for (auto d : devices) {
        uint32_t qCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(d, &qCount, nullptr);
        std::vector<VkQueueFamilyProperties> qFamilies(qCount);
        vkGetPhysicalDeviceQueueFamilyProperties(d, &qCount, qFamilies.data());

        std::optional<uint32_t> g, c, p;
        for (uint32_t i = 0; i < qCount; i++) {
            if (qFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) g = i;
            if (qFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) c = i;
            VkBool32 ps = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(d, i, app.surface, &ps);
            if (ps) p = i;
            if (g.has_value() && c.has_value() && p.has_value()) {
                app.graphicsIdx = *g; app.computeIdx = *c; app.presentIdx = *p;
                app.physicalDevice = d;
                break;
            }
        }
        if (app.physicalDevice) break;
    }
    if (!app.physicalDevice) throw std::runtime_error("no suitable GPU!");

    std::set<uint32_t> uniqueQs = {app.graphicsIdx, app.computeIdx, app.presentIdx};
    std::vector<VkDeviceQueueCreateInfo> qInfos;
    float priority = 1.0f;
    for (uint32_t q : uniqueQs) {
        VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        qi.queueFamilyIndex = q;
        qi.queueCount = 1;
        qi.pQueuePriorities = &priority;
        qInfos.push_back(qi);
    }

    std::vector<const char*> devExts = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#ifdef __APPLE__
    devExts.push_back("VK_KHR_portability_subset");
#endif
    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo dInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dInfo.queueCreateInfoCount = (uint32_t)qInfos.size();
    dInfo.pQueueCreateInfos = qInfos.data();
    dInfo.pEnabledFeatures = &features;
    dInfo.enabledExtensionCount = (uint32_t)devExts.size();
    dInfo.ppEnabledExtensionNames = devExts.data();

    if (vkCreateDevice(app.physicalDevice, &dInfo, nullptr, &app.device) != VK_SUCCESS)
        throw std::runtime_error("failed to create device!");

    vkGetDeviceQueue(app.device, app.graphicsIdx, 0, &app.graphicsQueue);
    vkGetDeviceQueue(app.device, app.computeIdx, 0, &app.computeQueue);
    vkGetDeviceQueue(app.device, app.presentIdx, 0, &app.presentQueue);
}

void createSwapchain(AppContext& app) {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(app.physicalDevice, app.surface, &caps);
    uint32_t fCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(app.physicalDevice, app.surface, &fCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(app.physicalDevice, app.surface, &fCount, formats.data());

    VkSurfaceFormatKHR format = formats[0];
    for (auto f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            format = f; break;
        }
    }
    app.swapchainFormat = format.format;

    int w, h;
    glfwGetFramebufferSize(app.window, &w, &h);
    app.swapchainExtent = {std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, (uint32_t)w)),
                           std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, (uint32_t)h))};

    uint32_t imgCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imgCount > caps.maxImageCount) imgCount = caps.maxImageCount;

    VkSwapchainCreateInfoKHR scInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    scInfo.surface = app.surface;
    scInfo.minImageCount = imgCount;
    scInfo.imageFormat = app.swapchainFormat;
    scInfo.imageColorSpace = format.colorSpace;
    scInfo.imageExtent = app.swapchainExtent;
    scInfo.imageArrayLayers = 1;
    scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) scInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    uint32_t qIndices[] = {app.graphicsIdx, app.presentIdx};
    if (app.graphicsIdx != app.presentIdx) {
        scInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        scInfo.queueFamilyIndexCount = 2;
        scInfo.pQueueFamilyIndices = qIndices;
    } else {
        scInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    scInfo.preTransform = caps.currentTransform;
    scInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    scInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    scInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(app.device, &scInfo, nullptr, &app.swapchain) != VK_SUCCESS)
        throw std::runtime_error("failed to create swapchain!");

    vkGetSwapchainImagesKHR(app.device, app.swapchain, &imgCount, nullptr);
    app.swapchainImages.resize(imgCount);
    vkGetSwapchainImagesKHR(app.device, app.swapchain, &imgCount, app.swapchainImages.data());

    app.swapchainImageViews.resize(imgCount);
    for (size_t i = 0; i < imgCount; i++) {
        VkImageViewCreateInfo ivInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        ivInfo.image = app.swapchainImages[i];
        ivInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivInfo.format = app.swapchainFormat;
        ivInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        if (vkCreateImageView(app.device, &ivInfo, nullptr, &app.swapchainImageViews[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create image view!");
    }

    VkAttachmentDescription colorAtt{};
    colorAtt.format = app.swapchainFormat;
    colorAtt.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAtt.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &colorAtt;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dep;

    if (vkCreateRenderPass(app.device, &rpInfo, nullptr, &app.renderPass) != VK_SUCCESS)
        throw std::runtime_error("failed to create render pass!");

    app.framebuffers.resize(imgCount);
    for (size_t i = 0; i < imgCount; i++) {
        VkFramebufferCreateInfo fbInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fbInfo.renderPass = app.renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &app.swapchainImageViews[i];
        fbInfo.width = app.swapchainExtent.width;
        fbInfo.height = app.swapchainExtent.height;
        fbInfo.layers = 1;
        if (vkCreateFramebuffer(app.device, &fbInfo, nullptr, &app.framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create framebuffer!");
    }
    bloomCreateTargets(app);
}

void cleanupSwapchain(AppContext& app) {
    bloomDestroyTargets(app);
    for (auto fb : app.framebuffers) vkDestroyFramebuffer(app.device, fb, nullptr);
    app.framebuffers.clear();
    if (app.renderPass) { vkDestroyRenderPass(app.device, app.renderPass, nullptr); app.renderPass = VK_NULL_HANDLE; }
    for (auto iv : app.swapchainImageViews) vkDestroyImageView(app.device, iv, nullptr);
    app.swapchainImageViews.clear();
    if (app.swapchain) { vkDestroySwapchainKHR(app.device, app.swapchain, nullptr); app.swapchain = VK_NULL_HANDLE; }
}

VkShaderModule createSM(VkDevice dev, const std::vector<char>& code) {
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = code.size();
    ci.pCode = (const uint32_t*)code.data();
    VkShaderModule sm;
    if (vkCreateShaderModule(dev, &ci, nullptr, &sm) != VK_SUCCESS) throw std::runtime_error("failed shader!");
    return sm;
}

void createPipelines(AppContext& app) {
    VkDescriptorSetLayoutBinding bindings[2]{};
    for (int i = 0; i < 2; i++) {
        bindings[i] = { (uint32_t)i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_VERTEX_BIT, nullptr };
    }
    VkDescriptorSetLayoutCreateInfo dslInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 2, bindings};
    if (vkCreateDescriptorSetLayout(app.device, &dslInfo, nullptr, &app.computeDSL) != VK_SUCCESS)
        throw std::runtime_error("failed DSL!");

    VkPushConstantRange compPush{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SimParams)};
    VkPipelineLayoutCreateInfo compLInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &app.computeDSL, 1, &compPush};
    if (vkCreatePipelineLayout(app.device, &compLInfo, nullptr, &app.computeLayout) != VK_SUCCESS)
        throw std::runtime_error("failed comp layout!");

    auto compCode = readSpvFile(shaderPath("nbody.comp"));
    VkShaderModule compSM = createSM(app.device, compCode);
    VkComputePipelineCreateInfo compPInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    compPInfo.layout = app.computeLayout;
    compPInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, compSM, "main", nullptr};
    if (vkCreateComputePipelines(app.device, VK_NULL_HANDLE, 1, &compPInfo, nullptr, &app.computePipeline) != VK_SUCCESS)
        throw std::runtime_error("failed comp pipeline!");
    vkDestroyShaderModule(app.device, compSM, nullptr);

    VkPushConstantRange vertPush{VK_SHADER_STAGE_VERTEX_BIT, 0, 80};
    VkPipelineLayoutCreateInfo gfxLInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &vertPush};
    if (vkCreatePipelineLayout(app.device, &gfxLInfo, nullptr, &app.graphicsLayout) != VK_SUCCESS)
        throw std::runtime_error("failed gfx layout!");

    auto vertCode = readSpvFile(shaderPath("point.vert"));
    auto fragCode = readSpvFile(shaderPath("point.frag"));
    VkShaderModule vertSM = createSM(app.device, vertCode);
    VkShaderModule fragSM = createSM(app.device, fragCode);

    VkPipelineShaderStageCreateInfo stages[2]{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertSM, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragSM, "main", nullptr}
    };

    VkVertexInputBindingDescription bindDesc{0, 32, VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrDescs[2]{
        {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 16}
    };
    VkPipelineVertexInputStateCreateInfo vertInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0, 1, &bindDesc, 2, attrDescs};
    VkPipelineInputAssemblyStateCreateInfo inputAss{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0, VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_FALSE};
    VkPipelineViewportStateCreateInfo vpState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, nullptr, 1, nullptr};
    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_FALSE, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FALSE, 0, 0, 0, 1.0f};
    VkPipelineMultisampleStateCreateInfo multi{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, nullptr, 0, VK_SAMPLE_COUNT_1_BIT, VK_FALSE, 0, nullptr, VK_FALSE, VK_FALSE};
    
    VkPipelineColorBlendAttachmentState attBlend{};
    attBlend.colorWriteMask = 0xF;
    attBlend.blendEnable = VK_FALSE;
    attBlend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    attBlend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    attBlend.colorBlendOp = VK_BLEND_OP_ADD;
    attBlend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attBlend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    attBlend.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, nullptr, 0, VK_FALSE, VK_LOGIC_OP_COPY, 1, &attBlend, {0,0,0,0}};
    VkDynamicState dyns[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0, 2, dyns};

    VkGraphicsPipelineCreateInfo gfxPInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, stages, &vertInput, &inputAss, nullptr, &vpState, &raster, &multi, nullptr, &blend, &dyn, app.graphicsLayout, app.renderPass, 0, VK_NULL_HANDLE, -1};
    if (vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &gfxPInfo, nullptr, &app.graphicsPipeline) != VK_SUCCESS)
        throw std::runtime_error("failed gfx pipeline!");

    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
}
