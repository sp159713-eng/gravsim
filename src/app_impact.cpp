#include "vk_app.h"
#include "gpu.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<char> readSpvFile(const std::string& path);
VkShaderModule createSM(VkDevice dev, const std::vector<char>& code);

int g_imPreset = 0;
float g_imAt = -1.0f;

namespace {

const float PI = 3.14159265f;
const float DEG = PI / 180.0f;
const float AZ_IN = 300.0f * DEG;
const float MISS_R = 6.02f;

struct PushData {
    float a[4], b[4], c[4], d[4], e[4], f[4], g[4], h[4];
};

float dot3(const float* a, const float* b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

void cross3(const float* a, const float* b, float* o) {
    o[0] = a[1] * b[2] - a[2] * b[1];
    o[1] = a[2] * b[0] - a[0] * b[2];
    o[2] = a[0] * b[1] - a[1] * b[0];
}

void unit(float* v) {
    float l = std::sqrt(dot3(v, v));
    if (l > 1e-12f) { v[0] /= l; v[1] /= l; v[2] /= l; }
}

void tangents(const float* n, float* east, float* north) {
    const float z[3] = {0, 0, 1};
    cross3(z, n, east);
    if (dot3(east, east) < 1e-10f) { east[0] = 0; east[1] = 1; east[2] = 0; }
    unit(east);
    cross3(n, east, north);
    unit(north);
}

void hdir(const float* n, float az, float* out) {
    float e[3], no[3];
    tangents(n, e, no);
    for (int i = 0; i < 3; i++) out[i] = std::cos(az) * no[i] + std::sin(az) * e[i];
}

void fromLatLon(float lat, float lon, float* n) {
    n[0] = std::cos(lat * DEG) * std::cos(lon * DEG);
    n[1] = std::cos(lat * DEG) * std::sin(lon * DEG);
    n[2] = std::sin(lat * DEG);
}

float frameD(const ImpactState& S) {
    if (S.miss) return 12.0f;
    float reach = std::max(S.r.r1Km, S.r.burstKm * 1.4f);
    return std::max(0.0004f, std::min(6.5f, reach / IM_EARTH_KM * 2.6f));
}

float tIn(const ImpactState& S) { return S.miss ? IM_T_MISS : IM_T_IN; }

bool keyDown(AppContext& app, int k) { return glfwGetKey(app.window, k) == GLFW_PRESS; }

bool edge(ImpactState& S, int idx, bool down, float dt, bool repeat) {
    bool fire = false;
    if (down) {
        if (!S.keyPrev[idx]) {
            S.keyHold[idx] = 0.0f;
            fire = true;
        } else {
            S.keyHold[idx] += dt;
            if (repeat && S.keyHold[idx] > 0.35f) {
                S.keyHold[idx] -= 0.06f;
                fire = true;
            }
        }
    }
    S.keyPrev[idx] = down;
    return fire;
}

void changed(ImpactState& S) {
    S.preset = -1;
    S.miss = false;
    S.running = false;
    S.t = 0.0f;
    imRefresh(S);
}

}

void imLoadPreset(ImpactState& S, int idx) {
    const ImpactPreset& pr = IM_PRESETS[idx];
    S.p.diaM = pr.diaM;
    S.p.speedKms = pr.speedKms;
    S.p.angleDeg = pr.angleDeg;
    S.p.type = pr.type;
    S.preset = idx;
    S.miss = pr.miss;
    S.running = false;
    S.t = 0.0f;
    imRefresh(S);
}

void imRefresh(ImpactState& S) {
    S.r = imCompute(S.p);
    imGeometry(S);
}

void imGeometry(ImpactState& S) {
    fromLatLon(S.lat, S.lon, S.I);
    float dirIn[3], h[3];
    hdir(S.I, AZ_IN, h);
    float th = S.p.angleDeg * DEG;
    if (S.miss) {
        float heading[3];
        hdir(S.I, AZ_IN + PI, heading);
        for (int i = 0; i < 3; i++) {
            float m = S.I[i] * MISS_R;
            S.A[i] = m - heading[i] * 14.0f;
            S.B[i] = m + heading[i] * 14.0f;
        }
        return;
    }
    for (int i = 0; i < 3; i++) dirIn[i] = std::cos(th) * h[i] + std::sin(th) * S.I[i];
    float alt = S.r.airburst ? S.r.burstKm / IM_EARTH_KM : 0.0f;
    for (int i = 0; i < 3; i++) {
        S.B[i] = S.I[i] * (1.0f + alt);
        S.A[i] = S.B[i] + dirIn[i] * 1.4f;
    }
}

float imHitT(const ImpactState& S) { return S.running && !S.miss ? S.t - IM_T_IN : -1000.0f; }

void imCamera(ImpactState& S, float dt) {
    if (S.running || S.recenter) {
        float k = 1.0f - std::exp(-3.0f * dt);
        for (int i = 0; i < 3; i++) S.nv[i] += (S.I[i] - S.nv[i]) * k;
        unit(S.nv);
        float dx = S.I[0] - S.nv[0], dy = S.I[1] - S.nv[1], dz = S.I[2] - S.nv[2];
        if (!S.running && dx * dx + dy * dy + dz * dz < 1e-10f) S.recenter = false;
    }
    float base = 2.8f;
    float tilt = 0.58f;
    if (S.miss) {
        base = 12.0f;
        tilt = 0.35f;
    } else if (S.running && imHitT(S) > 0.15f) {
        base = frameD(S);
    }
    float target = std::log(std::max(0.0003f, std::min(24.0f, base * S.zoom)));
    float cur = std::log(std::max(0.0003f, S.camD));
    bool diving = S.running && !S.miss && imHitT(S) > 0.15f;
    float rate = diving ? 1.3f : 4.0f;
    cur += (target - cur) * (1.0f - std::exp(-rate * dt));
    S.camD = std::exp(cur);
    S.camTilt = tilt;
    S.camAz = AZ_IN + PI * 0.5f;

    float e[3], no[3], eT[3], u[3];
    tangents(S.nv, e, no);
    for (int i = 0; i < 3; i++) eT[i] = std::cos(S.camAz) * no[i] + std::sin(S.camAz) * e[i];
    for (int i = 0; i < 3; i++) u[i] = S.nv[i] * std::cos(tilt) + eT[i] * std::sin(tilt);
    for (int i = 0; i < 3; i++) {
        S.camPos[i] = S.nv[i] + S.camD * u[i];
        S.F[i] = -u[i];
        S.U[i] = -eT[i] * std::cos(tilt) + S.nv[i] * std::sin(tilt);
    }
    cross3(S.F, S.U, S.R);
    unit(S.R);
}

bool imProject(const ImpactState& S, const float* P, float& x, float& y) {
    float v[3] = {P[0] - S.camPos[0], P[1] - S.camPos[1], P[2] - S.camPos[2]};
    float z = dot3(v, S.F);
    if (z < 1e-5f) return false;
    float cx = 0.5f * (S.panelEdge - 1.0f);
    x = dot3(v, S.R) / z / (S.tanf * S.aspect) + cx;
    y = -dot3(v, S.U) / z / S.tanf;
    return true;
}

void imSurfacePoint(const ImpactState& S, float angKm, float bearing, float* out) {
    float h[3];
    hdir(S.I, bearing, h);
    float a = angKm / IM_EARTH_KM;
    for (int i = 0; i < 3; i++) out[i] = S.I[i] * std::cos(a) + h[i] * std::sin(a);
}

void createImpact(AppContext& app) {
    ImpactState& S = app.im;
    VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushData)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &push};
    if (vkCreatePipelineLayout(app.device, &layoutInfo, nullptr, &S.layout) != VK_SUCCESS)
        throw std::runtime_error("failed impact layout!");
    VkShaderModule vertSM = createSM(app.device, readSpvFile(shaderPath("sky.vert")));
    VkShaderModule fragSM = createSM(app.device, readSpvFile(shaderPath("impact.frag")));
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
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, stages, &vertInput, &inputAss, nullptr, &vpState, &raster, &multi, nullptr, &blend, &dyn, S.layout, app.renderPass, 0, VK_NULL_HANDLE, -1};
    if (vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &info, nullptr, &S.pipeline) != VK_SUCCESS)
        throw std::runtime_error("failed impact pipeline!");
    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
}

