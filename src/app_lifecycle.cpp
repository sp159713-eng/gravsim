#include "vk_app.h"
#include "gpu.h"
#include "lifecycle.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

std::vector<char> readSpvFile(const std::string& path);
VkShaderModule createSM(VkDevice dev, const std::vector<char>& code);

namespace {

const float AU_KM = 1.496e8f;
const float LY_AU = 63241.0f;
const float FADE = 0.7f;
const float CUT_FADE = 0.55f;
const float PULSAR_SPIN = 9.0f;

struct ReqPush {
    float a[4], b[4], c[4], d[4], e[4], f[4];
};

float totalLen(const std::vector<int>& seq) {
    float t = 0.0f;
    for (int i : seq) t += STAGES[i].secs;
    return t;
}

float smooth(float x) {
    x = std::max(0.0f, std::min(1.0f, x));
    return x * x * (3.0f - 2.0f * x);
}

float lerp(float a, float b, float x) { return a + (b - a) * x; }

float lerpLog(float a, float b, float x) {
    if (a <= 0.0f || b <= 0.0f) return lerp(a, b, x);
    return std::exp(lerp(std::log(a), std::log(b), x));
}

struct Where {
    int idx;
    int stage;
    float local;
    float u;
};

Where locate(const std::vector<int>& seq, float t) {
    float at = 0.0f;
    for (size_t i = 0; i < seq.size(); i++) {
        float len = STAGES[seq[i]].secs;
        if (t < at + len || i + 1 == seq.size()) {
            float local = std::min(t - at, len);
            return {(int)i, seq[i], local, local / len};
        }
        at += len;
    }
    return {0, seq[0], 0.0f, 0.0f};
}

struct Frame {
    int kind;
    float u, viewH, R, T, aux, flash, ring, orbitSet, orbitAlpha, spin, fade;
    float roll, elev, epoch;
};

bool cutAt(const std::vector<int>& seq, int i) {
    if (seq[i] == ST_SUPERNOVA && i > 0 && seq[i - 1] == ST_WOLF_RAYET) return true;
    return STAGES[seq[i]].cut;
}

Frame frameAt(const AppContext& app, const std::vector<int>& seq, float t) {
    const LifeState& L = app.life;
    Where w = locate(seq, t);
    const Stage& s = STAGES[w.stage];
    Frame f{};
    float u = w.u;
    float ze = s.zEnd > 0.0f ? s.zEnd : 1.0f;
    float ez = smooth(u / ze);
    f.kind = s.kind;
    f.u = u;
    f.R = lerpLog(s.r0, s.r1, u);
    f.T = lerpLog(s.t0, s.t1, u);
    f.viewH = lerpLog(s.v0, s.v1, s.kind == 3 || s.kind == 4 || s.kind == 2 ? u : ez);
    f.aux = lerp(s.a0, s.a1, u);
    f.ring = s.g0 > 0.0f && s.g1 > 0.0f ? lerpLog(s.g0, s.g1, s.kind == 5 ? u : ez) : s.g0;
    float M = lifeStageMass(L, w.stage);
    float kRG = std::pow(M / 20.0f, 0.4f);
    float kR8 = std::pow(M / 20.0f, 0.6f);
    f.epoch = lifeEpoch(w.stage, u);
    f.orbitSet = w.stage == 9 ? 1.0f : 0.0f;
    f.orbitAlpha = s.kind == 3 ? 1.0f : 0.0f;
    if (s.special == 1) {
        float shrink = smooth((u - 0.12f) / 0.18f);
        f.R = lerpLog(1.2f, 0.3f, shrink);
        if (u > 0.32f) f.R = lerpLog(0.3f, 2.0f, smooth((u - 0.32f) / 0.68f));
        f.R *= 1.0f + 0.05f * std::sin(6.2831853f * u * 8.0f) * smooth((u - 0.4f) * 4.0f);
        f.flash = std::exp(-std::pow((u - 0.12f) / 0.05f, 2.0f));
        f.T = lerpLog(3300.0f, 4800.0f, shrink);
        if (u > 0.32f) f.T = lerpLog(4800.0f, 3200.0f, smooth((u - 0.32f) / 0.68f));
        f.viewH = lerpLog(6.0f, 40.0f, smooth(u));
    } else if (s.special == 2) {
        float cu = std::min(1.0f, u / 0.2f);
        f.R = lerpLog(5.6f, 0.01f, cu * cu);
        f.T = lerpLog(3500.0f, 60000.0f, cu);
        f.flash = smooth((u - 0.17f) / 0.04f) * std::exp(-(std::max(u, 0.21f) - 0.21f) / 0.06f);
        float rs = lerpLog(0.05f, 2.5e5f, smooth((u - 0.2f) / 0.8f));
        f.ring = u < 0.2f ? 0.05f : rs;
        f.viewH = std::max(30.0f, f.ring / lerp(0.06f, 0.227f, smooth((u - 0.2f) / 0.8f)));
    } else if (s.special == 3) {
        f.T = u < 0.5f ? s.t0 : lerpLog(s.t0, s.t1, smooth((u - 0.5f) / 0.5f));
    } else if (s.special == 4) {
        f.T = lerpLog(s.t0, s.t1, smooth(u));
    }
    if (w.stage == 9) {
        f.R = lerpLog(0.045f * kR8, 5.6f * kRG, u);
        f.T = lerpLog(31000.0f * std::pow(M / 20.0f, 0.3f), 3500.0f, u);
        f.viewH = lerpLog(0.28f * kR8, 30.0f * kRG, u);
    }
    if (w.stage == 10) {
        f.R = lerpLog(5.6f * kRG, 0.01f, std::pow(std::min(1.0f, u / 0.2f), 2.0f));
        f.viewH = std::max(30.0f * kRG, f.ring / lerp(0.06f, 0.227f, smooth((u - 0.2f) / 0.8f)));
    }
    if (s.special == 5) {
        float ph = std::fmod(u * 3.0f + 0.15f, 1.0f) - 0.5f;
        f.flash = 0.4f * std::exp(-ph * ph / 0.0006f);
    } else if (s.special == 6) {
        f.T = lerpLog(s.t0, s.t1, smooth(u));
    }
    if (s.kind == 12) {
        float q1 = (u - 0.3f) / 0.02f, q2 = (u - 0.72f) / 0.02f;
        f.flash = 0.9f * std::exp(-q1 * q1) + 1.1f * std::exp(-q2 * q2);
        f.spin = L.t * 0.9f;
    }
    if (s.kind == 13) {
        float q = (u - 0.57f) / 0.03f;
        f.flash = std::exp(-q * q) * 1.3f + (u > 0.57f ? 0.18f * std::exp(-(u - 0.57f) * 9.0f) : 0.0f);
    }
    if (w.stage == 2) {
        float kR = std::pow(M, 0.8f);
        f.R *= kR;
        f.viewH *= kR;
        f.T *= std::pow(M, 0.55f);
    } else if (w.stage == 8) {
        f.R *= kR8;
        f.viewH *= kR8;
        f.T *= std::pow(M / 20.0f, 0.3f);
    }
    if (s.kind == 9) f.spin = L.t * PULSAR_SPIN;
    if (s.kind == 1) f.aux = lerp(s.a0, s.a1, smooth(u));
    float inFade = cutAt(seq, w.idx) ? smooth(w.local / CUT_FADE) : 1.0f;
    float outFade = 1.0f;
    if (w.idx + 1 < (int)seq.size() && cutAt(seq, w.idx + 1)) outFade = smooth((s.secs - w.local) / CUT_FADE);
    f.fade = std::min(inFade, outFade);
    float prevPitch = w.idx > 0 ? STAGES[seq[w.idx - 1]].pitch : s.pitch;
    float prevRoll = w.idx > 0 ? STAGES[seq[w.idx - 1]].roll : s.roll;
    float blend = smooth(w.local / 2.0f);
    float pitch = lerp(prevPitch, s.pitch, blend);
    float roll = lerp(prevRoll, s.roll, blend) + 0.06f * std::sin(L.t * 0.17f);
    f.roll = L.manual ? L.roll : roll;
    f.elev = L.manual ? L.elev : pitch + 0.05f * std::sin(L.t * 0.11f);
    f.viewH *= L.zoom;
    return f;
}

std::string niceBar(float viewH, float H, float W, float& lenAu) {
    float target = 0.13f * W * viewH / H;
    const char* unit = "AU";
    float scale = 1.0f;
    if (target >= 10000.0f) {
        unit = target >= 2.0f * LY_AU ? "LIGHT YEARS" : "LIGHT YEAR";
        scale = LY_AU;
    } else if (target < 0.2f) {
        unit = "KM";
        scale = 1.0f / AU_KM;
    }
    float v = target / scale;
    float mag = std::pow(10.0f, std::floor(std::log10(v)));
    float n = v / mag;
    float nice = n >= 5.0f ? 5.0f : n >= 2.0f ? 2.0f : 1.0f;
    float val = nice * mag;
    lenAu = val * scale;
    if (scale == LY_AU && val == 1.0f) unit = "LIGHT YEAR";
    else if (scale == LY_AU) unit = "LIGHT YEARS";
    char buf[48];
    if (val >= 1.0f || val == std::floor(val)) std::snprintf(buf, sizeof(buf), "%.0f %s", val, unit);
    else std::snprintf(buf, sizeof(buf), "%g %s", val, unit);
    return buf;
}

void applyAutoCamera(AppContext& app, const Frame& f) {
    LifeState& L = app.life;
    if (L.manual) return;
    L.roll = f.roll;
    L.elev = f.elev;
    app.camera.yaw = L.yaw = f.roll;
    app.camera.pitch = L.pitch = f.elev;
}

}

