#include "vk_app.h"
#include "bodydata.h"
#include "solarx.h"
#include <cmath>
#include <cstdio>
#include <string>

namespace {

const float WHITE[3] = {1.0f, 1.0f, 1.0f};
const float TEXT[3] = {0.85f, 0.87f, 0.92f};
const float HINT[3] = {0.6f, 0.65f, 0.75f};
const float GREEN[3] = {0.4f, 1.0f, 0.55f};
const float FACT[3] = {1.0f, 0.78f, 0.45f};
const double AU_UNITS_TRUE = 1.26372;

std::string fix(double v, int decimals) {
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

std::string commas(long long v) {
    std::string s = std::to_string(v < 0 ? -v : v);
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ",");
    return v < 0 ? "-" + s : s;
}

std::string sci(double v) {
    int e = (int)std::floor(std::log10(v));
    return fix(v / std::pow(10.0, e), 2) + "E" + std::to_string(e);
}

std::string num(double v) {
    if (v >= 1000.0) return commas((long long)std::llround(v));
    if (v >= 10.0) return fix(v, 1);
    if (v >= 0.1) return fix(v, 2);
    if (v >= 0.001) return fix(v, 4);
    return sci(v);
}

std::string span(double days) {
    if (days < 700.0) return fix(days, days < 100.0 ? 2 : 1) + " DAYS" + (days >= 300.0 ? " (" + fix(days / DAYS_PER_YEAR, 2) + " YR)" : "");
    double yr = days / DAYS_PER_YEAR;
    if (yr >= 1000.0) return commas((long long)std::llround(yr)) + " YEARS";
    return fix(yr, 1) + " YEARS";
}

std::string dayText(double hours) {
    double h = std::fabs(hours);
    std::string t = h < 48.0 ? fix(h, 1) + " HOURS" : fix(h / 24.0, 1) + " DAYS";
    return hours < 0.0 ? t + " RETROGRADE" : t;
}

std::string kelvin(double k) {
    return fix(k, 0) + " K (" + fix(k - 273.15, 0) + " C)";
}

std::string gravText(double g) {
    return num(g) + " M/S2" + (g >= 0.5 ? " (" + fix(g / EARTH_G, 2) + " G)" : "");
}

void wrap(std::vector<CardLine>& lines, const std::string& text, size_t width, const float c[3]) {
    std::string cur;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t sp = text.find(' ', pos);
        std::string word = text.substr(pos, sp == std::string::npos ? std::string::npos : sp - pos);
        pos = sp == std::string::npos ? text.size() : sp + 1;
        if (!cur.empty() && cur.size() + 1 + word.size() > width) {
            lines.push_back({cur, {c[0], c[1], c[2]}});
            cur.clear();
        }
        cur += (cur.empty() ? "" : " ") + word;
    }
    if (!cur.empty()) lines.push_back({cur, {c[0], c[1], c[2]}});
}

void add(std::vector<CardLine>& lines, const std::string& text, const float c[3]) {
    lines.push_back({text, {c[0], c[1], c[2]}});
}

bool live(const AppContext& app, int i, double auPerUnit, double& au, double& kms) {
    if (!app.displayValid || i <= 0 || (uint32_t)i >= BODIES || (uint32_t)i >= app.n) return false;
    const Particle& s = app.displayBodies[0];
    const Particle& p = app.displayBodies[i];
    double r = 0.0, v = 0.0;
    for (int a = 0; a < 3; a++) {
        double d = (double)p.pos[a] - s.pos[a], w = (double)p.vel[a] - s.vel[a];
        r += d * d;
        v += w * w;
    }
    r = std::sqrt(r);
    v = std::sqrt(v);
    if (r < 1e-9 || auPerUnit <= 0.0) return false;
    au = r * auPerUnit;
    double vc = std::sqrt((double)app.simG * s.pos[3] / r);
    double mass = app.visit.active ? app.visit.sys.mass : 1.0;
    kms = (v / vc) * EARTH_ORBIT_KMS * std::sqrt(mass / au);
    return true;
}

void liveLines(std::vector<CardLine>& lines, bool ok, double au, double kms, const char* from) {
    if (!ok) return;
    add(lines, std::string(from) + " " + fix(au, au < 10.0 ? 2 : 1) + " AU", GREEN);
    add(lines, "SPEED " + fix(kms, 1) + " KM/S", GREEN);
}

std::vector<CardLine> homeCard(const AppContext& app, int i) {
    std::vector<CardLine> lines;
    const BodyInfo& b = BODY_INFO[i];
    const float* col = app.bodyColors[i];
    bool moon = i >= 9 && i <= 13, sun = i == 0;
    add(lines, b.name, WHITE);
    add(lines, b.type, col);
    add(lines, "MASS " + num(b.massKg / EARTH_MASS_KG) + " EARTH = " + sci(b.massKg) + " KG", TEXT);
    add(lines, "RADIUS " + num(b.radiusKm) + " KM (" + num(b.radiusKm / EARTH_RADIUS_KM) + " EARTH)", TEXT);
    if (!moon && !sun) {
        double s = AU_UNITS_TRUE;
        if (!app.trueScale && i <= 8) s = HOME_ENLARGED_RADIUS[i - 1] / HOME_REAL_AU[i - 1];
        double au = 0.0, kms = 0.0;
        bool ok = live(app, i, 1.0 / s, au, kms);
        liveLines(lines, ok, au, kms, "SUN DISTANCE");
    }
    if (sun) add(lines, "GALAXY ORBIT 230 KM/S, ~230 MYR", TEXT);
    else add(lines, std::string(moon ? "ORBIT " : "YEAR ") + span(b.periodDays), TEXT);
    add(lines, "DAY " + dayText(b.dayHours), TEXT);
    if (sun) add(lines, "PLANETS 8", TEXT);
    else if (!moon && i != 14) add(lines, "MOONS " + std::to_string(b.moons), TEXT);
    if (b.tiltDeg >= 0.0) add(lines, "AXIAL TILT " + fix(b.tiltDeg, b.tiltDeg < 1.0 ? 2 : 1) + " DEG", TEXT);
    add(lines, "GRAVITY " + gravText(b.gravity), TEXT);
    if (b.tempK > 0.0) add(lines, std::string(sun ? "SURFACE " : "TEMP ") + kelvin(b.tempK), TEXT);
    if (i == 14) solarxHalleyCard(app, lines);
    wrap(lines, std::string("FACT: ") + b.fact, 32, FACT);
    add(lines, "ESC = FREE-FLY", HINT);
    return lines;
}

