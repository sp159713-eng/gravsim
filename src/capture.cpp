#include "capture.h"
#include "capture_io.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace {

constexpr int REC_FPS = 30;
constexpr double REC_MAX = 60.0;
constexpr uint32_t HUD_VERTS = 12000;

struct Slot {
    VkBuffer buf = VK_NULL_HANDLE;
    VkDeviceMemory mem = VK_NULL_HANDLE;
    void* map = nullptr;
    VkDeviceSize cap = 0;
    bool coherent = true;
    bool pending = false;
    int kind = 0;
    int w = 0, h = 0, index = 0;
    std::string path, dir;
    VkBuffer hudBuf = VK_NULL_HANDLE;
    VkDeviceMemory hudMem = VK_NULL_HANDLE;
    void* hudMap = nullptr;
};

Slot g_slot[2];
bool g_ok = false;
bool g_bgr = true;
bool g_capNow = false;
bool g_f12Down = false;
bool g_shotReq = false;
bool g_recording = false;
bool g_finishWait = false;
double g_recStart = 0.0;
int g_recW = 0, g_recH = 0;
int g_lastIdx = -1;
std::string g_recDir, g_recMp4;
double g_shotAt = -1.0, g_recAt = -1.0, g_recFor = 2.0, g_recAutoStop = -1.0;
double g_t0 = 0.0;
bool g_started = false;

uint32_t memType(VkPhysicalDevice pd, uint32_t bits, VkMemoryPropertyFlags want) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(pd, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
    return UINT32_MAX;
}

bool makeHost(AppContext& app, VkDeviceSize size, VkBufferUsageFlags usage, bool cached, VkBuffer& buf, VkDeviceMemory& mem, void*& map, bool& coherent) {
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(app.device, &bi, nullptr, &buf) != VK_SUCCESS) return false;
    VkMemoryRequirements mr;
    vkGetBufferMemoryRequirements(app.device, buf, &mr);
    VkMemoryPropertyFlags vis = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    VkMemoryPropertyFlags coh = vis | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    uint32_t t = UINT32_MAX;
    coherent = true;
    if (cached) {
        t = memType(app.physicalDevice, mr.memoryTypeBits, coh | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
        if (t == UINT32_MAX) {
            t = memType(app.physicalDevice, mr.memoryTypeBits, vis | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);
            coherent = false;
        }
    }
    if (t == UINT32_MAX) {
        t = memType(app.physicalDevice, mr.memoryTypeBits, coh);
        coherent = true;
    }
    if (t == UINT32_MAX) return false;
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = t;
    if (vkAllocateMemory(app.device, &ai, nullptr, &mem) != VK_SUCCESS) return false;
    vkBindBufferMemory(app.device, buf, mem, 0);
    return vkMapMemory(app.device, mem, 0, VK_WHOLE_SIZE, 0, &map) == VK_SUCCESS;
}

void freeBuf(AppContext& app, VkBuffer& b, VkDeviceMemory& m, void*& map) {
    if (map) vkUnmapMemory(app.device, m);
    if (b) vkDestroyBuffer(app.device, b, nullptr);
    if (m) vkFreeMemory(app.device, m, nullptr);
    b = VK_NULL_HANDLE;
    m = VK_NULL_HANDLE;
    map = nullptr;
}

bool ensureStaging(AppContext& app, Slot& s, VkDeviceSize need) {
    if (s.buf && s.cap >= need) return true;
    freeBuf(app, s.buf, s.mem, s.map);
    s.cap = 0;
    if (!makeHost(app, need, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true, s.buf, s.mem, s.map, s.coherent)) {
        freeBuf(app, s.buf, s.mem, s.map);
        return false;
    }
    s.cap = need;
    return true;
}

