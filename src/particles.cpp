#include "particles.h"
#include "ephem.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

constexpr float PI = 3.14159265358979323846f;

std::vector<Particle> makeGalaxy(uint32_t n, uint32_t seed) {
    std::vector<Particle> particles;
    if (n == 0) return particles;

    particles.reserve(n);

    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> radiusDist(5.0f, 30.0f);
    std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * PI);
    std::uniform_real_distribution<float> yDist(-1.0f, 1.0f);

    // Heavy centre star
    Particle centre;
    centre.pos[0] = 0.0f;
    centre.pos[1] = 0.0f;
    centre.pos[2] = 0.0f;
    centre.pos[3] = 1000.0f; // mass
    centre.vel[0] = 0.0f;
    centre.vel[1] = 0.0f;
    centre.vel[2] = 0.0f;
    centre.vel[3] = 0.0f;
    particles.push_back(centre);

    // Other stars
    for (uint32_t i = 1; i < n; ++i) {
        float r = radiusDist(rng);
        float theta = angleDist(rng);
        float y = yDist(rng);

        float x = r * std::cos(theta);
        float z = r * std::sin(theta);

        Particle p;
        p.pos[0] = x;
        p.pos[1] = y;
        p.pos[2] = z;
        p.pos[3] = 1.0f; // mass

        // Orbital speed magnitude: sqrt(G * M_center / r) with G=1, M_center=1000
        float v_mag = std::sqrt(1000.0f / r);

        // Direction perpendicular to radius in xz plane: dir = normalize(-z, 0, x)
        // Since norm(-z, 0, x) = r, dir = (-z/r, 0, x/r) = (-sin(theta), 0, cos(theta))
        float vx = -v_mag * std::sin(theta);
        float vy = 0.0f;
        float vz = v_mag * std::cos(theta);

        p.vel[0] = vx;
        p.vel[1] = vy;
        p.vel[2] = vz;
        p.vel[3] = 0.0f;

        particles.push_back(p);
    }

    return particles;
}

constexpr float COLLISION_CENTRE_SOFT2 = 16.0f;

struct CollisionParams {
    float sep = 50.0f, impact = 22.0f, vFrac = 0.55f, centreMass = 5000.0f;
    float rMin = 3.5f, rMax = 11.0f, tiltX = 0.9f, tiltY = 0.35f, spinB = 1.0f, discFrac = 0.2f, sigma = 0.15f;
};

static std::vector<Particle> makeDisc(std::mt19937& rng, uint32_t count, float centreMass, float starMass,
                                      const float origin[3], const float drift[3], float rMin, float rMax,
                                      float tiltX, float tiltY, float spin, float tag, float sigma, float sunR = 0.0f) {
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::normal_distribution<float> jitter(0.0f, 0.3f);
    std::normal_distribution<float> thick(0.0f, 0.5f);
    std::normal_distribution<float> warm(0.0f, 1.0f);
    float cx = std::cos(tiltX), sx = std::sin(tiltX), cy = std::cos(tiltY), sy = std::sin(tiltY);
    auto rotate = [&](float v[3]) {
        float y = v[1] * cx - v[2] * sx, z = v[1] * sx + v[2] * cx;
        float x = v[0] * cy + z * sy;
        z = -v[0] * sy + z * cy;
        v[0] = x; v[1] = y; v[2] = z;
    };
    std::vector<Particle> out;
    out.reserve(count);
    Particle centre{};
    centre.pos[3] = centreMass;
    for (int a = 0; a < 3; a++) { centre.pos[a] = origin[a]; centre.vel[a] = drift[a]; }
    out.push_back(centre);
    for (uint32_t i = 1; i < count; i++) {
        float r = rMin + (rMax - rMin) * std::sqrt(unit(rng));
        if (sunR > 0.0f && i == 1) r = sunR;
        float theta = (unit(rng) < 0.5f ? 0.0f : PI) + 0.28f * r + jitter(rng);
        float discMass = starMass * (float)count * (r * r - rMin * rMin) / (rMax * rMax - rMin * rMin);
        float v = spin * std::sqrt(centreMass * r * r / std::pow(r * r + COLLISION_CENTRE_SOFT2, 1.5f) + discMass / r);
        float vr = sigma * v * warm(rng), vt = v * (1.0f + sigma * warm(rng));
        if (sunR > 0.0f && i == 1) { vr = 0.0f; vt = v; }
        float p[3] = {r * std::cos(theta), thick(rng) * (1.0f + 0.1f * r), r * std::sin(theta)};
        float u[3] = {vr * std::cos(theta) - vt * std::sin(theta), 0.5f * sigma * v * warm(rng), vr * std::sin(theta) + vt * std::cos(theta)};
        rotate(p);
        rotate(u);
        Particle s{};
        for (int a = 0; a < 3; a++) { s.pos[a] = origin[a] + p[a]; s.vel[a] = drift[a] + u[a]; }
        s.pos[3] = starMass;
        s.vel[3] = (sunR > 0.0f && i == 1) ? -0.3f : tag;
        out.push_back(s);
    }
    return out;
}