const char* planetType(int t) {
    switch (t) {
        case 2: return "HOT ROCKY PLANET";
        case 3: return "ROCKY PLANET, EARTH-LIKE";
        case 4: return "COLD ROCKY PLANET";
        case 9: return "BARREN ROCKY PLANET";
        case 10: return "GAS GIANT";
        default: return "ICE GIANT";
    }
}

const char* planetFact(int t) {
    switch (t) {
        case 2: return "SCORCHED BY ITS STAR, ITS SURFACE MAY BE MOLTEN ROCK.";
        case 3: return "LIQUID WATER COULD EXIST HERE - A GOOD PLACE TO LOOK FOR LIFE.";
        case 4: return "FROZEN FAR FROM ITS STAR, WITH A THIN OR NO ATMOSPHERE.";
        case 9: return "A BARREN WORLD, PROBABLY AIRLESS AND HEAVILY CRATERED.";
        case 10: return "A GIANT OF HYDROGEN AND HELIUM WITH NO SOLID SURFACE.";
        default: return "A COLD GIANT OF WATER, AMMONIA AND METHANE ICES.";
    }
}

double systemAuPerUnit(const AppContext& app, int i) {
    if (app.trueScale) return 1.0 / AU_UNITS_TRUE;
    if (!app.displayValid) return 0.0;
    const Particle& st = app.displayBodies[0];
    const Particle& pl = app.displayBodies[i];
    double r = 0.0;
    for (int a = 0; a < 3; a++) r += ((double)pl.pos[a] - st.pos[a]) * ((double)pl.pos[a] - st.pos[a]);
    r = std::sqrt(r);
    return r > 1e-9 ? (double)app.visit.sys.au0 * std::exp((r - 4.0) / 6.2) / r : 0.0;
}

std::vector<CardLine> systemCard(const AppContext& app, int i) {
    std::vector<CardLine> lines;
    const StarSystem& s = app.visit.sys;
    double au = 0.0, kms = 0.0;
    if (i == 14) {
        add(lines, "COMET", WHITE);
        add(lines, "ICY WANDERER", app.bodyColors[i]);
        liveLines(lines, live(app, i, systemAuPerUnit(app, i), au, kms), au, kms, "STAR DISTANCE");
        add(lines, "ESC = FREE-FLY", HINT);
        return lines;
    }
    if (i < 1 || i > s.planetCount) return lines;
    const PlanetSpec& p = s.planets[i - 1];
    add(lines, app.bodyNames[i], WHITE);
    add(lines, planetType(p.type), p.tint);
    if (s.real >= 0) add(lines, "REAL EXOPLANET", GREEN);
    if (p.habitable) add(lines, "POSSIBLY HABITABLE", GREEN);
    double earths = p.mass / 0.003, re = p.radiusKm / EARTH_RADIUS_KM;
    add(lines, "MASS " + num(earths) + " EARTH = " + sci(earths * EARTH_MASS_KG) + " KG", TEXT);
    add(lines, "RADIUS " + num(p.radiusKm) + " KM (" + num(re) + " EARTH)", TEXT);
    if (live(app, i, systemAuPerUnit(app, i), au, kms)) liveLines(lines, true, au, kms, "STAR DISTANCE");
    else add(lines, "STAR DISTANCE " + fix(p.au, 2) + " AU", TEXT);
    add(lines, "YEAR " + span(DAYS_PER_YEAR * std::sqrt((double)p.au * p.au * p.au / s.mass)), TEXT);
    add(lines, "DAY " + dayText(p.day * 24.0), TEXT);
    int moons = 0;
    for (int k = 0; k < s.moonCount; k++) moons += s.moons[k].parent == i ? 1 : 0;
    add(lines, "MOONS " + std::to_string(moons), TEXT);
    add(lines, "AXIAL TILT " + fix(p.tilt, 1) + " DEG", TEXT);
    add(lines, "GRAVITY " + gravText(EARTH_G * earths / (re * re)), TEXT);
    add(lines, "TEMP " + kelvin(255.0 * std::pow((double)s.lum, 0.25) / std::sqrt((double)p.au)), TEXT);
    wrap(lines, std::string("FACT: ") + planetFact(p.type), 32, FACT);
    add(lines, "ESC = FREE-FLY", HINT);
    return lines;
}

}

std::vector<CardLine> bodyCard(const AppContext& app) {
    if (g_sx.sel >= 0 && app.scene == Scene::SOLAR && app.visit.mode == 0 && !app.visit.active && !app.gal.active) return solarxCard(app);
    int i = app.follow;
    if (app.scene != Scene::SOLAR || i < 0 || i >= (int)BODIES || app.visit.mode != 0) return {};
    if (app.visit.active) return i == 0 ? std::vector<CardLine>{} : systemCard(app, i);
    if (app.gal.active) return {};
    return homeCard(app, i);
}
