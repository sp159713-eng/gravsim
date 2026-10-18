#include "vk_app.h"
#include "gpu.h"
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

struct Stop {
    const char* name;
    const char* tag;
    const char* size;
    const char* fact[3];
    double radius;
    float temp;
};

const Stop STOPS[] = {
    {"EARTH", "EARTH", "RADIUS 6,371 KM  -  1 EARTH",
     {"OUR HOME, AND THE YARDSTICK FOR EVERYTHING THAT FOLLOWS", "", ""}, 6371.0 / 695700.0, 0.0f},
    {"THE SUN", "SUN", "109 X EARTH'S RADIUS",
     {"1.3 MILLION EARTHS WOULD FIT INSIDE OUR STAR", "", ""}, 1.0, 5772.0f},
    {"SIRIUS A", "SIRIUS A", "~1.7 X THE SUN'S RADIUS",
     {"THE BRIGHTEST STAR IN OUR NIGHT SKY,", "WITH 25 TIMES THE SUN'S LIGHT", ""}, 1.71, 9940.0f},
    {"R136A1", "R136A1", "~40 X THE SUN'S RADIUS",
     {"HEAVIEST KNOWN STAR - ABOUT 200 SUNS OF MASS -", "YET ONLY ~40 SUN RADII ACROSS.", "HEAVY IS NOT THE SAME AS BIG."}, 40.0, 46000.0f},
    {"BETELGEUSE", "BETELGEUSE", "~760 X THE SUN'S RADIUS",
     {"PUT IT WHERE THE SUN IS AND IT SWALLOWS THE ORBITS", "OF MERCURY, VENUS, EARTH AND MARS.", ""}, 760.0, 3600.0f},
    {"VY CANIS MAJORIS", "VY CMA", "~1,400 X THE SUN'S RADIUS",
     {"A RED HYPERGIANT - ITS SURFACE WOULD REACH", "PAST JUPITER'S ORBIT.", ""}, 1400.0, 3490.0f},
    {"WOH G64", "WOH G64", "~1,500 X THE SUN'S RADIUS",
     {"A RED SUPERGIANT WRAPPED IN DUST, IN THE LARGE", "MAGELLANIC CLOUD, 160,000 LIGHT YEARS AWAY.", "IN 2024: FIRST STAR OUTSIDE OUR GALAXY IMAGED UP CLOSE"}, 1500.0, 3400.0f},
    {"UY SCUTI", "UY SCUTI", "~1,700 X THE SUN'S RADIUS",
     {"A PULSATING RED SUPERGIANT - ABOUT 5 BILLION SUNS", "WOULD FIT INSIDE IT.", ""}, 1700.0, 3365.0f},
    {"STEPHENSON 2-18", "STEPH 2-18", "~2,150 X THE SUN'S RADIUS",
     {"POSSIBLY THE LARGEST KNOWN STAR - IT WOULD REACH", "PAST SATURN'S ORBIT. ITS EXACT SIZE IS UNCERTAIN.", ""}, 2150.0, 3200.0f},
};
const int COUNT = 9;

struct Planet {
    const char* name;
    double au;
};

const Planet PLANETS[] = {
    {"MERCURY", 0.387}, {"VENUS", 0.723}, {"EARTH", 1.0}, {"MARS", 1.524}, {"JUPITER", 5.203}, {"SATURN", 9.537},
};
const double SUN_PER_AU = 215.0;
const double ORBIT_MIN_RADIUS = 100.0;
const float HOLD = 7.5f;
const float FADE_IN = 0.8f;
const float FADE_OUT = 0.6f;
const double FILL = 1.85;
const float CENTRE_Y = 0.36f;

struct PushData {
    float a[4], s[4][4], ids[4];
};

float smooth01(float x) {
    x = std::max(0.0f, std::min(1.0f, x));
    return x * x * (3.0f - 2.0f * x);
}

const double* positions() {
    static double x[COUNT];
    static bool ready = false;
    if (!ready) {
        x[0] = 0.0;
        for (int i = 1; i < COUNT; i++) x[i] = x[i - 1] + STOPS[i - 1].radius + 3.6 * STOPS[i - 1].radius + STOPS[i].radius;
        ready = true;
    }
    return x;
}

double halfView(int i) {
    return FILL * STOPS[i].radius;
}

