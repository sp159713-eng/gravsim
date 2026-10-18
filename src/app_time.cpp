#include "vk_app.h"
#include "bodydata.h"
#include "ephem.h"
#include "solarx.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

namespace {

AppContext* g_app = nullptr;

const double LADDER[] = {0.25, 0.5, 1.0, 2.0, 4.0, 7.0, 14.0, 30.44, 60.88, 121.75, 182.63, 365.25, 730.5, 1826.25};
const int LADDER_COUNT = (int)(sizeof(LADDER) / sizeof(LADDER[0]));
const double PRESETS[] = {1.0, 7.0, 30.44, 365.25};
const double YEAR_UNITS = 6.283185307179586 * 0.7155417527999327;
const double MAX_JUMP_YEARS = 2000.0;
const float STEP_DT = 0.01f;
const uint32_t LITE_N = BODIES + 600;

long long daysFromCivil(long long y, int m, int d) {
    y -= m <= 2 ? 1 : 0;
    long long era = (y >= 0 ? y : y - 399) / 400;
    long long yoe = y - era * 400;
    long long doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    long long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

void civilFromDays(long long z, long long& y, int& m, int& d) {
    z += 719468;
    long long era = (z >= 0 ? z : z - 146096) / 146097;
    long long doe = z - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    y = yoe + era * 400;
    long long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long long mp = (5 * doy + 2) / 153;
    d = (int)(doy - (153 * mp + 2) / 5 + 1);
    m = (int)(mp < 10 ? mp + 3 : mp - 9);
    y += m <= 2 ? 1 : 0;
}

bool leap(long long y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

int monthDays(long long y, int m) {
    static const int D[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return m == 2 && leap(y) ? 29 : D[m - 1];
}

std::string dateOf(const AppContext& app, double simTime) {
    long long days = app.tm.launchDay + (long long)std::floor(simTime / YEAR_UNITS * DAYS_PER_YEAR);
    long long y;
    int m, d;
    civilFromDays(days, y, m, d);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04lld-%02d-%02d", y, m, d);
    return buf;
}

bool homeView(const AppContext& app) {
    return app.scene == Scene::SOLAR && !app.visit.active && !app.gal.active && app.tour.mode == 0 && app.visit.mode == 0;
}

void finishJump(AppContext& app) {
    TimeState& t = app.tm;
    t.jumping = false;
    t.pauseNow = true;
    for (auto& trail : app.trails) trail.clear();
}

void startJump(AppContext& app) {
    TimeState& t = app.tm;
    int parts[3] = {0, 1, 1};
    int count = 0;
    size_t pos = 0;
    bool ok = !t.buf.empty();
    while (ok) {
        size_t dash = t.buf.find('-', pos);
        std::string piece = t.buf.substr(pos, dash == std::string::npos ? std::string::npos : dash - pos);
        if (piece.empty() || piece.size() > 4 || count >= 3) { ok = false; break; }
        parts[count++] = std::atoi(piece.c_str());
        if (dash == std::string::npos) break;
        pos = dash + 1;
    }
    long long y = parts[0];
    int m = parts[1], d = parts[2];
    if (!ok || m < 1 || m > 12 || d < 1 || d > monthDays(y, m)) {
        t.msg = "INVALID DATE";
        t.msgT = 2.5f;
        return;
    }
    long long targetDay = daysFromCivil(y, m, d);
    double deltaDays = (double)(targetDay - t.todayDay);
    if (std::fabs(deltaDays) > MAX_JUMP_YEARS * DAYS_PER_YEAR) {
        t.msg = "OUT OF RANGE - LIMIT 2000 YEARS";
        t.msgT = 2.5f;
        return;
    }
    char label[32];
    std::snprintf(label, sizeof(label), "%04lld-%02d-%02d", y, m, d);
    t.jumpLabel = label;
    bool shifted = t.launchDay != t.todayDay;
    t.target = (deltaDays < 0.0 ? 0.5 : deltaDays + 0.5) / DAYS_PER_YEAR * YEAR_UNITS;
    t.jumping = true;
    t.needReset = deltaDays < 0.0 || shifted || t.target < app.simTime;
    t.launchDay = deltaDays < 0.0 ? targetDay : t.todayDay;
    t.start = t.needReset ? 0.0 : app.simTime;
    t.budget = 48;
}

}

void timeAttach(AppContext* app) {
    g_app = app;
}

void timeInit(AppContext& app) {
    std::time_t now = std::time(nullptr);
    std::tm local = *std::localtime(&now);
    app.tm.launchDay = app.tm.todayDay = daysFromCivil(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
    solarSetEpochDay(app.tm.launchDay);
}

bool timeTyping() {
    return g_app && g_app->tm.typing;
}

bool timeKeyHook(int key, int action) {
    if (!g_app) return false;
    AppContext& app = *g_app;
    TimeState& t = app.tm;
    if (t.typing) {
        if (action == GLFW_RELEASE) return true;
        bool digit = (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) || (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9);
        if (digit && t.buf.size() < 10) t.buf += (char)('0' + (key >= GLFW_KEY_KP_0 ? key - GLFW_KEY_KP_0 : key - GLFW_KEY_0));
        else if ((key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT) && t.buf.size() < 10) t.buf += '-';
        else if (key == GLFW_KEY_BACKSPACE && !t.buf.empty()) t.buf.pop_back();
        else if (key == GLFW_KEY_ESCAPE) t.typing = false;
        else if ((key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) && action == GLFW_PRESS) {
            t.typing = false;
            startJump(app);
        }
        return true;
    }
    if (action != GLFW_PRESS) return false;
    if (key == GLFW_KEY_ESCAPE && t.jumping) {
        t.jumping = false;
        t.pauseNow = true;
        return false;
    }
    if (!homeView(app)) return false;
    if (key == GLFW_KEY_J && !t.jumping) {
        t.typing = true;
        t.buf.clear();
        return true;
    }
    if (key == GLFW_KEY_COMMA) timePreset(app, -1);
    if (key == GLFW_KEY_PERIOD) timePreset(app, 1);
    return false;
}

void timeRate(AppContext& app, int dir) {
    app.tm.rate = std::max(0, std::min(LADDER_COUNT - 1, app.tm.rate + dir));
}

void timePreset(AppContext& app, int dir) {
    double cur = LADDER[app.tm.rate];
    double pick = dir > 0 ? PRESETS[3] : PRESETS[0];
    if (dir > 0) {
        for (double p : PRESETS) if (p > cur + 1e-6) { pick = p; break; }
    } else {
        for (int k = 3; k >= 0; k--) if (PRESETS[k] < cur - 1e-6) { pick = PRESETS[k]; break; }
    }
    for (int k = 0; k < LADDER_COUNT; k++) if (std::fabs(LADDER[k] - pick) < 1e-6) app.tm.rate = k;
}

std::string timeRateLabel(const AppContext& app) {
    double r = LADDER[app.tm.rate];
    char buf[32];
    if (r < 1.0) std::snprintf(buf, sizeof(buf), "X %.2f DAYS/S", r);
    else if (r == 1.0) std::snprintf(buf, sizeof(buf), "X 1 DAY/S");
    else std::snprintf(buf, sizeof(buf), "X %.0f DAYS/S", r);
    return buf;
}

void timePlan(AppContext& app, float frameDt, bool paused, uint32_t& steps, float& dt) {
    TimeState& t = app.tm;
    t.paused = paused;
    frameDt = std::max(0.001f, std::min(0.05f, frameDt));
    if (t.msgT > 0.0f) t.msgT -= frameDt;
    steps = 0;
    dt = STEP_DT;
    t.simN = LADDER[t.rate] > 200.0 ? LITE_N : 0;
    if (t.jumping) {
        double rem = t.target - app.simTime;
        if (rem <= 1e-7) {
            finishJump(app);
            return;
        } else {
            if (frameDt < 0.028f && t.budget < 20000) t.budget = t.budget * 5 / 4 + 1;
            else if (frameDt > 0.045f && t.budget > 8) t.budget = t.budget * 4 / 5;
            uint32_t need = (uint32_t)std::ceil(rem / STEP_DT);
            steps = std::min(need, t.budget);
            if (steps == need) dt = (float)(rem / steps);
            t.simN = steps == need ? LITE_N : BODIES;
            return;
        }
    }
    if (paused) return;
    double units = solarxUnits(app, LADDER[t.rate] / DAYS_PER_YEAR * YEAR_UNITS * frameDt, frameDt);
    steps = std::max<uint32_t>(1, (uint32_t)std::ceil(units / STEP_DT));
    dt = (float)(units / steps);
}

void emitTimeText(AppContext& app, OverlayVertex* out, uint32_t& used, float px, float py) {
    TimeState& t = app.tm;
    if (!homeView(app) || !app.displayValid || app.refDist > 2000.0f) return;
    float W = (float)app.swapchainExtent.width;
    int big = W >= 1000.0f ? 3 : 2, mid = W >= 1000.0f ? 2 : 1;
    float y = 14.0f;
    bigText(out, used, dateOf(app, app.simTime).c_str(), W, y, px, py, big, 1.0f, 1.0f, 1.0f);
    y += 7.0f * big + 8.0f;
    std::string rate = t.jumping ? "TIME TRAVEL" : t.paused ? "PAUSED" : timeRateLabel(app);
    bigText(out, used, rate.c_str(), W, y, px, py, mid, 0.55f, 0.85f, 1.0f);
    y += 7.0f * mid + 8.0f;
    if (t.jumping) {
        double span = std::max(1e-9, std::fabs(t.target - t.start));
        double frac = std::max(0.0, std::min(1.0, std::fabs(app.simTime - t.start) / span));
        int fill = (int)std::lround(frac * 24.0);
        std::string bar(std::string((size_t)fill, '#') + std::string((size_t)(24 - fill), '-'));
        bigText(out, used, ("JUMPING TO " + t.jumpLabel).c_str(), W, y, px, py, mid, 1.0f, 0.85f, 0.45f);
        y += 7.0f * mid + 6.0f;
        bigText(out, used, (bar + " " + std::to_string((int)std::lround(frac * 100.0)) + " PCT").c_str(), W, y, px, py, mid, 1.0f, 0.85f, 0.45f);
        y += 7.0f * mid + 6.0f;
        bigText(out, used, "ESC CANCELS", W, y, px, py, 1, 0.6f, 0.65f, 0.75f);
    } else if (t.typing) {
        bigText(out, used, ("JUMP TO DATE: " + t.buf + "#").c_str(), W, y, px, py, mid, 1.0f, 0.85f, 0.45f);
        y += 7.0f * mid + 6.0f;
        bigText(out, used, "TYPE YYYY OR YYYY-MM-DD  ENTER = GO  ESC = CANCEL", W, y, px, py, 1, 0.6f, 0.65f, 0.75f);
    } else if (t.msgT > 0.0f) {
        bigText(out, used, t.msg.c_str(), W, y, px, py, mid, 1.0f, 0.45f, 0.4f);
    }
}
