#include "vk_app.h"
#include "gpu.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags properties);

float g_tdRadius = 0.0f;
bool g_tdMiller = false;

void createTimeDilation(AppContext& app) {
    TimeDilState& S = app.td;
    VkBufferCreateInfo binfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    binfo.size = sizeof(Particle);
    binfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    binfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(app.device, &binfo, nullptr, &S.buf) != VK_SUCCESS)
        throw std::runtime_error("failed time dilation buffer!");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(app.device, S.buf, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(app.device, &alloc, nullptr, &S.mem) != VK_SUCCESS)
        throw std::runtime_error("failed time dilation memory!");
    vkBindBufferMemory(app.device, S.buf, S.mem, 0);
    Particle p{};
    p.vel[1] = 1.0f;
    p.pos[3] = 3.7e10f;
    void* data;
    vkMapMemory(app.device, S.mem, 0, sizeof(Particle), 0, &data);
    std::memcpy(data, &p, sizeof(Particle));
    vkUnmapMemory(app.device, S.mem);
}

void destroyTimeDilation(AppContext& app) {
    vkDestroyBuffer(app.device, app.td.buf, nullptr);
    vkFreeMemory(app.device, app.td.mem, nullptr);
}

void tdEnter(AppContext& app) {
    TimeDilState& S = app.td;
    S.ready = true;
    S.miller = g_tdMiller;
    S.x = g_tdRadius > 1.0f ? std::clamp(td::xOf(g_tdRadius), td::X_MIN, td::X_MAX) : td::X_MAX;
    S.rapidity = 0.0f;
    S.tau = 0.0;
    S.far = 0.0;
    S.clock = 0.0f;
    Camera& c = app.camera;
    c = Camera{};
    c.free = false;
    c.yaw = 0.0f;
    c.pitch = 0.2f;
    c.dist = td::DIST;
    c.minDist = 1.0f;
    c.nearPlane = 0.1f;
    c.target[0] = c.target[1] = c.target[2] = 0.0f;
}

void tdRestart(AppContext& app) {
    g_tdRadius = 0.0f;
    g_tdMiller = false;
    tdEnter(app);
}

void tdUpdate(AppContext& app, float dt) {
    TimeDilState& S = app.td;
    Camera& c = app.camera;
    c.zoomImpulse = 0.0f;
    dt = std::min(dt, 0.1f);
    S.clock += dt;
    float notches = std::log(c.dist / td::DIST) / std::log(0.9f);
    c.dist = td::DIST;
    float move = -notches * 0.25f;
    GLFWwindow* w = app.window;
    if (glfwGetKey(w, GLFW_KEY_UP) == GLFW_PRESS) move -= 2.0f * dt;
    if (glfwGetKey(w, GLFW_KEY_DOWN) == GLFW_PRESS) move += 2.0f * dt;
    if (std::fabs(move) > 1e-5f) {
        S.miller = false;
        S.x = std::clamp(S.x + move, td::X_MIN, td::X_MAX);
    }
    float dv = 0.0f;
    if (glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS) dv += 1.2f * dt;
    if (glfwGetKey(w, GLFW_KEY_LEFT) == GLFW_PRESS) dv -= 1.2f * dt;
    S.rapidity = std::clamp(S.rapidity + dv, 0.0f, td::RAPID_MAX);
    static bool millerDown = false;
    bool key = glfwGetKey(w, GLFW_KEY_I) == GLFW_PRESS;
    if (key && !millerDown) {
        S.miller = !S.miller;
        if (S.miller) S.rapidity = 0.0f;
    }
    millerDown = key;
    if (!S.paused) {
        double step = (double)dt * td::SECS_PER_SEC * S.speed;
        S.tau += step;
        S.far += step * (double)td::totalFactor(S);
    }
}

void drawTimeDilation(AppContext& app, VkCommandBuffer cmd, float aspect) {
    float vp[16];
    cameraViewProj(app.camera, aspect, vp);
    if (app.showSky) drawSky(app, cmd, vp);
    drawHoleCustom(app, cmd, app.td.buf, 1, vp, app.camera, td::HOLE_R, 1.0f);
}
