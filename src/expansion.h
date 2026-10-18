#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

struct AppContext;
struct OverlayVertex;
struct LgPen;

constexpr double EX_TEND_GYR = 50.0;
constexpr double EX_BREAK = 0.22;
constexpr double EX_TMIN = 1e-43;

struct ExpansionState {
    bool ready = false;
    bool playing = false;
    double tSec = 0.0;
    double glideTo = -1.0;
    float clock = 0.0f;
    bool keyPrev[8] = {};
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
};

struct ExTexts {
    struct Item {
        std::string s;
        float x, y, sc;
        float c[3];
    };
    std::vector<Item> q;
    void add(const std::string& s, float x, float y, float sc, const float* c) { q.push_back({s, x, y, sc, {c[0], c[1], c[2]}}); }
    float width(const std::string& s, float sc) const { return (float)s.size() * 6.0f * sc; }
    void flush(OverlayVertex* out, uint32_t& used, float W, float H);
};

struct ExEra {
    double t0, t1;
    const char* title;
    const char* sub1;
    const char* sub2;
    const char* bar;
    float col[3];
};

extern float g_exAtGyr;

ExpansionState& exState();
const std::vector<ExEra>& exEras();
int exEraAt(double tSec);
double exTimeToU(double tSec);
double exUToTime(double u);
void exTempColor(double kelvin, float rgb[3]);
std::string exFmtAge(double tSec);
std::string exFmtTemp(double kelvin);
std::string exFmtHubble(double h);
std::string exFmtScale(double a);

void createExpansion(AppContext& app);
void destroyExpansion(AppContext& app);
void exEnter(AppContext& app);
void exLeave(AppContext& app);
void exUpdate(AppContext& app, float dt);
void drawExpansion(AppContext& app, VkCommandBuffer cmd, float aspect);
void exEmitBar(LgPen& pen, ExTexts& tx, float W, float H, float k, double tSec);
void emitExpansion(AppContext& app, OverlayVertex* out, uint32_t& used);
int cosmoCheck();