static std::vector<Particle> makeCollisionWith(const CollisionParams& c, uint32_t n, uint32_t seed) {
    std::vector<Particle> particles;
    if (n == 0) return particles;
    if (n < 4) return makeGalaxy(n, seed);
    std::mt19937 rng(seed);
    uint32_t nA = n / 2, nB = n - nA;
    float starMass = c.discFrac * c.centreMass / (float)(n / 2);
    float total = 2.0f * c.centreMass + starMass * (float)n;
    float vRel = c.vFrac * std::sqrt(2.0f * total / c.sep);
    float oA[3] = {-0.5f * c.sep, 0.0f, -0.5f * c.impact}, dA[3] = {0.5f * vRel, 0.0f, 0.0f};
    float oB[3] = {0.5f * c.sep, 0.0f, 0.5f * c.impact}, dB[3] = {-0.5f * vRel, 0.0f, 0.0f};
    std::vector<Particle> a = makeDisc(rng, nA, c.centreMass, starMass, oA, dA, c.rMin, c.rMax, 0.0f, 0.0f, 1.0f, -0.25f, c.sigma);
    std::vector<Particle> b = makeDisc(rng, nB, c.centreMass, starMass, oB, dB, c.rMin, c.rMax, c.tiltX, c.tiltY, c.spinB, -1.0f, c.sigma);
    particles.reserve(n);
    a[0].vel[3] = -2.0f;
    b[0].vel[3] = -2.0f;
    particles.push_back(a[0]);
    particles.push_back(b[0]);
    particles.insert(particles.end(), a.begin() + 1, a.end());
    particles.insert(particles.end(), b.begin() + 1, b.end());
    return particles;
}

std::vector<Particle> makeCollision(uint32_t n, uint32_t seed) {
    return makeCollisionWith(CollisionParams{}, n, seed);
}

std::vector<Particle> makeAndromeda(uint32_t count, uint32_t seed) {
    std::vector<Particle> particles;
    if (count == 0) return particles;
    if (count < 4) return makeGalaxy(count, seed);
    uint32_t n = std::min(count, ANDROMEDA_N);
    const float sep = 110.0f, impact = 6.0f, vRad = 10.0f, vTan = 8.0f, centreMass = 5000.0f, rMin = 3.5f;
    const float rMaxA = 11.0f, rMaxB = 19.25f, tiltB = 77.0f * PI / 180.0f;
    std::mt19937 rng(seed);
    uint32_t nA = (n * 2) / 5, nB = n - nA;
    float starMass = 0.1f * centreMass / (float)(n / 2);
    float total = 2.0f * centreMass + starMass * (float)n;
    float mA = centreMass + starMass * (float)nA, mB = centreMass + starMass * (float)nB;
    float oA[3] = {-0.5f * sep, 0.0f, -0.5f * impact}, dA[3] = {vRad * mB / total, 0.0f, vTan * mB / total};
    float oB[3] = {0.5f * sep, 0.0f, 0.5f * impact}, dB[3] = {-vRad * mA / total, 0.0f, -vTan * mA / total};
    std::vector<Particle> a = makeDisc(rng, nA, centreMass, starMass, oA, dA, rMin, rMaxA, 0.0f, 0.0f, 1.0f, -0.25f, 0.15f, 0.55f * rMaxA);
    std::vector<Particle> b = makeDisc(rng, nB, centreMass, starMass, oB, dB, rMin, rMaxB, tiltB, 0.35f, 1.0f, -1.0f, 0.15f);
    a[0].vel[3] = -2.0f;
    b[0].vel[3] = -2.0f;
    particles.reserve(n);
    particles.push_back(a[0]);
    particles.push_back(b[0]);
    particles.insert(particles.end(), a.begin() + 1, a.end());
    particles.insert(particles.end(), b.begin() + 1, b.end());
    particles.resize(count, Particle{});
    return particles;
}