void viewAt(float s, double& xc, double& v) {
    s = std::max(0.0f, std::min((float)(COUNT - 1), s));
    int i = std::min(COUNT - 2, (int)std::floor(s));
    double f = s - (float)i;
    double fz = 1.0 - (1.0 - f) * (1.0 - f), fp = f * f;
    xc = positions()[i] + (positions()[i + 1] - positions()[i]) * fp;
    v = std::exp(std::log(halfView(i)) + (std::log(halfView(i + 1)) - std::log(halfView(i))) * fz);
}

void startMove(StarState& S, int goal) {
    double a = std::log(halfView(std::min(COUNT - 1, (int)std::lround(S.s))));
    double b = std::log(halfView(goal));
    S.from = S.s;
    S.goal = goal;
    S.moveT = 0.0f;
    S.moveLen = 2.2f + 1.2f * (float)(std::fabs(b - a) / 2.302585);
}

float textAlpha(const StarState& S) {
    if (S.moveT < 1.0f) return 0.0f;
    float a = smooth01(S.dwell / FADE_IN);
    if (S.mode == 1 && S.goal < COUNT - 1) a *= smooth01((HOLD - S.dwell) / FADE_OUT);
    return a;
}

bool offSame(float a, float b, float lo, float hi) {
    return (a < lo && b < lo) || (a > hi && b > hi);
}

struct Lines {
    OverlayVertex* out;
    uint32_t& used;
    float W, H, px, py;

    void seg(float x0, float y0, float x1, float y1, const float c[3]) {
        if (used + 2 > MAX_VERTS - 12000) return;
        if (offSame(x0, x1, 0.0f, W) || offSame(y0, y1, 0.0f, H)) return;
        out[used++] = {-1.0f + x0 * px, -1.0f + y0 * py, c[0], c[1], c[2]};
        out[used++] = {-1.0f + x1 * px, -1.0f + y1 * py, c[0], c[1], c[2]};
    }

    void circle(float cx, float cy, float r, const float c[3]) {
        if (r < 2.5f || r > 3e5f) return;
        int n = std::min(720, std::max(64, (int)(r / 3.0f)));
        float x0 = cx + r, y0 = cy;
        for (int i = 1; i <= n; i++) {
            float a = 6.2831853f * (float)i / (float)n;
            float x1 = cx + r * std::cos(a), y1 = cy + r * std::sin(a);
            seg(x0, y0, x1, y1, c);
            x0 = x1;
            y0 = y1;
        }
    }
};

struct Box {
    float x0, y0, x1, y1;
};

bool clash(const Box& a, const Box& b) {
    return a.x0 < b.x1 + 4.0f && a.x1 + 4.0f > b.x0 && a.y0 < b.y1 + 4.0f && a.y1 + 4.0f > b.y0;
}

}

void starsRestart(AppContext& app) {
    StarState& S = app.stars;
    S = StarState{};
    app.camera.zoomImpulse = 0.0f;
}

void starsSetStop(AppContext& app, int stop, float dwell) {
    StarState& S = app.stars;
    S = StarState{};
    S.goal = std::max(0, std::min(COUNT - 1, stop));
    S.s = (float)S.goal;
    S.from = S.s;
    S.dwell = dwell;
}

void starsToggle(AppContext& app) {
    StarState& S = app.stars;
    S.mode = S.mode == 1 ? 2 : 1;
}

void starsStep(AppContext& app, int dir) {
    StarState& S = app.stars;
    int goal = std::max(0, std::min(COUNT - 1, S.goal + dir));
    if (goal == S.goal) return;
    startMove(S, goal);
}

void starsUpdate(AppContext& app, float dt) {
    StarState& S = app.stars;
    app.camera.zoomImpulse = 0.0f;
    dt = std::min(dt, 0.1f);
    S.clock += dt;
    if (S.moveT < 1.0f) {
        S.moveT += dt / S.moveLen;
        if (S.moveT >= 1.0f) {
            S.moveT = 1.0f;
            S.s = (float)S.goal;
            S.dwell = 0.0f;
        } else {
            S.s = S.from + ((float)S.goal - S.from) * smooth01(S.moveT);
        }
        return;
    }
    if (S.mode != 1) return;
    S.dwell += dt;
    if (S.dwell >= HOLD && S.goal < COUNT - 1) startMove(S, S.goal + 1);
}