void destroyImpact(AppContext& app) {
    vkDestroyPipeline(app.device, app.im.pipeline, nullptr);
    vkDestroyPipelineLayout(app.device, app.im.layout, nullptr);
}

void imEnter(AppContext& app) {
    ImpactState& S = app.im;
    S.ready = true;
    S.lat = 28.61f;
    S.lon = 77.21f;
    S.p = ImpactParams{};
    S.p.diaM = 100.0f;
    S.zoom = 1.0f;
    S.recenter = false;
    S.mousePrev = false;
    S.dragged = false;
    S.preset = -1;
    S.miss = false;
    S.running = false;
    S.t = 0.0f;
    imGeometry(S);
    for (int i = 0; i < 3; i++) S.nv[i] = S.I[i];
    if (g_imPreset >= 1 && g_imPreset <= IM_PRESETS_N) imLoadPreset(S, g_imPreset - 1);
    else imRefresh(S);
    g_imPreset = 0;
    if (g_imAt >= 0.0f) {
        S.running = true;
        S.t = g_imAt;
    }
    S.camD = 2.8f;
    S.aspect = (float)app.swapchainExtent.width / (float)app.swapchainExtent.height;
    S.panelEdge = 1.0f - 2.0f * 432.0f * ((float)app.swapchainExtent.height / 720.0f) / (float)app.swapchainExtent.width;
    imCamera(S, 0.0f);
    if (g_imAt >= 0.0f) imCamera(S, 100.0f);
    g_imAt = -1.0f;
    for (int i = 0; i < 16; i++) {
        S.keyPrev[i] = true;
        S.keyHold[i] = 0.0f;
    }
    app.showSky = false;
    app.camera.zoomImpulse = 0.0f;
    app.camera.wheelHold = true;
    app.camera.wheelAcc = 0.0f;
}