std::vector<Particle> makeScene(Scene scene, uint32_t n, uint32_t seed, bool trueScale) {
    if (scene == Scene::GALAXY) return makeGalaxy(n, seed);
    if (scene == Scene::COLLISION) return makeCollision(n, seed);
    if (scene == Scene::ANDROMEDA) return makeAndromeda(n, seed);
    if (scene == Scene::BHSIZES) return makeBhSizes(n);
    return makeSolarSystem(n, seed, trueScale);
}

const char* sceneName(Scene scene) {
    if (scene == Scene::GALAXY) return "galaxy";
    if (scene == Scene::COLLISION) return "galaxy collision";
    if (scene == Scene::ANDROMEDA) return "milky way + andromeda";
    if (scene == Scene::BHSIZES) return "black hole sizes";
    if (scene == Scene::LIFECYCLE) return "star life cycle";
    if (scene == Scene::STARSIZES) return "star sizes";
    if (scene == Scene::SANDBOX) return "sandbox";
    if (scene == Scene::EARTHMOON) return "earth and moon";
    if (scene == Scene::TIMEDIL) return "time dilation";
    if (scene == Scene::LAGRANGE) return "lagrange points";
    if (scene == Scene::LIGHTRIDE) return "ride a light beam";
    if (scene == Scene::FUTURESKY) return "andromeda in our future sky";
    if (scene == Scene::NIGHTSKY) return "night sky";
    if (scene == Scene::IMPACT) return "asteroid impact";
    if (scene == Scene::ECLIPSE) return "eclipses";
    if (scene == Scene::EXPANSION) return "expansion of the universe";
    if (scene == Scene::BHDISC) return "black hole feeding";
    return "solar system";
}

void cpuStep(std::vector<Particle>& particles, const SimParams& params) {
    const float dt = params.dt;
    const float G = params.G;
    const float soft = params.soft;
    const uint32_t n = static_cast<uint32_t>(particles.size());
    if (n == 0) return;

    // Compute accelerations based on current positions (old)
    std::vector<float> ax(n, 0.0f);
    std::vector<float> ay(n, 0.0f);
    std::vector<float> az(n, 0.0f);

    const float soft2 = soft * soft;

    for (uint32_t i = 0; i < n; ++i) {
        const float px_i = particles[i].pos[0];
        const float py_i = particles[i].pos[1];
        const float pz_i = particles[i].pos[2];

        float ax_i = 0.0f;
        float ay_i = 0.0f;
        float az_i = 0.0f;

        for (uint32_t j = 0; j < n; ++j) {
            if (i == j) continue;
            float dx = particles[j].pos[0] - px_i;
            float dy = particles[j].pos[1] - py_i;
            float dz = particles[j].pos[2] - pz_i;
            float dist2 = dx * dx + dy * dy + dz * dz;
            float denom = std::pow(dist2 + soft2, 1.5f);
            float factor = G * particles[j].pos[3] / denom;
            ax_i += factor * dx;
            ay_i += factor * dy;
            az_i += factor * dz;
        }
        ax[i] = ax_i;
        ay[i] = ay_i;
        az[i] = az_i;
    }

    const float kick = params.prevDt < 0.0f ? dt : 0.5f * (params.prevDt + dt);
    for (uint32_t i = 0; i < n; ++i) {
        Particle& p = particles[i];
        p.vel[0] += ax[i] * kick;
        p.vel[1] += ay[i] * kick;
        p.vel[2] += az[i] * kick;

        p.pos[0] += p.vel[0] * dt;
        p.pos[1] += p.vel[1] * dt;
        p.pos[2] += p.vel[2] * dt;
    }
}

