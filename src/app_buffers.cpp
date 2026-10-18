#include "vk_app.h"
#include "particles.h"
#include <cstring>
#include <stdexcept>

static uint32_t findMemoryTypeLocal(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

void createBuffers(AppContext& app) {
    std::vector<Particle> particles = makeScene(app.scene, app.n, 7, app.trueScale);
    VkDeviceSize bufferSize = sizeof(Particle) * app.n;

    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    VkBufferCreateInfo stagingBufferInfo{};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = bufferSize;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(app.device, &stagingBufferInfo, nullptr, &stagingBuffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(app.device, stagingBuffer, &memRequirements);

    VkMemoryAllocateInfo stagingAllocInfo{};
    stagingAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAllocInfo.allocationSize = memRequirements.size;
    stagingAllocInfo.memoryTypeIndex = findMemoryTypeLocal(app.physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    vkAllocateMemory(app.device, &stagingAllocInfo, nullptr, &stagingBufferMemory);
    vkBindBufferMemory(app.device, stagingBuffer, stagingBufferMemory, 0);

    void* data;
    vkMapMemory(app.device, stagingBufferMemory, 0, bufferSize, 0, &data);
    std::memcpy(data, particles.data(), (size_t)bufferSize);
    vkUnmapMemory(app.device, stagingBufferMemory);

    // Create 2 DEVICE_LOCAL storage buffers
    for (int i = 0; i < 2; i++) {
        VkBufferCreateInfo localBufferInfo{};
        localBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        localBufferInfo.size = bufferSize;
        localBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        localBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        vkCreateBuffer(app.device, &localBufferInfo, nullptr, &app.particleBuffers[i]);

        vkGetBufferMemoryRequirements(app.device, app.particleBuffers[i], &memRequirements);

        VkMemoryAllocateInfo localAllocInfo{};
        localAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        localAllocInfo.allocationSize = memRequirements.size;
        localAllocInfo.memoryTypeIndex = findMemoryTypeLocal(app.physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        vkAllocateMemory(app.device, &localAllocInfo, nullptr, &app.particleMemories[i]);
        vkBindBufferMemory(app.device, app.particleBuffers[i], app.particleMemories[i], 0);
    }

    // Copy staging buffer contents into both DEVICE_LOCAL storage buffers via single-time command buffer execution
    VkCommandBufferAllocateInfo cmdAllocInfo{};
    cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandPool = app.commandPool;
    cmdAllocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(app.device, &cmdAllocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = bufferSize;

    vkCmdCopyBuffer(commandBuffer, stagingBuffer, app.particleBuffers[0], 1, &copyRegion);
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, app.particleBuffers[1], 1, &copyRegion);
    app.leapReset = true;

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(app.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(app.graphicsQueue);

    vkFreeCommandBuffers(app.device, app.commandPool, 1, &commandBuffer);

    // Destroy staging buffer
    vkDestroyBuffer(app.device, stagingBuffer, nullptr);
    vkFreeMemory(app.device, stagingBufferMemory, nullptr);

    // Create descriptor pool and allocate 2 descriptor sets using app.computeDSL
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSize.descriptorCount = 4;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 2;

    vkCreateDescriptorPool(app.device, &poolInfo, nullptr, &app.descriptorPool);

    VkDescriptorSetLayout layouts[] = { app.computeDSL, app.computeDSL };
    VkDescriptorSetAllocateInfo setAllocInfo{};
    setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    setAllocInfo.descriptorPool = app.descriptorPool;
    setAllocInfo.descriptorSetCount = 2;
    setAllocInfo.pSetLayouts = layouts;

    vkAllocateDescriptorSets(app.device, &setAllocInfo, app.descriptorSets);

    // Set 0 reads buffer 0 / writes buffer 1; Set 1 reads buffer 1 / writes buffer 0
    VkDescriptorBufferInfo bufferInfo00{ app.particleBuffers[0], 0, bufferSize };
    VkDescriptorBufferInfo bufferInfo01{ app.particleBuffers[1], 0, bufferSize };
    VkDescriptorBufferInfo bufferInfo10{ app.particleBuffers[1], 0, bufferSize };
    VkDescriptorBufferInfo bufferInfo11{ app.particleBuffers[0], 0, bufferSize };

    VkWriteDescriptorSet descriptorWrites[4]{};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = app.descriptorSets[0];
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfo00;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = app.descriptorSets[0];
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &bufferInfo01;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = app.descriptorSets[1];
    descriptorWrites[2].dstBinding = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &bufferInfo10;

    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = app.descriptorSets[1];
    descriptorWrites[3].dstBinding = 1;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pBufferInfo = &bufferInfo11;

    vkUpdateDescriptorSets(app.device, 4, descriptorWrites, 0, nullptr);
}

void cleanupBuffers(AppContext& app) {
    for (int i = 0; i < 2; i++) {
        if (app.particleBuffers[i] != VK_NULL_HANDLE) {
            vkDestroyBuffer(app.device, app.particleBuffers[i], nullptr);
            app.particleBuffers[i] = VK_NULL_HANDLE;
        }
        if (app.particleMemories[i] != VK_NULL_HANDLE) {
            vkFreeMemory(app.device, app.particleMemories[i], nullptr);
            app.particleMemories[i] = VK_NULL_HANDLE;
        }
    }
    if (app.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(app.device, app.descriptorPool, nullptr);
        app.descriptorPool = VK_NULL_HANDLE;
    }
}

void createCommands(AppContext& app) {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = app.graphicsIdx;
    if (vkCreateCommandPool(app.device, &poolInfo, nullptr, &app.commandPool) != VK_SUCCESS)
        throw std::runtime_error("failed to create command pool");

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = app.commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 2;
    if (vkAllocateCommandBuffers(app.device, &allocInfo, app.commandBuffers) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate command buffers");

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (int i = 0; i < 2; i++) {
        if (vkCreateSemaphore(app.device, &semInfo, nullptr, &app.imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(app.device, &semInfo, nullptr, &app.renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(app.device, &fenceInfo, nullptr, &app.inFlightFences[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create sync objects");
    }
}