void imLeave(AppContext& app) {
    app.camera.wheelHold = false;
    app.camera.wheelAcc = 0.0f;
}

void imUpdate(AppContext& app, float dt, bool mouse) {
    ImpactState& S = app.im;
    dt = std::min(dt, 0.1f);
    app.camera.zoomImpulse = 0.0f;
    app.camera.wheelHold = true;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    S.aspect = W / H;
    S.panelEdge = std::max(-0.2f, 1.0f - 2.0f * 432.0f * (H / 720.0f) / W);

    float wheel = std::max(-8.0f, std::min(8.0f, app.camera.wheelAcc));
    app.camera.wheelAcc = 0.0f;
    if (wheel != 0.0f) S.zoom = std::max(0.08f, std::min(8.0f, S.zoom * std::pow(0.88f, wheel)));

    bool shift = keyDown(app, GLFW_KEY_LEFT_SHIFT) || keyDown(app, GLFW_KEY_RIGHT_SHIFT);
    bool dirty = false;
    if (edge(S, 0, keyDown(app, GLFW_KEY_LEFT_BRACKET), dt, true)) { S.p.diaM = std::max(10.0f, S.p.diaM / 1.1548f); dirty = true; }
    if (edge(S, 1, keyDown(app, GLFW_KEY_RIGHT_BRACKET), dt, true)) { S.p.diaM = std::min(15000.0f, S.p.diaM * 1.1548f); dirty = true; }
    if (edge(S, 2, keyDown(app, GLFW_KEY_SEMICOLON), dt, true)) { S.p.speedKms = std::max(11.0f, S.p.speedKms - 1.0f); dirty = true; }
    if (edge(S, 3, keyDown(app, GLFW_KEY_APOSTROPHE), dt, true)) { S.p.speedKms = std::min(72.0f, S.p.speedKms + 1.0f); dirty = true; }
    if (edge(S, 4, keyDown(app, GLFW_KEY_COMMA), dt, true)) { S.p.angleDeg = std::max(15.0f, S.p.angleDeg - 5.0f); dirty = true; }
    if (edge(S, 5, keyDown(app, GLFW_KEY_PERIOD), dt, true)) { S.p.angleDeg = std::min(90.0f, S.p.angleDeg + 5.0f); dirty = true; }
    if (edge(S, 6, keyDown(app, GLFW_KEY_TAB), dt, false)) { S.p.type = (S.p.type + 1) % IM_TYPES; dirty = true; }
    if (dirty) changed(S);

    if (edge(S, 7, keyDown(app, GLFW_KEY_SPACE), dt, false)) {
        S.running = true;
        S.t = 0.0f;
        S.recenter = false;
    }
    for (int i = 0; i < IM_PRESETS_N; i++) {
        bool d = !shift && (keyDown(app, GLFW_KEY_1 + i) || keyDown(app, GLFW_KEY_KP_1 + i));
        if (edge(S, 8 + i, d, dt, false)) {
            imLoadPreset(S, i);
            S.recenter = true;
        }
    }

    if (S.running) S.t = std::min(S.t + dt, 1000.0f);

    double cx = app.cursorFb[0], cy = app.cursorFb[1];
    if (mouse && app.cursorOk) {
        if (!S.mousePrev) {
            S.downX = S.lastX = (float)cx;
            S.downY = S.lastY = (float)cy;
            S.dragged = false;
        } else {
            float dx = (float)cx - S.lastX, dy = (float)cy - S.lastY;
            if (std::fabs((float)cx - S.downX) + std::fabs((float)cy - S.downY) > 5.0f) S.dragged = true;
            if (S.dragged && !S.running) {
                float k = 2.0f * S.tanf * S.camD / H;
                float d[3], tmp[3];
                for (int i = 0; i < 3; i++) d[i] = (-dx * S.R[i] + dy * S.U[i]) * k;
                float dn = dot3(d, S.nv);
                for (int i = 0; i < 3; i++) tmp[i] = S.nv[i] + d[i] - dn * S.nv[i];
                unit(tmp);
                for (int i = 0; i < 3; i++) S.nv[i] = tmp[i];
                S.recenter = false;
            }
            S.lastX = (float)cx;
            S.lastY = (float)cy;
        }
        S.mousePrev = true;
    } else {
        if (S.mousePrev && !S.dragged && app.cursorOk) {
            float nx = 2.0f * S.downX / W - 1.0f, ny = 2.0f * S.downY / H - 1.0f;
            if (nx < S.panelEdge && !S.miss) {
                float c0 = 0.5f * (S.panelEdge - 1.0f);
                float d[3];
                for (int i = 0; i < 3; i++) d[i] = S.F[i] + S.R[i] * ((nx - c0) * S.tanf * S.aspect) - S.U[i] * (ny * S.tanf);
                unit(d);
                double ro[3] = {S.camPos[0], S.camPos[1], S.camPos[2]};
                double b = ro[0] * d[0] + ro[1] * d[1] + ro[2] * d[2];
                double c = ro[0] * ro[0] + ro[1] * ro[1] + ro[2] * ro[2] - 1.0;
                double disc = b * b - c;
                if (disc > 0.0) {
                    double t = -b - std::sqrt(disc);
                    if (t > 0.0) {
                        double P[3] = {ro[0] + t * d[0], ro[1] + t * d[1], ro[2] + t * d[2]};
                        S.lat = (float)(std::asin(std::max(-1.0, std::min(1.0, P[2]))) / DEG);
                        S.lon = (float)(std::atan2(P[1], P[0]) / DEG);
                        changed(S);
                        S.recenter = true;
                    }
                }
            }
        }
        S.mousePrev = false;
    }
    imCamera(S, dt);
}