void lifeRestart(AppContext& app) {
    LifeState& L = app.life;
    L.mode = 1;
    L.t = 0.0f;
    L.manual = false;
    L.zoom = 1.0f;
    L.roll = 0.0f;
    L.elev = 0.3f;
    app.camera.zoomImpulse = 0.0f;
    std::vector<int> seq = lifeSequence(L);
    applyAutoCamera(app, frameAt(app, seq, 0.0f));
}

void lifeToggle(AppContext& app) {
    LifeState& L = app.life;
    if (L.mode == 1) L.mode = 2;
    else if (L.mode == 2) L.mode = 1;
}

void lifeCycleMass(AppContext& app) {
    app.life.mass = app.life.mass >= 3 ? 0 : (app.life.mass + 1) % 3;
    lifeRestart(app);
}

void lifeStepMass(AppContext& app, int dir) {
    LifeState& L = app.life;
    if (L.mass != 3) {
        L.preset = L.mass == 1 ? 5 : 2;
        L.mass = 3;
    } else {
        L.preset = std::max(0, std::min(PRESET_COUNT - 1, L.preset + dir));
    }
    lifeRestart(app);
}

void lifeUpdateCamera(AppContext& app, float dt) {
    LifeState& L = app.life;
    Camera& c = app.camera;
    float wheel = c.zoomImpulse;
    c.zoomImpulse = 0.0f;
    if (!L.manual && (std::fabs(c.yaw - L.yaw) > 1e-4f || std::fabs(c.pitch - L.pitch) > 1e-4f || wheel != 0.0f)) {
        L.manual = true;
        L.roll = c.yaw;
        L.elev = std::max(0.05f, std::min(1.4f, std::fabs(c.pitch)));
    }
    if (L.manual) {
        L.roll = c.yaw;
        L.elev = std::max(0.05f, std::min(1.4f, std::fabs(c.pitch)));
        L.zoom = std::max(0.02f, std::min(50.0f, L.zoom * std::exp(-0.15f * wheel)));
    }
    std::vector<int> seq = lifeSequence(L);
    if (L.mode == 1) L.t += std::min(dt, 0.1f);
    float total = totalLen(seq);
    if (L.t >= total) {
        L.t = total - 1e-3f;
        if (L.mode == 1) L.mode = 0;
    }
    applyAutoCamera(app, frameAt(app, seq, L.t));
}

