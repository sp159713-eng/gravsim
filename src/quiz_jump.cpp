#include "quiz.h"
#include "solarx.h"
#include "lightride.h"
#include "lifecycle.h"
#include "closeup.h"
#include <cstring>
#include <string>

extern bool g_sxNosunDemo;
extern std::string g_closeupName;
extern int g_ecView;
extern float g_ecPhase;
extern bool g_ecApogee;
extern int g_ecSeason;
extern int g_imPreset;
extern float g_imAt;
extern bool g_lgJupiter;
extern bool g_nsLookSet;
extern float g_nsLook[2];
extern float g_nsFov;
extern std::string g_nsHover;
extern float g_tdRadius;
extern bool g_tdMiller;
extern double g_fsStartAt;

namespace {

struct Pending {
    bool on = false;
    QuizShow s{};
    int wait = 0;
};

Pending g_p;
float g_tourAt = 0.0f;
bool g_tourHold = false;

float tourStopStart(int stop) {
    float t = 0.0f;
    for (int k = 0; k < stop; k++) {
        float hold = k == 21 ? 15.0f : (k == 16 || k == 17) ? 8.5f : k == 20 ? 5.5f : 4.0f;
        float trans = k == 20 ? 4.5f : 5.0f;
        t += hold + trans;
    }
    return t;
}

void toScene(AppContext& app, Scene sc) {
    if (app.tour.mode != 0) tourStop(app);
    app.cu.mode = 0;
    g_closeupName.clear();
    g_p.on = false;
    g_tourHold = false;
    switchScene(app, sc);
}

void applySolarCam(AppContext& app, const QuizShow& s) {
    float dist = s.z * solarUnitsPerAu(true);
    if (s.s && s.s[0]) {
        solarxCamNow(app, s.s, s.z > 0.0f, s.x, s.y, dist);
        return;
    }
    if (s.a >= 0) {
        enterFollow(app, s.a, app.displayBodies[s.a].pos);
        if (s.z > 0.0f) {
            app.camera.yaw = s.x;
            app.camera.pitch = s.y;
            app.camera.dist = dist;
        }
        return;
    }
    solarxCamNow(app, "", true, s.x, s.y, dist);
}

void lightAt(AppContext& app, const QuizShow& s) {
    LightRideState& S = app.lr;
    double t = s.x;
    if (s.s && s.s[0]) {
        size_t n = std::strlen(s.s);
        for (const LrMilestone& m : S.ms)
            if (m.head.compare(0, n, s.s) == 0) {
                t = m.t;
                break;
            }
    }
    S.t = t;
    S.frozen = true;
    S.backAmt = 0.0f;
    S.realT = t > 0.0 ? 60.0 : 0.0;
    for (int i = 0; i < (int)S.ms.size(); i++)
        if (S.ms[i].t <= S.t + 2.0) {
            S.capIdx = i;
            S.capAge = 0.6;
        }
    lrPose(S, S.t);
}

void lifeAt(AppContext& app, const QuizShow& s) {
    app.showSky = false;
    app.life.mass = s.a;
    lifeRestart(app);
    app.life.mode = 2;
    std::vector<int> seq = lifeSequence(app.life);
    float at = 0.0f;
    for (int id : seq) {
        if (id == s.b) {
            app.life.t = at + 0.1f * STAGES[id].secs;
            break;
        }
        at += STAGES[id].secs;
    }
}

}

float quizTourAt() {
    return g_tourAt;
}

void quizJump(AppContext& app, const QuizShow& s, int& flags) {
    flags |= 1;
    switch (s.kind) {
    case QK_SCENE:
        toScene(app, (Scene)s.a);
        if (s.a == (int)Scene::BHSIZES && s.x > 0.0f) {
            app.bhTour.t = s.x;
            app.bhTour.mode = 2;
        }
        break;
    case QK_SOLAR:
        app.trueScale = true;
        toScene(app, Scene::SOLAR);
        g_p = {true, s, 0};
        break;
    case QK_NOSUN:
        app.trueScale = true;
        toScene(app, Scene::SOLAR);
        g_sxNosunDemo = true;
        break;
    case QK_TOUR:
        toScene(app, Scene::SOLAR);
        g_tourAt = tourStopStart(s.a) + s.x;
        g_tourHold = true;
        flags |= 4;
        break;
    case QK_STARS:
        toScene(app, Scene::STARSIZES);
        starsSetStop(app, s.a, 2.0f);
        app.stars.mode = 2;
        break;
    case QK_LIGHT:
        toScene(app, Scene::LIGHTRIDE);
        lightAt(app, s);
        break;
    case QK_ECLIPSE:
        g_ecView = s.a;
        g_ecPhase = s.x;
        g_ecApogee = s.b != 0;
        g_ecSeason = s.c;
        toScene(app, Scene::ECLIPSE);
        break;
    case QK_IMPACT:
        g_imPreset = s.a;
        g_imAt = s.x;
        toScene(app, Scene::IMPACT);
        break;
    case QK_LAGRANGE:
        g_lgJupiter = s.a != 0;
        toScene(app, Scene::LAGRANGE);
        app.lg.selected = s.b;
        break;
    case QK_NIGHT:
        g_nsLookSet = true;
        g_nsLook[0] = s.x;
        g_nsLook[1] = s.y;
        g_nsFov = s.z;
        g_nsHover = s.s ? s.s : "";
        toScene(app, Scene::NIGHTSKY);
        break;
    case QK_TIMEDIL:
        g_tdRadius = s.x;
        g_tdMiller = s.a != 0;
        toScene(app, Scene::TIMEDIL);
        break;
    case QK_EARTHMOON:
        toScene(app, Scene::EARTHMOON);
        app.em.days = s.x;
        break;
    case QK_CLOSEUP:
        toScene(app, Scene::SOLAR);
        g_closeupName = s.s;
        break;
    case QK_FUTURE:
        g_fsStartAt = s.x;
        toScene(app, Scene::FUTURESKY);
        flags |= 2;
        break;
    case QK_LIFE:
        toScene(app, Scene::LIFECYCLE);
        lifeAt(app, s);
        break;
    }
}

void quizJumpCancel() {
    g_p.on = false;
    g_tourHold = false;
}

void quizJumpTick(AppContext& app) {
    if (g_tourHold && app.tour.mode == 1 && app.tour.t >= g_tourAt) {
        g_tourHold = false;
        tourToggle(app);
    }
    if (!g_p.on) return;
    if (app.scene != Scene::SOLAR) {
        g_p.on = false;
        return;
    }
    if (!app.displayValid) return;
    if (++g_p.wait < 12) return;
    g_p.on = false;
    applySolarCam(app, g_p.s);
}
