#pragma once
#include <cstdint>
#include <vector>

struct Particle {
    float pos[4];
    float vel[4];
};

struct SimParams {
    float dt;
    float G;
    float soft;
    uint32_t n;
    uint32_t bh = 0;
    float bodySoft = -1.0f;
    float prevDt = -1.0f;
    uint32_t pad = 0;
};

constexpr uint32_t SOLAR_SUB = 1;
constexpr float BODY_SOFT_RATIO = 0.05f;
constexpr uint32_t BODY_PAIRS = 9;

enum class Scene { SOLAR, GALAXY, COLLISION, BHSIZES, LIFECYCLE, ANDROMEDA, STARSIZES, SANDBOX, EARTHMOON, TIMEDIL, LAGRANGE, LIGHTRIDE, FUTURESKY, NIGHTSKY, IMPACT, ECLIPSE, EXPANSION, BHDISC };

inline bool collisionLike(Scene scene) { return scene == Scene::COLLISION || scene == Scene::ANDROMEDA; }

std::vector<Particle> makeGalaxy(uint32_t n, uint32_t seed);
std::vector<Particle> makeCollision(uint32_t n, uint32_t seed);
std::vector<Particle> makeAndromeda(uint32_t n, uint32_t seed);
constexpr uint32_t ANDROMEDA_SUN = 2;
constexpr uint32_t ANDROMEDA_N = 2048;
std::vector<Particle> makeScene(Scene scene, uint32_t n, uint32_t seed, bool trueScale);
const char* sceneName(Scene scene);
std::vector<Particle> makeBhSizes(uint32_t n);
std::vector<Particle> makeSolarSystem(uint32_t n, uint32_t seed, bool trueScale);
float solarG(bool trueScale);
float solarSoft(bool trueScale);
float solarUnitsPerAu(bool trueScale);
float bodyRadius(bool trueScale, int body);
void setBodyRadii(const float* trueR, const float* enlR);
void clearBodyRadii();
float followDistance(bool trueScale, int body);
void cpuStep(std::vector<Particle>& particles, const SimParams& params);