void createLifecycle(AppContext& app) {
    VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ReqPush)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &push};
    if (vkCreatePipelineLayout(app.device, &layoutInfo, nullptr, &app.lifeLayout) != VK_SUCCESS)
        throw std::runtime_error("failed lifecycle layout!");
    VkShaderModule vertSM = createSM(app.device, readSpvFile(shaderPath("sky.vert")));
    VkShaderModule fragSM = createSM(app.device, readSpvFile(shaderPath("lifecycle.frag")));
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
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, stages, &vertInput, &inputAss, nullptr, &vpState, &raster, &multi, nullptr, &blend, &dyn, app.lifeLayout, app.renderPass, 0, VK_NULL_HANDLE, -1};
    if (vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &info, nullptr, &app.lifePipeline) != VK_SUCCESS)
        throw std::runtime_error("failed lifecycle pipeline!");
    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
}

void drawLifecycle(AppContext& app, VkCommandBuffer cmd, float aspect) {
    std::vector<int> seq = lifeSequence(app.life);
    Frame f = frameAt(app, seq, app.life.t);
    float H = (float)app.swapchainExtent.height, W = (float)app.swapchainExtent.width;
    float barAu = 0.0f;
    niceBar(f.viewH, H, W, barAu);
    Where w = locate(seq, app.life.t);
    ReqPush p{};
    p.a[0] = aspect; p.a[1] = app.life.t; p.a[2] = (float)f.kind; p.a[3] = f.u;
    p.b[0] = f.viewH; p.b[1] = f.R; p.b[2] = f.T; p.b[3] = f.aux;
    p.c[0] = f.roll; p.c[1] = f.elev; p.c[2] = f.flash; p.c[3] = f.ring;
    p.d[0] = barAu / f.viewH * H / W; p.d[1] = (float)seq.size(); p.d[2] = (float)w.idx; p.d[3] = f.fade;
    p.e[0] = 2.0f / H; p.e[1] = f.spin; p.e[2] = f.orbitSet; p.e[3] = f.orbitAlpha;
    p.f[0] = f.epoch; p.f[1] = 1.0f; p.f[2] = lifeUiScale(H);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.lifePipeline);
    vkCmdPushConstants(cmd, app.lifeLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ReqPush), &p);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void destroyLifecycle(AppContext& app) {
    vkDestroyPipeline(app.device, app.lifePipeline, nullptr);
    vkDestroyPipelineLayout(app.device, app.lifeLayout, nullptr);
}

