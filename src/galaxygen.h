#pragma once
#include "milkyway.h"
#include <cstdint>
#include <string>
#include <vector>

enum class GalType { SPIRAL, ELLIPTICAL, IRREGULAR, LENTICULAR };

struct GalaxySpec {
    uint32_t index = 0;
    int named = 0;
    GalType type = GalType::SPIRAL;
    std::string name = "MILKY WAY";
    std::string typeName = "BARRED SPIRAL";
    float radiusLy = 50000.0f;
    float bhMsun = 4.0e6f;
    float starsBillion = 200.0f;
    int arms = 4;
    float pitchDeg = 12.0f;
    bool bar = true;
    int ellN = 0;
    float normal[3] = {0.0f, 0.0f, 1.0f};
    float e1[3] = {1.0f, 0.0f, 0.0f};
    float e2[3] = {0.0f, 1.0f, 0.0f};
    float anchor[3] = {0.0f, 0.0f, 0.0f};
    double centreMly[3] = {0.0, 0.0, 0.0};
    float distMly = 0.0f;
    uint32_t seed = 0;
};

struct GalState {
    bool active = false;
    GalaxySpec spec;
    double originMly[3] = {0.0, 0.0, 0.0};
    double centreMly[3] = {0.0, 0.0, 0.0};
    float scale = 1.0f;
    float fadeIn = 1.0f;
    int hover = -1;
    float hoverPos[3] = {0.0f, 0.0f, 0.0f};
    int hoverFrame = 0;
    int hoverNamed = 0;
    int uniPoint = -1;
    bool hidden = false;
    int kind = 0;
    float t = 0.0f, dur = 1.0f;
    bool swapped = false;
    double e0[3] = {0.0, 0.0, 0.0};
    double dir0[3] = {0.0, 0.0, 1.0};
    double dir1[3] = {0.0, 0.0, 1.0};
    double d0 = 1.0, d1 = 1.0;
    double f0[3] = {0.0, 0.0, -1.0};
    double p1[3] = {0.0, 0.0, 0.0};
};

constexpr float MW_RADIUS_LY = 50000.0f;
constexpr float SGR_MSUN = 4.0e6f;

GalaxySpec galaxySpec(uint32_t index, int named, const double centreMly[3]);
std::vector<GalaxyPoint> generateGalaxy(GalaxySpec& spec, unsigned count);
std::string fmtMass(float msun);
