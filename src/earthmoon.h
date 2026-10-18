#pragma once
#include <vulkan/vulkan.h>

struct EarthMoonState {
    bool ready = false;
    bool paused = false;
    float speed = 1.0f;
    float days = 0.0f;
    float clock = 0.0f;
    int highCount = 0;
    int tideState = 0;
    bool wasHigh = false;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};