void createStarSizes(AppContext& app) {
    VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushData)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &push};
    if (vkCreatePipelineLayout(app.device, &layoutInfo, nullptr, &app.starLayout) != VK_SUCCESS)
        throw std::runtime_error("failed star sizes layout!");
    VkShaderModule vertSM = createSM(app.device, readSpvFile(shaderPath("sky.vert")));
    VkShaderModule fragSM = createSM(app.device, readSpvFile(shaderPath("starsizes.frag")));
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
    VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, nullptr, 0, 2, stages, &vertInput, &inputAss, nullptr, &vpState, &raster, &multi, nullptr, &blend, &dyn, app.starLayout, app.renderPass, 0, VK_NULL_HANDLE, -1};
    if (vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &info, nullptr, &app.starPipeline) != VK_SUCCESS)
        throw std::runtime_error("failed star sizes pipeline!");
    vkDestroyShaderModule(app.device, fragSM, nullptr);
    vkDestroyShaderModule(app.device, vertSM, nullptr);
}

void destroyStarSizes(AppContext& app) {
    vkDestroyPipeline(app.device, app.starPipeline, nullptr);
    vkDestroyPipelineLayout(app.device, app.starLayout, nullptr);
}

void drawStarSizes(AppContext& app, VkCommandBuffer cmd, float aspect) {
    const StarState& S = app.stars;
    double xc, v;
    viewAt(S.s, xc, v);
    float H = (float)app.swapchainExtent.height;
    PushData p{};
    p.a[0] = aspect;
    p.a[1] = app.stars.clock;
    p.a[2] = 2.0f / H;
    int i0 = std::min(COUNT - 1, (int)std::floor(S.s + 1e-4f));
    for (int k = 0; k < 4; k++) {
        int idx = i0 - 1 + k;
        if (idx < 0 || idx >= COUNT) {
            p.ids[k] = -1.0f;
            continue;
        }
        p.s[k][0] = (float)((positions()[idx] - xc) / v);
        p.s[k][1] = 2.0f * CENTRE_Y - 1.0f;
        p.s[k][2] = (float)(STOPS[idx].radius / v);
        p.s[k][3] = STOPS[idx].temp;
        p.ids[k] = (float)idx;
    }
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.starPipeline);
    vkCmdPushConstants(cmd, app.starLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushData), &p);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void emitStarSizes(AppContext& app, OverlayVertex* out, uint32_t& used) {
    const StarState& S = app.stars;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float px = 2.0f / W, py = 2.0f / H;
    float k = H / 720.0f;
    double xc, v;
    viewAt(S.s, xc, v);
    float pxPerUnit = (float)(0.5 * H / v);
    float cy = CENTRE_Y * H;
    int i0 = std::min(COUNT - 1, (int)std::floor(S.s + 1e-4f));
    Lines L{out, used, W, H, px, py};

    struct Orbit {
        float cx, r;
        const char* name;
    };
    std::vector<Orbit> orbits;
    for (int idx = i0; idx <= std::min(COUNT - 1, i0 + 1); idx++) {
        if (STOPS[idx].radius < ORBIT_MIN_RADIUS) continue;
        float w = std::max(0.0f, std::min(1.0f, 1.0f - 2.0f * std::fabs(S.s - (float)idx)));
        if (w <= 0.0f) continue;
        float cx = 0.5f * W + (float)((positions()[idx] - xc) * pxPerUnit);
        float c[3] = {0.62f * w, 0.85f * w, 1.0f * w};
        for (const Planet& pl : PLANETS) {
            float r = (float)(pl.au * SUN_PER_AU * pxPerUnit);
            L.circle(cx, cy, r, c);
            if (w > 0.99f) orbits.push_back({cx, r, pl.name});
        }
    }
    uint32_t lineVerts = used;

    std::vector<Box> boxes;
    float starCx[COUNT], starR[COUNT];
    for (int idx = 0; idx < COUNT; idx++) {
        starCx[idx] = 0.5f * W + (float)((positions()[idx] - xc) * pxPerUnit);
        starR[idx] = (float)(STOPS[idx].radius * pxPerUnit);
    }
    int focus = std::min(COUNT - 1, (int)std::lround(S.s));
    for (const Orbit& o : orbits) {
        if (o.r < 12.0f) continue;
        float tl = (float)std::strlen(o.name) * 6.0f * 2.0f, th = 14.0f;
        const float angles[5] = {0.5f, 0.8f, 1.1f, 1.4f, 0.25f};
        for (float a : angles) {
            float x = o.cx + o.r * std::cos(a), y = cy - o.r * std::sin(a);
            Box b{x + 4.0f, y - th - 2.0f, x + 4.0f + tl, y - 2.0f};
            if (b.x0 < 4.0f || b.x1 > W - 4.0f || b.y0 < 4.0f || b.y1 > H - 4.0f) continue;
            bool bad = false;
            for (const Box& q : boxes) bad = bad || clash(b, q);
            if (bad) continue;
            boxes.push_back(b);
            emitText(out, used, o.name, -1.0f + b.x0 * px, -1.0f + b.y0 * py, px, py, 2.0f, 0.75f, 0.9f, 1.0f);
            break;
        }
    }
    for (int idx = 0; idx < COUNT; idx++) {
        if (idx == focus) continue;
        if (starR[idx] < 1.2f || starR[idx] > 0.2f * H || starCx[idx] < 40.0f || starCx[idx] > W - 40.0f) continue;
        if (std::fabs(starCx[idx] - starCx[focus]) < starR[focus] + 4.0f) continue;
        const char* tag = STOPS[idx].tag;
        float tl = (float)std::strlen(tag) * 6.0f;
        float ly = cy - std::max(starR[idx], 2.0f) - 16.0f;
        Box b{starCx[idx] - 0.5f * tl, ly, starCx[idx] + 0.5f * tl, ly + 7.0f};
        bool bad = false;
        for (const Box& q : boxes) bad = bad || clash(b, q);
        if (bad) continue;
        boxes.push_back(b);
        emitText(out, used, tag, -1.0f + b.x0 * px, -1.0f + ly * py, px, py, 1.0f, 0.75f, 0.8f, 0.9f);
    }

    float a = textAlpha(S);
    if (a > 0.0f && S.goal == focus) {
        const Stop& st = STOPS[S.goal];
        int s1 = fitScale(st.name, W, std::max(2, (int)std::lround(6.0f * k)));
        int s2 = fitScale(st.size, W, std::max(2, (int)std::lround(4.0f * k)));
        float y = std::floor(0.655f * H);
        bigText(out, used, st.name, W, y, px, py, s1, 1.0f * a, 0.85f * a, 0.45f * a);
        y += 7.0f * (float)s1 + 14.0f * k;
        bigText(out, used, st.size, W, y, px, py, s2, 0.85f * a, 0.9f * a, 1.0f * a);
        y += 7.0f * (float)s2 + 12.0f * k;
        for (int i = 0; i < 3; i++) {
            if (!st.fact[i][0]) continue;
            int s3 = fitScale(st.fact[i], W, std::max(1, (int)std::lround(3.0f * k)));
            bigText(out, used, st.fact[i], W, y, px, py, s3, 1.0f * a, 0.65f * a, 0.5f * a);
            y += 7.0f * (float)s3 + 5.0f * k;
        }
    }

    emitText(out, used, "STAR SIZES - TRUE SCALE", -1.0f + 12.0f * px, -1.0f + 12.0f * py, px, py, 2.0f, 0.6f, 0.8f, 1.0f);
    emitText(out, used, "CIRCLES: ORBITS OF MERCURY TO SATURN   1 AU = 215 SUN RADII", -1.0f + 12.0f * px, -1.0f + 36.0f * py, px, py, 1.0f, 0.7f, 0.75f, 0.85f);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "STOP %d OF %d", focus + 1, COUNT);
    emitText(out, used, buf, -1.0f + 12.0f * px, -1.0f + 48.0f * py, px, py, 1.0f, 0.9f, 0.9f, 0.95f);

    for (int i = 0; i < COUNT; i++) {
        const char* tag = STOPS[i].tag;
        float tl = (float)std::strlen(tag) * 6.0f;
        float cx = W * (0.06f + 0.88f * ((float)i + 0.5f) / (float)COUNT);
        float c = i == focus ? 1.0f : 0.5f;
        emitText(out, used, tag, -1.0f + (cx - 0.5f * tl) * px, -1.0f + 0.934f * H * py, px, py, 1.0f, c, c * 0.9f, c * 0.75f);
    }
    const char* hint = S.mode == 1 ? "SPACE PAUSE  R RESTART  LEFT RIGHT STEP" : "PAUSED - SPACE RESUME  R RESTART  LEFT RIGHT STEP";
    float hl = (float)std::strlen(hint) * 6.0f;
    emitText(out, used, hint, -1.0f + (W - hl - 14.0f) * px, -1.0f + (H - 20.0f) * py, px, py, 1.0f, 0.7f, 0.75f, 0.85f);
    app.overlayLineVerts[app.currentFrame] = lineVerts;
    app.overlayPointVerts[app.currentFrame] = used - lineVerts;
}