void drawImpact(AppContext& app, VkCommandBuffer cmd, float aspect) {
    ImpactState& S = app.im;
    S.aspect = aspect;
    float tHit = imHitT(S);
    float u = 0.0f;
    float tt = S.running ? S.t : 0.0f;
    if (S.miss) u = std::min(1.0f, tt / IM_T_MISS);
    else u = std::pow(std::min(1.0f, tt / IM_T_IN), 2.2f);
    if (!S.running) u = 0.0f;
    float frameKm = frameD(S) * IM_EARTH_KM;
    float rfv = std::max(S.r.fireballKm, 0.04f * S.camD * IM_EARTH_KM);
    float craterR = S.r.airburst ? 0.0f : 0.5f * S.r.finalKm;
    rfv = std::max(rfv, 2.0f * craterR);
    float ej = S.r.airburst ? 0.0f : std::max(S.r.finalKm * 2.5f, 0.07f * frameKm);
    PushData p{};
    for (int i = 0; i < 3; i++) {
        p.a[i] = S.F[i]; p.b[i] = S.R[i]; p.c[i] = S.camPos[i];
        p.d[i] = S.I[i]; p.e[i] = S.A[i]; p.f[i] = S.B[i];
    }
    p.a[3] = S.tanf;
    p.b[3] = aspect;
    p.c[3] = (float)glfwGetTime();
    p.d[3] = tHit;
    p.e[3] = S.miss ? 1.0f : 0.0f;
    p.f[3] = S.panelEdge;
    p.g[0] = S.r.r1Km;
    p.g[1] = S.r.r5Km;
    p.g[2] = craterR;
    p.g[3] = rfv;
    p.h[0] = S.r.veil;
    p.h[1] = ej;
    p.h[2] = u;
    p.h[3] = (!S.miss && tHit < 0.0f) ? 1.0f : 0.0f;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, S.pipeline);
    vkCmdPushConstants(cmd, S.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushData), &p);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}