void harvest(AppContext& app, Slot& s) {
    if (!s.pending) return;
    s.pending = false;
    size_t bytes = (size_t)s.w * s.h * 4;
    if (!s.coherent) {
        VkMappedMemoryRange r{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        r.memory = s.mem;
        r.offset = 0;
        r.size = VK_WHOLE_SIZE;
        vkInvalidateMappedMemoryRanges(app.device, 1, &r);
    }
    CapJob j;
    j.kind = s.kind;
    j.px.resize(bytes);
    std::memcpy(j.px.data(), s.map, bytes);
    j.w = s.w;
    j.h = s.h;
    j.bgr = g_bgr;
    j.path = s.path;
    j.index = s.index;
    j.dir = s.dir;
    capIoPush(std::move(j));
}

std::string frameName(int idx) {
    char b[32];
    std::snprintf(b, sizeof(b), "/frame_%05d.png", idx);
    return b;
}

void startRec(AppContext& app) {
    std::string ts = capTimestamp();
    std::string base = capPicturesDir();
    g_recDir = base + "/gravsim_" + ts;
    g_recMp4 = base + "/gravsim_" + ts + ".mp4";
    std::error_code ec;
    std::filesystem::create_directories(g_recDir, ec);
    g_recW = (int)app.swapchainExtent.width;
    g_recH = (int)app.swapchainExtent.height;
    g_recStart = capClock();
    g_lastIdx = -1;
    g_recording = true;
    CapJob j;
    j.kind = 2;
    capIoPush(std::move(j));
}

void stopRec() {
    if (!g_recording) return;
    g_recording = false;
    g_finishWait = true;
}

void pushFinish() {
    g_finishWait = false;
    if (g_lastIdx < 0) {
        capToastSet("RECORDING EMPTY");
        return;
    }
    CapJob j;
    j.kind = 3;
    j.dir = g_recDir;
    j.mp4 = g_recMp4;
    j.index = g_lastIdx;
    capIoPush(std::move(j));
}

void hudRect(OverlayVertex* o, uint32_t& n, float W, float H, float x0, float y0, float x1, float y1, const float c[3]) {
    float a = -1.0f + 2.0f * x0 / W, b = -1.0f + 2.0f * y0 / H, e = -1.0f + 2.0f * x1 / W, f = -1.0f + 2.0f * y1 / H;
    o[n++] = {a, b, c[0], c[1], c[2]};
    o[n++] = {e, b, c[0], c[1], c[2]};
    o[n++] = {e, f, c[0], c[1], c[2]};
    o[n++] = {a, b, c[0], c[1], c[2]};
    o[n++] = {e, f, c[0], c[1], c[2]};
    o[n++] = {a, f, c[0], c[1], c[2]};
}

void hudText(OverlayVertex* o, uint32_t& n, float W, float H, const char* s, float x, float y, int ts, const float c[3]) {
    if (n + 1500 > HUD_VERTS) return;
    emitText(o, n, s, -1.0f + 2.0f * x / W, -1.0f + 2.0f * y / H, 2.0f / W, 2.0f / H, (float)ts, c[0], c[1], c[2]);
}

}

float g_capShotAt = -1.0f;
float g_capRecAt = -1.0f;
float g_capRecFor = 2.0f;