constexpr float AU_TRUE = 1.26372f;
constexpr float TRUE_FACTOR = AU_TRUE / 8.0f;

float solarG(bool trueScale) {
    return trueScale ? TRUE_FACTOR * TRUE_FACTOR * TRUE_FACTOR : 1.0f;
}

float solarSoft(bool trueScale) {
    return trueScale ? 0.03f : 0.5f;
}

float solarUnitsPerAu(bool trueScale) {
    return trueScale ? AU_TRUE : 8.0f;
}

static float g_overrideTrue[9], g_overrideEnl[9];
static bool g_overrideRadii = false;

void setBodyRadii(const float* trueR, const float* enlR) {
    for (int i = 0; i < 9; i++) {
        g_overrideTrue[i] = trueR[i];
        g_overrideEnl[i] = enlR[i];
    }
    g_overrideRadii = true;
}

void clearBodyRadii() {
    g_overrideRadii = false;
}

float bodyRadius(bool trueScale, int body) {
    if (g_overrideRadii && body >= 0 && body <= 8) return trueScale ? g_overrideTrue[body] : g_overrideEnl[body];
    static const float enlarged[9] = {2.0f, 0.24f, 0.57f, 0.6f, 0.36f, 1.5f, 1.35f, 0.9f, 0.9f};
    static const float real[9] = {5.877e-3f, 2.061e-5f, 5.112e-5f, 5.382e-5f, 2.863e-5f, 5.906e-4f, 4.919e-4f, 2.142e-4f, 2.080e-4f};
    if (body < 0 || body > 8) return 0.0f;
    return trueScale ? real[body] : enlarged[body];
}

float followDistance(bool trueScale, int body) {
    if (trueScale) return 120.0f * bodyRadius(true, body);
    return body == 0 ? 40.0f : (body == 5 ? 8.0f : 4.0f);
}

static void addOrbiter(std::vector<Particle>& out, float r, float theta, float y, float mass, float gm) {
    Particle p{};
    p.pos[0] = r * std::cos(theta);
    p.pos[1] = y;
    p.pos[2] = r * std::sin(theta);
    p.pos[3] = mass;
    float v = std::sqrt(gm / r);
    p.vel[0] = -v * std::sin(theta);
    p.vel[2] = v * std::cos(theta);
    out.push_back(p);
}