void emitMassSlider(AppContext& app, OverlayVertex* out, uint32_t& used, float sm) {
    const LifeState& L = app.life;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float px = 2.0f / W, py = 2.0f / H;
    std::string labels[PRESET_COUNT];
    float total = 6.0f * 6.0f;
    for (int i = 0; i < PRESET_COUNT; i++) {
        labels[i] = (L.mass == 3 && i == L.preset ? "(" : " ") + lifeMassText(MASS_PRESETS[i]) + (L.mass == 3 && i == L.preset ? ")" : " ");
        total += ((float)labels[i].size() + 1.0f) * 6.0f;
    }
    float x = 0.5f * (W - total);
    emitText(out, used, "MASS", -1.0f + x * px, -1.0f + 16.0f * py, px, py, 1.0f, 0.7f, 0.8f, 1.0f);
    x += 36.0f;
    for (int i = 0; i < PRESET_COUNT; i++) {
        bool cur = L.mass == 3 && i == L.preset;
        float c = cur ? 1.0f : 0.5f;
        emitText(out, used, labels[i].c_str(), -1.0f + x * px, -1.0f + 16.0f * py, px, py, 1.0f, c, cur ? 0.85f : c, cur ? 0.35f : c);
        x += ((float)labels[i].size() + 1.0f) * 6.0f;
    }
    std::string line = (L.mass == 3 ? "" : "CLASSIC PRESET - ") + lifeMassText(sm) + (sm == 1.0f ? " SUN" : " SUNS") + "   LIFETIME ABOUT " + lifeLifetimeText(sm);
    float ll = (float)line.size() * 6.0f;
    emitText(out, used, line.c_str(), -1.0f + 0.5f * (W - ll) * px, -1.0f + 30.0f * py, px, py, 1.0f, 0.75f, 0.85f, 1.0f);
}

