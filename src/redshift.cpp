#include "redshift.h"
#include "cosmo.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags properties);

bool g_redshift = false;

namespace {

struct Stop {
    float z, r, g, b, bright;
};

const Stop STOPS[] = {
    {0.0f, 0.70f, 0.85f, 1.00f, 1.0f},
    {0.1f, 0.95f, 0.97f, 1.00f, 1.0f},
    {0.3f, 1.00f, 0.93f, 0.65f, 1.0f},
    {0.55f, 1.00f, 0.72f, 0.28f, 1.0f},
    {1.0f, 1.00f, 0.38f, 0.12f, 1.0f},
    {1.6f, 0.85f, 0.14f, 0.05f, 1.0f},
    {2.5f, 0.62f, 0.05f, 0.03f, 0.9f},
    {4.0f, 0.42f, 0.03f, 0.03f, 0.55f},
    {7.0f, 0.30f, 0.02f, 0.02f, 0.35f},
};

float smooth01(float x) {
    x = std::clamp(x, 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

struct Named {
    const char* name;
    double z;
};

const Named NAMED[] = {
    {"TON 618", 2.22},
    {"PHOENIX CLUSTER", 0.597},
    {"EL GORDO", 0.87},
    {"HERCULES-CORONA BOREALIS GREAT WALL", 1.9},
};

double zOfLabel(const UniLabel& l, double& lookbackGyr) {
    double z = -1.0;
    for (const Named& n : NAMED)
        if (std::strcmp(n.name, l.text) == 0) z = n.z;
    if (z < 0.0) {
        double d = std::sqrt((double)l.pos[0] * l.pos[0] + (double)l.pos[1] * l.pos[1] + (double)l.pos[2] * l.pos[2]);
        if (d < 100.0) return -1.0;
        z = cosmo::zOfComovingMly(d);
    }
    lookbackGyr = cosmo::lookbackGyrOfZ(z);
    return z;
}

const float TICKS[] = {0.0f, 0.5f, 1.0f, 2.0f, 3.0f, 6.0f};
const char* TICK_NAMES[] = {"Z 0", "Z 0.5", "Z 1", "Z 2", "Z 3", "Z 6+"};

float zToU(float z) { return std::log10(1.0f + z) / std::log10(7.0f); }
float uToZ(float u) { return std::pow(7.0f, std::clamp(u, 0.0f, 1.0f)) - 1.0f; }

struct Legend {
    float W, H, k, x0, x1, yb, th;
};

Legend legend(const AppContext& app) {
    Legend L;
    L.W = (float)app.swapchainExtent.width;
    L.H = (float)app.swapchainExtent.height;
    L.k = L.H / 720.0f;
    L.x0 = L.W * 0.5f - 230.0f * L.k;
    L.x1 = L.W * 0.5f + 230.0f * L.k;
    L.yb = L.H - 92.0f * L.k;
    L.th = 11.0f * L.k;
    return L;
}

bool legendOn(const AppContext& app) {
    return g_redshift && app.scene == Scene::SOLAR && app.showUniverse && universeFade(app) > 0.05f;
}

std::string ageText(double gyr) {
    char b[48];
    std::snprintf(b, sizeof b, "%.1f BILLION YEARS AGO", gyr);
    return b;
}

void text(OverlayVertex* out, uint32_t& used, const Legend& L, const std::string& s, float cx, float y, float sc, const float* c, bool center) {
    float px = 2.0f / L.W, py = 2.0f / L.H;
    float w = (float)s.size() * 6.0f * sc;
    float x = center ? cx - 0.5f * w : cx;
    emitText(out, used, s.c_str(), -1.0f + x * px, -1.0f + y * py, px, py, sc, c[0], c[1], c[2]);
}

}

void redshiftRamp(double zd, float rgb[3], float& bright) {
    float z = (float)zd;
    const int n = (int)(sizeof(STOPS) / sizeof(STOPS[0]));
    if (z >= STOPS[n - 1].z) {
        rgb[0] = STOPS[n - 1].r;
        rgb[1] = STOPS[n - 1].g;
        rgb[2] = STOPS[n - 1].b;
        bright = STOPS[n - 1].bright;
        return;
    }
    int i = 0;
    while (i + 2 < n && z > STOPS[i + 1].z) i++;
    const Stop &a = STOPS[i], &b = STOPS[i + 1];
    float f = std::clamp((z - a.z) / (b.z - a.z), 0.0f, 1.0f);
    rgb[0] = a.r + f * (b.r - a.r);
    rgb[1] = a.g + f * (b.g - a.g);
    rgb[2] = a.b + f * (b.b - a.b);
    bright = a.bright + f * (b.bright - a.bright);
}

std::vector<UniPoint> redshiftPoints(const std::vector<UniPoint>& pts, unsigned keep) {
    std::vector<UniPoint> out = pts;
    for (size_t i = keep; i < out.size(); i++) {
        UniPoint& q = out[i];
        double d = std::sqrt((double)q.pos[0] * q.pos[0] + (double)q.pos[1] * q.pos[1] + (double)q.pos[2] * q.pos[2]);
        double z = cosmo::zOfComovingMly(d);
        float rgb[3], br;
        redshiftRamp(z, rgb, br);
        float w = smooth01((float)z / 0.08f);
        float luma = 0.3f * q.color[0] + 0.5f * q.color[1] + 0.2f * q.color[2];
        float shape = 0.75f + 0.25f * std::min(luma * 1.4f, 1.0f);
        for (int a = 0; a < 3; a++) q.color[a] = q.color[a] + w * (rgb[a] * shape - q.color[a]);
        q.bright *= 1.0f + w * (br - 1.0f);
    }
    return out;
}

void redshiftCreateBuffer(AppContext& app, const std::vector<UniPoint>& pts, VkBuffer& buf, VkDeviceMemory& mem) {
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.size = pts.size() * sizeof(UniPoint);
    info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(app.device, &info, nullptr, &buf) != VK_SUCCESS) throw std::runtime_error("failed redshift buffer!");
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(app.device, buf, &req);
    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = req.size;
    alloc.memoryTypeIndex = findMemoryType(app.physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(app.device, &alloc, nullptr, &mem) != VK_SUCCESS) throw std::runtime_error("failed redshift memory!");
    vkBindBufferMemory(app.device, buf, mem, 0);
    void* ptr;
    vkMapMemory(app.device, mem, 0, info.size, 0, &ptr);
    std::memcpy(ptr, pts.data(), (size_t)info.size);
    vkUnmapMemory(app.device, mem);
}

void redshiftKey(AppContext& app) {
    static bool prev = false;
    bool now = !timeTyping() && glfwGetKey(app.window, GLFW_KEY_V) == GLFW_PRESS && glfwGetKey(app.window, GLFW_KEY_LEFT_SHIFT) != GLFW_PRESS;
    if (now && !prev && app.scene == Scene::SOLAR) g_redshift = !g_redshift;
    prev = now;
}

void emitRedshiftLines(AppContext& app, OverlayVertex* out, uint32_t& used) {
    if (!legendOn(app)) return;
    Legend L = legend(app);
    float px = 2.0f / L.W, py = 2.0f / L.H;
    float a = std::min(1.0f, universeFade(app) * 1.5f);
    int cols = (int)(L.x1 - L.x0);
    for (int i = 0; i <= cols; i++) {
        float z = uToZ((float)i / (float)cols);
        float rgb[3], br;
        redshiftRamp(z, rgb, br);
        float x = L.x0 + (float)i;
        float c = 0.85f * a * (0.45f + 0.55f * br);
        out[used++] = {-1.0f + x * px, -1.0f + L.yb * py, rgb[0] * c, rgb[1] * c, rgb[2] * c};
        out[used++] = {-1.0f + x * px, -1.0f + (L.yb + L.th) * py, rgb[0] * c, rgb[1] * c, rgb[2] * c};
    }
    for (float t : TICKS) {
        float x = L.x0 + zToU(t) * (L.x1 - L.x0);
        float c = 0.8f * a;
        out[used++] = {-1.0f + x * px, -1.0f + (L.yb - 4.0f * L.k) * py, c, c, c};
        out[used++] = {-1.0f + x * px, -1.0f + (L.yb + L.th + 3.0f * L.k) * py, c, c, c};
    }
}

void emitRedshiftText(AppContext& app, OverlayVertex* out, uint32_t& used) {
    if (!legendOn(app)) return;
    Legend L = legend(app);
    float a = std::min(1.0f, universeFade(app) * 1.5f);
    float sc = std::max(1.0f, 1.3f * L.k);
    const float head[3] = {1.0f * a, 0.85f * a, 0.5f * a};
    const float dim[3] = {0.7f * a, 0.74f * a, 0.84f * a};
    const float cap[3] = {1.0f * a, 0.55f * a, 0.35f * a};
    text(out, used, L, "REDSHIFT Z - LIGHT LEFT X BILLION YEARS AGO", 0.5f * L.W, L.yb - 36.0f * L.k, std::max(1.0f, 1.5f * L.k), head, true);
    for (int i = 0; i < 6; i++) {
        float x = L.x0 + zToU(TICKS[i]) * (L.x1 - L.x0);
        text(out, used, L, TICK_NAMES[i], x, L.yb - 18.0f * L.k, sc, dim, true);
        double gyr = cosmo::lookbackGyrOfZ(TICKS[i]);
        char b[32];
        if (i == 0) std::snprintf(b, sizeof b, "NOW");
        else std::snprintf(b, sizeof b, "%.1f GYR", gyr);
        text(out, used, L, b, x, L.yb + L.th + 8.0f * L.k, sc, dim, true);
    }
    text(out, used, L, "SPACE ITSELF STRETCHES THE LIGHT ON ITS WAY TO US - THE FARTHER, THE REDDER", 0.5f * L.W, L.yb + L.th + 30.0f * L.k, std::max(1.0f, 1.6f * L.k), cap, true);

    if (!app.cursorOk) return;
    static std::vector<UniLabel> labels = universeLabels();
    float cx = (float)app.cursorFb[0], cy = (float)app.cursorFb[1];
    float ex[3];
    galaxyEye(app, ex);
    float eye[3];
    for (int i = 0; i < 3; i++) eye[i] = (float)(app.gal.originMly[i] + (double)ex[i] * 1e-6);
    float best = (46.0f * L.k) * (46.0f * L.k);
    const UniLabel* hit = nullptr;
    float hx = 0.0f, hy = 0.0f;
    for (const UniLabel& l : labels) {
        if (l.dynamic) continue;
        double lb;
        if (zOfLabel(l, lb) < 0.0) continue;
        float dx = l.pos[0] - eye[0], dy = l.pos[1] - eye[1], dz = l.pos[2] - eye[2];
        float w = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (w < l.lo || w > l.hi) continue;
        float nx, ny;
        if (!universeProject(app, l.pos, nx, ny) || std::fabs(nx) > 1.0f || std::fabs(ny) > 1.0f) continue;
        float sx = (nx + 1.0f) * 0.5f * L.W, sy = (ny + 1.0f) * 0.5f * L.H;
        float d2 = (sx - cx) * (sx - cx) + (sy - cy) * (sy - cy);
        if (d2 < best) {
            best = d2;
            hit = &l;
            hx = sx;
            hy = sy;
        }
    }
    if (!hit) return;
    double lb;
    double z = zOfLabel(*hit, lb);
    float rgb[3], br;
    redshiftRamp(z, rgb, br);
    char b1[96], b2[96];
    std::snprintf(b1, sizeof b1, "REDSHIFT Z = %.*f", z < 0.1 ? 3 : 2, z);
    std::snprintf(b2, sizeof b2, "STRETCHED BY (1+Z) = %.2fX", 1.0 + z);
    float s2 = std::max(1.0f, 1.7f * L.k);
    (void)hx;
    (void)hy;
    float x = std::min(cx + 18.0f * L.k, L.W - 330.0f * L.k);
    float y = std::min(cy + 34.0f * L.k, L.H - 150.0f * L.k);
    const float c1[3] = {rgb[0], rgb[1] * 0.95f, rgb[2]};
    const float c3[3] = {0.8f, 0.82f, 0.9f};
    text(out, used, L, b1, x, y, s2, c1, false);
    text(out, used, L, b2, x, y + 14.0f * L.k, s2, c1, false);
    text(out, used, L, "LIGHT LEFT " + ageText(lb), x, y + 28.0f * L.k, s2 * 0.85f, c3, false);
}
