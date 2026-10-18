#include "vk_app.h"
#include "universe.h"
#include "expansion.h"
#include "bhdisc.h"
#include "ephem.h"
#include "solarx.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags properties);

void applyScale(AppContext& app) {
    app.simG = app.scene != Scene::SOLAR ? 1.0f : solarG(app.trueScale);
    app.simSoft = app.scene != Scene::SOLAR ? 0.5f : solarSoft(app.trueScale);
}

void resetSystem(AppContext& app) {
    vkDeviceWaitIdle(app.device);
    solarxReset(app);
    solarSetEpochDay(app.tm.launchDay);
    applyScale(app);
    std::vector<Particle> particles = app.scene == Scene::SOLAR && app.visit.active
        ? buildSystem(app.visit.sys, app.n, app.trueScale) : makeScene(app.scene, app.n, 7, app.trueScale);
    VkDeviceSize size = sizeof(Particle) * app.n;

    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = size;
    info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkBuffer staging;
    if (vkCreateBuffer(app.device, &info, nullptr, &staging) != VK_SUCCESS) throw std::runtime_error("failed reset buffer!");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(app.device, staging, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkDeviceMemory memory;
    if (vkAllocateMemory(app.device, &alloc, nullptr, &memory) != VK_SUCCESS) throw std::runtime_error("failed reset memory!");
    vkBindBufferMemory(app.device, staging, memory, 0);
    void* data;
    vkMapMemory(app.device, memory, 0, size, 0, &data);
    std::memcpy(data, particles.data(), (size_t)size);
    vkUnmapMemory(app.device, memory);

    VkCommandBufferAllocateInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdInfo.commandPool = app.commandPool;
    cmdInfo.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(app.device, &cmdInfo, &cmd);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin);
    VkBufferCopy region{0, 0, size};
    vkCmdCopyBuffer(cmd, staging, app.particleBuffers[0], 1, &region);
    vkCmdCopyBuffer(cmd, staging, app.particleBuffers[1], 1, &region);
    app.leapReset = true;
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(app.graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(app.graphicsQueue);
    vkFreeCommandBuffers(app.device, app.commandPool, 1, &cmd);
    vkDestroyBuffer(app.device, staging, nullptr);
    vkFreeMemory(app.device, memory, nullptr);

    for (auto& trail : app.trails) trail.clear();
    throwReset(app, particles);
    app.readValid[0] = app.readValid[1] = false;
    app.simTime = 0.0;
    app.displayValid = false;
    app.follow = -1;
    app.camera = Camera{};
    if (collisionLike(app.scene)) {
        const bool andromeda = app.scene == Scene::ANDROMEDA;
        const float dist = andromeda ? 250.0f : 120.0f;
        app.camera.pos[0] = dist * (andromeda ? 0.12f : 0.52f);
        app.camera.pos[1] = dist * (andromeda ? 0.42f : 0.39f);
        app.camera.pos[2] = dist * (andromeda ? 0.9f : 0.76f);
    }
    andromedaFrameReset(app);
    if (app.scene == Scene::BHSIZES) bhResetCamera(app);
    if (app.scene == Scene::LIFECYCLE) lifeRestart(app);
    if (app.scene == Scene::STARSIZES) {
        app.showSky = false;
        starsRestart(app);
    }
    if (app.scene == Scene::SANDBOX) sbEnter(app);
    if (app.scene == Scene::EARTHMOON) emEnter(app);
    if (app.scene == Scene::TIMEDIL) tdEnter(app);
    if (app.scene == Scene::LAGRANGE) lgEnter(app);
    if (app.scene == Scene::LIGHTRIDE) lrEnter(app);
    if (app.scene == Scene::FUTURESKY) fsEnter(app);
    if (app.scene == Scene::NIGHTSKY) nsEnter(app);
    if (app.scene == Scene::IMPACT) imEnter(app);
    if (app.scene == Scene::ECLIPSE) ecEnter(app);
    if (app.scene == Scene::EXPANSION) exEnter(app);
    if (app.scene == Scene::BHDISC) bdEnter(app);
    visitSync(app);
    visitAfterReset(app);
}

void switchScene(AppContext& app, Scene scene) {
    app.showSky = scene != Scene::SOLAR;
    app.showGalaxy = scene == Scene::SOLAR;
    app.scene = scene;
    galVisitReset(app);
    visitClear(app);
    resetSystem(app);
}

void enterFree(AppContext& app) {
    float eye[3];
    cameraEye(app.camera, eye);
    for (int a = 0; a < 3; a++) app.camera.pos[a] = eye[a];
    app.camera.free = true;
    app.follow = -1;
}

void enterFollow(AppContext& app, int body, const float p[3]) {
    float eye[3];
    cameraEye(app.camera, eye);
    float d[3] = {eye[0] - p[0], eye[1] - p[1], eye[2] - p[2]};
    float len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len < 1e-12f) {
        d[0] = 0.0f; d[1] = 0.0f; d[2] = 1.0f;
        len = 1e-6f;
    }
    Camera& c = app.camera;
    c.yaw = std::atan2(d[0], d[2]);
    c.pitch = std::max(-1.5f, std::min(1.5f, std::asin(std::max(-1.0f, std::min(1.0f, d[1] / len)))));
    for (int a = 0; a < 3; a++) c.target[a] = p[a];
    c.minDist = std::max(app.trueScale ? 0.0f : 1.5f, 1.3f * bodyRadius(app.trueScale, body));
    c.dist = std::max(c.minDist, std::min(len, followDistance(app.trueScale, body)));
    c.free = false;
    app.follow = body;
}