void emitLifecycle(AppContext& app, OverlayVertex* out, uint32_t& used) {
    const LifeState& L = app.life;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float px = 2.0f / W, py = 2.0f / H;
    float k = H / 720.0f;
    std::vector<int> seq = lifeSequence(L);
    Frame f = frameAt(app, seq, L.t);
    Where w = locate(seq, L.t);
    const Stage& st = STAGES[w.stage];
    float sm = lifeStageMass(L, w.stage);
    StageText tx = lifeStageText(w.stage, sm);

    float len = st.secs;
    if (w.local < len - 0.3f) {
        float a = std::max(0.0f, std::min(1.0f, std::min(w.local, len - 0.3f - w.local) / FADE));
        a = a * a * (3.0f - 2.0f * a);
        const std::string& fact = !tx.fact2.empty() && w.u >= 0.5f ? tx.fact2 : tx.fact;
        int s1 = fitScale(tx.name.c_str(), W, std::max(2, (int)std::lround(6.0f * k)));
        int s2 = fitScale(tx.span.c_str(), W, std::max(2, (int)std::lround(4.0f * k)));
        int s3 = fitScale(fact.c_str(), W, std::max(1, (int)std::lround(3.0f * k)));
        float y = std::floor(0.68f * H);
        bigText(out, used, tx.name.c_str(), W, y, px, py, s1, 1.0f * a, 0.85f * a, 0.45f * a);
        y += 7.0f * (float)s1 + 14.0f * k;
        bigText(out, used, tx.span.c_str(), W, y, px, py, s2, 0.85f * a, 0.9f * a, 1.0f * a);
        y += 7.0f * (float)s2 + 12.0f * k;
        bigText(out, used, fact.c_str(), W, y, px, py, s3, 1.0f * a, 0.65f * a, 0.5f * a);
        if (!tx.ex.empty()) {
            y += 7.0f * (float)s3 + 10.0f * k;
            int s4 = fitScale(tx.ex.c_str(), W, std::max(1, (int)std::lround(2.0f * k)));
            bigText(out, used, tx.ex.c_str(), W, y, px, py, s4, 0.5f * a, 0.9f * a, 0.8f * a);
        }
    }

    char buf[96];
    const char* massName = L.mass == 3 ? "STAR" : L.mass == 0 ? "SUN-LIKE STAR (1 SUN)" : L.mass == 1 ? "MASSIVE STAR (20 SUNS)" : "SUN-LIKE THEN MASSIVE STAR";
    std::snprintf(buf, sizeof(buf), "%s   STAGE %d OF %d", massName, w.idx + 1, (int)seq.size());
    emitText(out, used, buf, -1.0f + 16.0f * px, -1.0f + 16.0f * py, px, py, 1.0f, 0.85f, 0.9f, 1.0f);
    if (f.kind >= 1 && f.kind <= 6 && f.kind != 5) {
        std::snprintf(buf, sizeof(buf), "SURFACE %.0f K", f.T);
        emitText(out, used, buf, -1.0f + 16.0f * px, -1.0f + 30.0f * py, px, py, 1.0f, 1.0f, 0.75f, 0.5f);
    }

    emitLifeTable(app, out, used, f.epoch);
    emitMassSlider(app, out, used, sm);

    float barAu = 0.0f;
    std::string label = niceBar(f.viewH, H, W, barAu);
    emitText(out, used, label.c_str(), -1.0f + 0.04f * W * px, -1.0f + (0.11f * H - 16.0f) * py, px, py, 1.0f, 0.9f, 0.9f, 0.95f);

    if (f.orbitAlpha > 0.0f) {
        const char* names[2][4] = {{"MERCURY", "VENUS", "EARTH", "MARS"}, {"EARTH", "MARS", "JUPITER", "SATURN"}};
        const float au[2][4] = {{0.387f, 0.723f, 1.0f, 1.524f}, {1.0f, 1.524f, 5.203f, 9.537f}};
        int set = f.orbitSet > 0.5f ? 1 : 0;
        float lastY = -100.0f;
        for (int i = 0; i < 4; i++) {
            float r = au[set][i] / f.viewH * H;
            if (r < 26.0f || r > 0.48f * H || au[set][i] < f.R) continue;
            float y = 0.5f * H - r - 12.0f;
            if (std::fabs(y - lastY) < 11.0f) continue;
            lastY = y;
            float tl = (float)std::strlen(names[set][i]) * 6.0f;
            emitText(out, used, names[set][i], -1.0f + (0.5f * W - 0.5f * tl) * px, -1.0f + y * py, px, py, 1.0f, 0.6f, 0.75f, 1.0f);
        }
    }

    int n = (int)seq.size();
    for (int i = 0; i < n; i++) {
        const char* tag = STAGES[seq[i]].tag;
        float tl = (float)std::strlen(tag) * 6.0f;
        float cx = W * (0.06f + 0.88f * ((float)i + 0.5f) / (float)n);
        float c = i == w.idx ? 1.0f : 0.5f;
        emitText(out, used, tag, -1.0f + (cx - 0.5f * tl) * px, -1.0f + 0.934f * H * py, px, py, 1.0f, c, c * 0.9f, c * 0.75f);
    }

    const char* hint = L.mode == 1 ? "SPACE PAUSE  R RESTART  LEFT RIGHT MASS  M PRESETS  DRAG TO LOOK" : L.mode == 2 ? "PAUSED - SPACE RESUME  R RESTART  LEFT RIGHT MASS  M PRESETS" : "R REPLAY  LEFT RIGHT MASS  M PRESETS";
    float hl = (float)std::strlen(hint) * 6.0f;
    emitText(out, used, hint, -1.0f + (W - hl - 14.0f) * px, -1.0f + (H - 20.0f) * py, px, py, 1.0f, 0.7f, 0.75f, 0.85f);
}
