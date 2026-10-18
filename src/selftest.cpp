#include "gpu.h"
#include "particles.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstdint>

#define VK_CHECK(expr) { \
    VkResult err = (expr); \
    if (err != VK_SUCCESS) { \
        std::cerr << "Vulkan call failed: " << #expr << " (error code " << err << ")" << std::endl; \
    } \
}

static uint32_t findMemType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    return 0;
}

int runSelfTest() {
    const uint32_t n = 1024;
    SimParams params{0.01f, 1.0f, 0.5f, n};
    params.prevDt = 0.005f;
    const VkDeviceSize bufSize = n * sizeof(Particle);

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, "Selftest", 0, "Engine", 0, VK_API_VERSION_1_0};
    VkInstanceCreateInfo instInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, 0, &appInfo, 0, nullptr, 0, nullptr};
    VkInstance instance = VK_NULL_HANDLE;
    VK_CHECK(vkCreateInstance(&instInfo, nullptr, &instance));

    uint32_t gpuCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
    if (gpuCount == 0) return 1;
    std::vector<VkPhysicalDevice> gpus(gpuCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &gpuCount, gpus.data()));
    VkPhysicalDevice phys = gpus[0];

    uint32_t qCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &qCount, nullptr);
    std::vector<VkQueueFamilyProperties> qProps(qCount);
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &qCount, qProps.data());

    uint32_t computeIdx = UINT32_MAX;
    for (uint32_t i = 0; i < qCount; ++i) {
        if (qProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            computeIdx = i;
            break;
        }
    }
    if (computeIdx == UINT32_MAX) return 1;

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo qCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, computeIdx, 1, &queuePriority};
    VkDeviceCreateInfo devInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, nullptr, 0, 1, &qCreateInfo, 0, nullptr, 0, nullptr, nullptr};
    VkDevice device = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDevice(phys, &devInfo, nullptr, &device));

    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, computeIdx, 0, &queue);

    auto initialParticles = makeGalaxy(n, 7);
    VkBuffer buffer[2];
    VkDeviceMemory memory[2];
    for (int i = 0; i < 2; ++i) {
        VkBufferCreateInfo bInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, bufSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE, 0, nullptr};
        VK_CHECK(vkCreateBuffer(device, &bInfo, nullptr, &buffer[i]));
        VkMemoryRequirements memReq;
        vkGetBufferMemoryRequirements(device, buffer[i], &memReq);
        VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, memReq.size, findMemType(phys, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)};
        VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &memory[i]));
        VK_CHECK(vkBindBufferMemory(device, buffer[i], memory[i], 0));

        void* mapped = nullptr;
        VK_CHECK(vkMapMemory(device, memory[i], 0, bufSize, 0, &mapped));
        std::memcpy(mapped, initialParticles.data(), bufSize);
        vkUnmapMemory(device, memory[i]);
    }

    std::string spvPath = shaderPath("nbody.comp");
    std::ifstream file(spvPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << spvPath << std::endl;
        return 1;
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<uint32_t> code(fileSize / sizeof(uint32_t));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(code.data()), fileSize);
    file.close();

    VkShaderModuleCreateInfo smInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0, fileSize, code.data()};
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device, &smInfo, nullptr, &shaderModule));

    VkDescriptorSetLayoutBinding bindings[2]{};
    bindings[0] = {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo dslInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 2, bindings};
    VkDescriptorSetLayout descLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &dslInfo, nullptr, &descLayout));

    VkPushConstantRange pcRange{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SimParams)};
    VkPipelineLayoutCreateInfo plInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &descLayout, 1, &pcRange};
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device, &plInfo, nullptr, &pipelineLayout));

    VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, nullptr, 0,
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, shaderModule, "main", nullptr},
        pipelineLayout, VK_NULL_HANDLE, 0};
    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline));

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0, 2, 1, &poolSize};
    VkDescriptorPool descPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descPool));

    VkDescriptorSetLayout layouts[2] = {descLayout, descLayout};
    VkDescriptorSetAllocateInfo dsAlloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, descPool, 2, layouts};
    VkDescriptorSet descSets[2];
    VK_CHECK(vkAllocateDescriptorSets(device, &dsAlloc, descSets));

    VkDescriptorBufferInfo bufInfo[2][2] = {
        { {buffer[0], 0, bufSize}, {buffer[1], 0, bufSize} },
        { {buffer[1], 0, bufSize}, {buffer[0], 0, bufSize} }
    };
    VkWriteDescriptorSet writes[4]{};
    for (int s = 0; s < 2; ++s) {
        for (int b = 0; b < 2; ++b) {
            writes[s * 2 + b] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descSets[s], (uint32_t)b, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bufInfo[s][b], nullptr};
        }
    }
    vkUpdateDescriptorSets(device, 4, writes, 0, nullptr);

    VkCommandPoolCreateInfo cpInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, 0, computeIdx};
    VkCommandPool cmdPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateCommandPool(device, &cpInfo, nullptr, &cmdPool));

    VkCommandBufferAllocateInfo cbAlloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, nullptr, cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1};
    VkCommandBuffer cmdBuf = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device, &cbAlloc, &cmdBuf));

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr};
    VK_CHECK(vkBeginCommandBuffer(cmdBuf, &beginInfo));
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SimParams), &params);

    for (int step = 0; step < 10; ++step) {
        if (step > 0) {
            VkMemoryBarrier mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT};
            vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &mb, 0, nullptr, 0, nullptr);
        }
        vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descSets[step % 2], 0, nullptr);
        vkCmdDispatch(cmdBuf, (n + 255) / 256, 1, 1);
    }
    VK_CHECK(vkEndCommandBuffer(cmdBuf));

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 0, nullptr, nullptr, 1, &cmdBuf, 0, nullptr};
    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(queue));

    auto cpuParticles = initialParticles;
    for (int step = 0; step < 10; ++step) {
        cpuStep(cpuParticles, params);
    }

    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(device, memory[0], 0, bufSize, 0, &mapped));
    const Particle* gpuParticles = static_cast<const Particle*>(mapped);

    float maxDiff = 0.0f;
    for (uint32_t i = 0; i < n; ++i) {
        for (int c = 0; c < 3; ++c) {
            float diff = std::abs(gpuParticles[i].pos[c] - cpuParticles[i].pos[c]);
            if (diff > maxDiff) maxDiff = diff;
        }
    }
    vkUnmapMemory(device, memory[0]);

    std::cout << "Max abs difference: " << maxDiff << std::endl;
    int result = 0;
    if (maxDiff < 1e-2f) {
        std::cout << "SELFTEST PASS" << std::endl;
        result = 0;
    } else {
        std::cout << "SELFTEST FAIL" << std::endl;
        result = 1;
    }

    vkDestroyShaderModule(device, shaderModule, nullptr);
    vkDestroyPipeline(device, pipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyDescriptorPool(device, descPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descLayout, nullptr);
    vkDestroyCommandPool(device, cmdPool, nullptr);
    for (int i = 0; i < 2; ++i) {
        vkDestroyBuffer(device, buffer[i], nullptr);
        vkFreeMemory(device, memory[i], nullptr);
    }
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);

    return result;
}