void updateCamera(AppContext& app, float dt) {
    if (closeupStep(app, dt)) return;
    if (app.scene == Scene::LIFECYCLE) {
        lifeUpdateCamera(app, dt);
        return;
    }
    if (app.scene == Scene::BHSIZES) {
        bhUpdateCamera(app, dt);
        return;
    }
    if (app.scene == Scene::STARSIZES) {
        starsUpdate(app, dt);
        return;
    }
    if (app.scene == Scene::SANDBOX) return;
    if (app.scene == Scene::EARTHMOON) {
        emUpdate(app, dt);
        return;
    }
    if (app.scene == Scene::TIMEDIL) {
        tdUpdate(app, dt);
        return;
    }
    if (app.scene == Scene::LAGRANGE || app.scene == Scene::LIGHTRIDE || app.scene == Scene::FUTURESKY || app.scene == Scene::NIGHTSKY || app.scene == Scene::IMPACT || app.scene == Scene::ECLIPSE || app.scene == Scene::EXPANSION || app.scene == Scene::BHDISC) return;
    Camera& c = app.camera;
    const Particle* bodies;
    uint32_t count;
    float surface = 10.0f;
    if (currentBodies(app, bodies, count)) {
        float eye[3];
        cameraEye(c, eye);
        float bestSurface = 1e30f, bestCentre = 1e30f;
        for (uint32_t i = 0; i < count; i++) {
            float p[3];
            bodyPosition(app, bodies, i, p);
            float dx = p[0] - eye[0], dy = p[1] - eye[1], dz = p[2] - eye[2];
            float centre = std::sqrt(dx * dx + dy * dy + dz * dz);
            bestCentre = std::min(bestCentre, centre);
            bestSurface = std::min(bestSurface, centre - bodyRadius(app.trueScale, (int)i));
        }
        surface = std::max(bestSurface, 1e-7f);
        app.refDist = std::max(bestCentre, 1e-7f);
        c.nearPlane = std::max(1e-7f, std::min(0.1f, 0.1f * surface));
    } else {
        c.nearPlane = 0.1f;
    }
    if (app.visit.mode != 0) {
        visitStep(app, dt);
        return;
    }
    float zoomDir = app.showHelp ? 0.0f : (glfwGetKey(app.window, GLFW_KEY_PAGE_UP) == GLFW_PRESS ? 1.0f : 0.0f)
        - (glfwGetKey(app.window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS ? 1.0f : 0.0f);
    float wheel = c.zoomImpulse;
    c.zoomImpulse = 0.0f;
    if (wheel != 0.0f && app.scene != Scene::SOLAR) {
        c.moveMult = std::min(50.0f, std::max(0.02f, c.moveMult * std::pow(1.25f, wheel)));
        wheel = 0.0f;
    }
    float cdt = std::min(dt, 0.1f);
    app.zoomDt = app.zoomDt > 0.0f ? app.zoomDt + (cdt - app.zoomDt) * 0.08f : cdt;
    float sdt = app.zoomDt;
    app.zoomWheel += wheel;
    float wheelStep = 0.0f;
    if (std::fabs(app.zoomWheel) > 1e-3) {
        wheelStep = (float)(app.zoomWheel * (1.0 - std::exp(-16.0 * (double)sdt)));
        app.zoomWheel -= wheelStep;
    } else {
        app.zoomWheel = 0.0;
    }
    if ((zoomDir != 0.0f || wheelStep != 0.0f) && app.scene == Scene::SOLAR) {
        if (!c.free) enterFree(app);
        float sun[3];
        sunPosition(app, sun);
        double rel[3] = {(double)c.pos[0] - sun[0], (double)c.pos[1] - sun[1], (double)c.pos[2] - sun[2]};
        if (app.zoomValid) {
            bool same = true;
            double cand[3];
            for (int a = 0; a < 3; a++) {
                cand[a] = (double)app.zoomSun[a] + app.zoomRel[a];
                same = same && (float)cand[a] == c.pos[a];
            }
            if (same) for (int a = 0; a < 3; a++) rel[a] = cand[a] - sun[a];
        }
        double d = std::sqrt(rel[0] * rel[0] + rel[1] * rel[1] + rel[2] * rel[2]);
        double maxD = (app.showUniverse ? (double)UNIVERSE_MAX_MLY * 1e6 : 150000.0) / LY_PER_AU_INV * solarUnitsPerAu(app.trueScale);
        double rate = 5.0 * (glfwGetKey(app.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 2.0 : 1.0);
        double nd = std::min(maxD, std::max(3.0, d * std::exp((double)zoomDir * rate * sdt - 0.35 * wheelStep)));
        if (d < 1e-6) { rel[0] = 0.0; rel[1] = 0.0; rel[2] = 1.0; d = 1.0; }
        for (int a = 0; a < 3; a++) {
            app.zoomRel[a] = rel[a] / d * nd;
            app.zoomSun[a] = sun[a];
            c.pos[a] = (float)((double)sun[a] + app.zoomRel[a]);
        }
        app.zoomValid = true;
        if (nd > 200.0) {
            float blend = 1.0f - std::exp(-4.0f * sdt);
            float wantYaw = (float)std::atan2(rel[0], rel[2]);
            float wantPitch = (float)std::asin(std::max(-1.0, std::min(1.0, rel[1] / d)));
            float dy = wantYaw - c.yaw;
            dy -= 6.2831853f * std::floor((dy + 3.14159265f) / 6.2831853f);
            c.yaw += dy * blend;
            c.pitch += (std::max(-1.5f, std::min(1.5f, wantPitch)) - c.pitch) * blend;
        }
    }
    if (!c.free) return;

    GLFWwindow* w = app.window;
    auto down = [w](int key) { return glfwGetKey(w, key) == GLFW_PRESS ? 1.0f : 0.0f; };
    float forward = down(GLFW_KEY_W) - down(GLFW_KEY_S);
    float strafe = down(GLFW_KEY_D) - down(GLFW_KEY_A);
    float lift = down(GLFW_KEY_E) - down(GLFW_KEY_Q);
    float boost = down(GLFW_KEY_LEFT_SHIFT) > 0.0f ? 4.0f : 1.0f;
    float speed = std::max(std::min(80.0f, surface * 0.8f), surface * 0.08f) * c.moveMult * boost;
    float cp = std::cos(c.pitch), sp = std::sin(c.pitch), sy = std::sin(c.yaw), cy = std::cos(c.yaw);
    float f[3] = {-cp * sy, -sp, -cp * cy};
    float r[3] = {cy, 0.0f, -sy};
    float step = speed * std::min(dt, 0.1f);
    for (int a = 0; a < 3; a++) c.pos[a] += (f[a] * forward + r[a] * strafe) * step;
    c.pos[1] += lift * step;
}