std::vector<Particle> makeSolarSystem(uint32_t n, uint32_t seed, bool trueScale) {
    const float sunMass = 1000.0f;
    const float gm = solarG(trueScale) * sunMass;
    const float au = AU_TRUE;
    const float radius[8] = {4.0f, 6.0f, 8.0f, 10.5f, 21.0f, 27.0f, 33.0f, 38.0f};
    const float realAu[8] = {0.387f, 0.723f, 1.0f, 1.524f, 5.203f, 9.537f, 19.19f, 30.07f};
    const float mass[8] = {0.00017f, 0.0024f, 0.003f, 0.00032f, 0.95f, 0.29f, 0.044f, 0.051f};
    std::vector<Particle> particles;
    particles.reserve(n);
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> angle(0.0f, 2.0f * PI);
    std::uniform_real_distribution<float> tilt(-0.3f, 0.3f);

    Particle sun{};
    sun.pos[3] = sunMass;
    particles.push_back(sun);
    const double jd = solarEpochJd();
    for (int i = 0; i < 8 && particles.size() < n; i++) {
        OrbitState o = ephemPlanet(i, jd, trueScale ? realAu[i] * au : radius[i], gm);
        Particle p{};
        for (int a = 0; a < 3; a++) {
            p.pos[a] = (float)o.pos[a];
            p.vel[a] = (float)o.vel[a];
        }
        p.pos[3] = mass[i];
        particles.push_back(p);
    }

    const int moonParent[5] = {3, 5, 5, 5, 5};
    const float moonEnlarged[5] = {1.2f, 2.1f, 2.7f, 3.4f, 4.4f};
    const float moonReal[5] = {0.003247f, 0.003562f, 0.005669f, 0.009042f, 0.015904f};
    const float moonPeriod[5] = {1.5f, 1.0f, 2.0f, 4.0f, 9.4f};
    for (int i = 0; i < 5 && particles.size() < n && (size_t)moonParent[i] < particles.size(); i++) {
        const Particle& parent = particles[moonParent[i]];
        float mr = trueScale ? moonReal[i] : moonEnlarged[i];
        float a = i == 0 ? (float)ephemMoonLon(jd) : angle(rng);
        Particle m{};
        m.pos[0] = parent.pos[0] + mr * std::cos(a);
        m.pos[1] = parent.pos[1];
        m.pos[2] = parent.pos[2] + mr * std::sin(a);
        m.pos[3] = 0.000001f;
        m.vel[0] = mr;
        m.vel[1] = a;
        m.vel[2] = 2.0f * PI / moonPeriod[i];
        m.vel[3] = (float)(moonParent[i] + 1);
        particles.push_back(m);
    }

    if (particles.size() < n) {
        OrbitState o = ephemHalley(jd, gm, trueScale, au);
        Particle comet{};
        for (int a = 0; a < 3; a++) {
            comet.pos[a] = (float)o.pos[a];
            comet.vel[a] = (float)o.vel[a];
        }
        comet.pos[3] = 0.000005f;
        particles.push_back(comet);
    }

    const float ringLo = trueScale ? 6.3e-4f : 1.7f, ringHi = trueScale ? 1.15e-3f : 3.1f;
    const float ringRef = 0.5f * (ringLo + ringHi);
    std::uniform_real_distribution<float> ringRadius(ringLo, ringHi);
    for (int i = 0; i < 600 && particles.size() < n && particles.size() > 6; i++) {
        const Particle& saturn = particles[6];
        float r = ringRadius(rng);
        float a = angle(rng);
        Particle ring{};
        ring.pos[0] = saturn.pos[0] + r * std::cos(a);
        ring.pos[1] = saturn.pos[1];
        ring.pos[2] = saturn.pos[2] + r * std::sin(a);
        ring.pos[3] = 0.0000003f;
        ring.vel[0] = r;
        ring.vel[1] = a;
        ring.vel[2] = 2.0f * PI / (1.2f * std::pow(r / ringRef, 1.5f));
        ring.vel[3] = 7.0f;
        particles.push_back(ring);
    }

    uint32_t rocks = n > particles.size() ? n - static_cast<uint32_t>(particles.size()) : 0;
    uint32_t inner = rocks * 2 / 5;
    std::uniform_real_distribution<float> asteroid(trueScale ? 2.2f * au : 13.0f, trueScale ? 3.2f * au : 17.0f);
    std::uniform_real_distribution<float> kuiper(trueScale ? 40.0f : 42.0f, trueScale ? 62.0f : 50.0f);
    const float tiltScale = trueScale ? 0.2f : 1.0f;
    for (uint32_t i = 0; i < rocks; i++) {
        float r = i < inner ? asteroid(rng) : kuiper(rng);
        addOrbiter(particles, r, angle(rng), tilt(rng) * tiltScale, 0.000001f, gm);
    }

    double momentum[3] = {0.0, 0.0, 0.0}, moment[3] = {0.0, 0.0, 0.0}, total = 0.0;
    for (const Particle& p : particles) {
        if (p.vel[3] > 0.5f) continue;
        total += p.pos[3];
        for (int a = 0; a < 3; a++) {
            momentum[a] += (double)p.pos[3] * p.vel[a];
            moment[a] += (double)p.pos[3] * p.pos[a];
        }
    }
    for (int a = 0; a < 3; a++) particles[0].vel[a] -= (float)(momentum[a] / sunMass);
    for (Particle& p : particles)
        for (int a = 0; a < 3; a++) p.pos[a] -= (float)(moment[a] / total);
    return particles;
}