void captureInit(AppContext& app) {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(app.physicalDevice, app.surface, &caps);
    VkFormat f = app.swapchainFormat;
    bool bgr = f == VK_FORMAT_B8G8R8A8_SRGB || f == VK_FORMAT_B8G8R8A8_UNORM;
    bool rgb = f == VK_FORMAT_R8G8B8A8_SRGB || f == VK_FORMAT_R8G8B8A8_UNORM;
    g_bgr = bgr;
    g_ok = (bgr || rgb) && (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    for (Slot& s : g_slot) {
        bool c;
        makeHost(app, HUD_VERTS * sizeof(OverlayVertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, false, s.hudBuf, s.hudMem, s.hudMap, c);
    }
    g_shotAt = g_capShotAt;
    g_recAt = g_capRecAt;
    if (g_capRecFor > 0.0f) g_recFor = g_capRecFor;
    g_t0 = capClock();
    g_started = true;
    capIoStart();
}

void captureKeys(AppContext& app) {
    bool f12 = glfwGetKey(app.window, GLFW_KEY_F12) == GLFW_PRESS;
    bool shift = glfwGetKey(app.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(app.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    if (f12 && !g_f12Down) {
        if (!g_ok) capToastSet("CAPTURE NOT SUPPORTED ON THIS GPU");
        else if (shift) {
            if (g_recording) stopRec();
            else startRec(app);
        } else g_shotReq = true;
    }
    g_f12Down = f12;
}

bool captureBeginFrame(AppContext& app) {
    g_capNow = false;
    if (!g_started) return false;
    Slot& s = g_slot[app.currentFrame];
    harvest(app, s);
    double now = capClock();
    if (g_ok) {
        if (g_shotAt >= 0.0 && now - g_t0 >= g_shotAt) {
            g_shotAt = -1.0;
            g_shotReq = true;
        }
        if (g_recAt >= 0.0 && now - g_t0 >= g_recAt) {
            g_recAt = -1.0;
            if (!g_recording) {
                startRec(app);
                g_recAutoStop = now + g_recFor;
            }
        }
        if (g_recording && ((g_recAutoStop >= 0.0 && now >= g_recAutoStop) || now - g_recStart >= REC_MAX)) {
            g_recAutoStop = -1.0;
            stopRec();
        }
    }
    if (g_finishWait && !g_slot[0].pending && !g_slot[1].pending) pushFinish();
    if (!g_ok || s.pending) return false;
    int W = (int)app.swapchainExtent.width, H = (int)app.swapchainExtent.height;
    int kind = -1, idx = 0;
    std::string path, dir;
    if (g_shotReq) {
        g_shotReq = false;
        kind = 0;
        path = capPicturesDir() + "/gravsim_" + capTimestamp() + ".png";
    } else if (g_recording && W == g_recW && H == g_recH && capIoBacklog() < 8) {
        idx = (int)std::floor((now - g_recStart) * REC_FPS);
        if (idx > g_lastIdx) {
            kind = 1;
            dir = g_recDir;
            path = g_recDir + frameName(idx);
        }
    }
    if (kind < 0) return false;
    if (!ensureStaging(app, s, (VkDeviceSize)W * H * 4)) {
        capToastSet("CAPTURE OUT OF MEMORY");
        return false;
    }
    s.kind = kind;
    s.w = W;
    s.h = H;
    s.index = idx;
    s.path = path;
    s.dir = dir;
    if (kind == 1) g_lastIdx = idx;
    g_capNow = true;
    return true;
}

void captureRecordCopy(AppContext& app, VkCommandBuffer cmd, uint32_t imageIndex) {
    if (!g_capNow) return;
    Slot& s = g_slot[app.currentFrame];
    VkImage img = app.swapchainImages[imageIndex];
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    b.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
    VkBufferImageCopy rg{};
    rg.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    rg.imageExtent = {(uint32_t)s.w, (uint32_t)s.h, 1};
    vkCmdCopyImageToBuffer(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, s.buf, 1, &rg);
    b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    b.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    b.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    b.dstAccessMask = 0;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &b);
    VkBufferMemoryBarrier hb{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    hb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    hb.buffer = s.buf;
    hb.size = VK_WHOLE_SIZE;
    hb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    hb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1, &hb, 0, nullptr);
    s.pending = true;
}

void captureDraw(AppContext& app, VkCommandBuffer cmd) {
    Slot& s = g_slot[app.currentFrame];
    if (!g_started || g_capNow || !s.hudMap) return;
    std::string toast;
    double age = 0.0;
    bool showToast = capToastGet(toast, age);
    if (!g_recording && !showToast) return;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    OverlayVertex* o = (OverlayVertex*)s.hudMap;
    uint32_t tris = 0;
    const float bg[3] = {0.012f, 0.022f, 0.05f};
    const float red[3] = {1.0f, 0.15f, 0.15f};
    const float white[3] = {0.95f, 0.97f, 1.0f};
    int ts = H >= 500.0f ? 2 : 1;
    char rec[32] = "";
    float rx = 12.0f, ry = 12.0f;
    float btn = std::max(24.0f, std::floor(H / 26.0f));
    if (g_recording) {
        int sec = (int)(capClock() - g_recStart);
        std::snprintf(rec, sizeof(rec), "REC %02d:%02d", sec / 60, sec % 60);
        float rw = (float)(std::strlen(rec) * 6) * ts + 16.0f * ts;
        float rh = 7.0f * ts + 12.0f * ts;
        rx = W - 24.0f - btn - rw;
        ry = 12.0f + (btn - rh) * 0.5f;
        hudRect(o, tris, W, H, rx, ry, rx + rw, ry + rh, bg);
    }
    std::string up;
    for (char c : toast) {
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        if (c == '_') c = '-';
        if (c == '\\') c = '/';
        up += c;
    }
    int tts = ts;
    float tw = (float)up.size() * 6.0f * tts;
    while (tts > 1 && tw + 40.0f > W) {
        tts--;
        tw = (float)up.size() * 6.0f * tts;
    }
    float tx = std::max(8.0f, (W - tw) * 0.5f), ty = H - 40.0f * tts;
    if (showToast) hudRect(o, tris, W, H, tx - 8.0f * tts, ty - 6.0f * tts, tx + tw + 8.0f * tts, ty + 13.0f * tts, bg);
    uint32_t n = tris;
    if (g_recording) hudText(o, n, W, H, rec, rx + 8.0f * ts, ry + 6.0f * ts, ts, red);
    if (showToast) hudText(o, n, W, H, up.c_str(), tx, ty, tts, white);
    VkDeviceSize off = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &s.hudBuf, &off);
    if (tris) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.overlayTris);
        vkCmdDraw(cmd, tris, 1, 0, 0);
    }
    if (n > tris) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.overlayPoints);
        vkCmdDraw(cmd, n - tris, 1, tris, 0);
    }
}

void captureShutdown(AppContext& app) {
    if (!g_started) return;
    for (Slot& s : g_slot) harvest(app, s);
    if (g_recording) stopRec();
    if (g_finishWait) pushFinish();
    capIoStop();
    for (Slot& s : g_slot) {
        freeBuf(app, s.buf, s.mem, s.map);
        freeBuf(app, s.hudBuf, s.hudMem, s.hudMap);
    }
    g_started = false;
}
