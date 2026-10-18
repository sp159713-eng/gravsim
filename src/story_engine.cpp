#include "story.h"
#include "vk_app.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#undef glfwGetKey

StoryState g_story;
int g_storyBoot = 0;
int g_storyAtCh = 0;
int g_storyAtObj = 0;
int g_storyUi = 0;
bool g_storyMute = false;
int g_storyScreen = 0;
int g_storySel = 0;

namespace {

bool g_pending = false;
int g_pendCh = 0, g_pendObj = 0;
int g_spaceCount = 0;
int g_uiShown = 0;
double g_clock = -1.0;
bool g_keyPrev[512] = {};

void runActions(AppContext& app, const std::vector<StoryAction>& v, int& flags) {
    for (const StoryAction& a : v) {
        if (a.type == ACT_SCENE) {
            if (app.scene != (Scene)a.show.a) quizJump(app, a.show, flags);
        } else if (a.type == ACT_LOG) {
            pdaPush(a.s);
        } else if (a.type == ACT_UNLOCK) {
            storyUnlock(a.s);
        } else if (a.fn) {
            a.fn(app);
        }
    }
}

const Chapter& chapter() {
    return storyChapters()[g_story.ch];
}

void markSeen(const std::vector<StoryAction>& v) {
    for (const StoryAction& a : v)
        if (a.type == ACT_LOG) pdaRecordSeen(a.s);
}

bool cameraMoved(AppContext& app) {
    const Camera& c = app.camera;
    StoryState& G = g_story;
    if (!G.camSnapped) {
        G.camSnap[0] = c.yaw;
        G.camSnap[1] = c.pitch;
        G.camSnap[2] = c.dist;
        G.camSnap[3] = c.target[0];
        G.camSnap[4] = c.target[2];
        G.camSnapped = true;
        return false;
    }
    double pan = std::hypot(c.target[0] - G.camSnap[3], c.target[2] - G.camSnap[4]);
    return std::fabs(c.yaw - G.camSnap[0]) > 0.05 || std::fabs(c.pitch - G.camSnap[1]) > 0.05 ||
           std::fabs(c.dist / std::max(1e-6f, G.camSnap[2]) - 1.0) > 0.06 || pan > 1.0;
}

bool reachedDistance(AppContext& app, float need) {
    StoryState& G = g_story;
    float eye[3];
    cameraEye(app.camera, eye);
    if (!G.eyeSnapped) {
        for (int i = 0; i < 3; i++) G.eyeSnap[i] = eye[i];
        G.eyeSnapped = true;
        return false;
    }
    double d = std::sqrt(std::pow(eye[0] - G.eyeSnap[0], 2) + std::pow(eye[1] - G.eyeSnap[1], 2) + std::pow(eye[2] - G.eyeSnap[2], 2));
    return d >= need;
}

void startObjective(AppContext& app, int& flags) {
    StoryState& G = g_story;
    G.objT = 0.0f;
    G.camSnapped = false;
    G.eyeSnapped = false;
    if (G.obj < (int)chapter().objs.size()) runActions(app, chapter().objs[G.obj].onStart, flags);
}

void completeObjective(AppContext& app, int& flags) {
    StoryState& G = g_story;
    runActions(app, chapter().objs[G.obj].onDone, flags);
    G.doneToast = 3.0f;
    G.obj++;
    if (G.obj >= (int)chapter().objs.size()) G.complete = true;
    else startObjective(app, flags);
    storySaveFile();
}

bool objectiveDone(AppContext& app) {
    StoryState& G = g_story;
    const Objective& O = chapter().objs[G.obj];
    bool done = false;
    switch (O.type) {
    case OBJ_WAIT:
        done = G.objT >= O.p || ((O.flags & OF_CAMERA_SKIP) && cameraMoved(app));
        break;
    case OBJ_SCENE:
        done = (int)app.scene == O.id;
        break;
    case OBJ_SCAN:
        for (int s : G.scanned)
            if (s == O.id) done = true;
        break;
    case OBJ_DIST:
        done = reachedDistance(app, O.p);
        break;
    case OBJ_CUSTOM:
        done = O.check && O.check(app, G.objT);
        break;
    }
    if (done && (O.flags & OF_WAIT_PDA) && pdaBusy()) done = false;
    return done;
}

}

const StoryModeCfg& storyMode() {
    static StoryModeCfg cfg[4] = {
        {GM_SURVIVAL, true, true, true, false, false, 1.0f, 1.0f},
        {GM_CREATIVE, false, false, false, false, true, 0.0f, 0.0f},
        {GM_HARDCORE, true, true, true, true, false, 1.6f, 1.8f},
        {GM_FREEDOM, true, true, false, false, false, 1.0f, 1.0f},
    };
    int m = g_story.gameMode;
    return cfg[m < 0 || m > 3 ? 0 : m];
}

const char* storyModeName(int m) {
    const char* n[4] = {"SURVIVAL", "CREATIVE", "HARDCORE", "FREEDOM"};
    return n[m < 0 || m > 3 ? 0 : m];
}

float storyFade() {
    static double t0 = -1.0;
    double now = glfwGetTime();
    if (t0 < 0.0) t0 = now;
    return (float)std::min(1.0, (now - t0) / 1.5);
}

bool storyTitleActive() {
    return g_story.mode == SM_TITLE;
}

bool storyEdge(AppContext& app, int key) {
    bool now = glfwGetKey(app.window, key) == GLFW_PRESS;
    bool edge = now && !g_keyPrev[key];
    g_keyPrev[key] = now;
    return edge;
}

bool storyEatSpace() {
    if (g_story.mode == SM_OFF) return false;
    if (g_story.mode == SM_TITLE || storyModal() || pdaActive()) {
        g_spaceCount++;
        return true;
    }
    return false;
}

int storyTakeSpace() {
    int n = g_spaceCount;
    g_spaceCount = 0;
    return n;
}

bool storyModal() {
    return g_story.mode == SM_TITLE || (g_story.mode == SM_STORY && (g_story.menuOpen || g_story.historyOpen));
}

bool storyFrozen() {
    return storyModal();
}

bool storyBlocksQuiz() {
    return g_story.mode != SM_OFF;
}

bool storyMaskKey(int key) {
    const StoryState& G = g_story;
    if (G.mode == SM_OFF) return false;
    if (G.mode == SM_TITLE || G.menuOpen || G.historyOpen) return key != GLFW_KEY_LEFT_SHIFT && key != GLFW_KEY_RIGHT_SHIFT;
    if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_L || key == GLFW_KEY_Z) return true;
    if (G.gameMode == GM_CREATIVE) return false;
    bool digit = (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) || (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9);
    if (digit) {
        int d = key >= GLFW_KEY_KP_0 ? key - GLFW_KEY_KP_0 : key - GLFW_KEY_0;
        return !(G.inSandbox && d >= 1 && d <= 4);
    }
    if (G.inSandbox && (key == GLFW_KEY_R || key == GLFW_KEY_C || key == GLFW_KEY_P || key == GLFW_KEY_X)) return true;
    return key == GLFW_KEY_BACKSPACE || key == GLFW_KEY_U;
}

bool storyPanelHit(const AppContext&) {
    return storyModal();
}

void storyScan(int id) {
    for (int s : g_story.scanned)
        if (s == id) return;
    g_story.scanned.push_back(id);
}

void storyUnlock(const char* tool) {
    if (!storyHasTool(tool)) g_story.unlocked.push_back(tool);
}

bool storyHasTool(const char* tool) {
    for (const std::string& s : g_story.unlocked)
        if (s == tool) return true;
    return false;
}

void storyBegin(AppContext& app, int ch, int obj, bool fresh, int& flags) {
    StoryState& G = g_story;
    if (fresh) {
        G.unlocked.clear();
        G.scanned.clear();
        G.logs.clear();
    }
    pdaReset();
    ch1Reset();
    G.mode = SM_STORY;
    G.ch = ch;
    G.obj = obj;
    G.menuOpen = false;
    G.historyOpen = false;
    G.sel = 0;
    G.doneToast = 0.0f;
    G.saveToast = 0.0f;
    const Chapter& C = chapter();
    if (fresh)
        for (int i = 0; i < obj && i < (int)C.objs.size(); i++) {
            markSeen(C.objs[i].onStart);
            markSeen(C.objs[i].onDone);
        }
    G.complete = obj >= (int)C.objs.size();
    runActions(app, C.onEnter, flags);
    startObjective(app, flags);
    if (fresh) storySaveFile();
}

void storyToTitle(AppContext& app, int& flags) {
    StoryState& G = g_story;
    storySaveFile();
    G.mode = SM_TITLE;
    G.menuOpen = false;
    G.historyOpen = false;
    G.sel = 0;
    G.hasSave = true;
    pdaReset();
    ch1Reset();
    app.sb = SandboxState{};
    if (app.scene != Scene::SOLAR) {
        QuizShow s{QK_SCENE, (int)Scene::SOLAR, 0, 0, 0.0f, 0.0f, 0.0f, nullptr, nullptr};
        quizJump(app, s, flags);
    }
}

void storyInit(AppContext& app) {
    StoryState& G = g_story;
    storySettingsLoad();
    bool wantFull = G.fullscreen;
    G.fullscreen = false;
    if (wantFull) storyToggleFullscreen(app);
    G.hasSave = storySaveExists();
    if (g_storyBoot == 1) {
        G.mode = SM_TITLE;
        G.screen = g_storyScreen;
        G.sel = g_storySel;
        G.optSel = g_storySel;
    } else if (g_storyBoot == 2) {
        g_pending = true;
        g_pendCh = std::max(0, g_storyAtCh - 1);
        g_pendObj = std::max(0, g_storyAtObj - 1);
    }
}

int storyUpdate(AppContext& app) {
    StoryState& G = g_story;
    int flags = 0;
    double now = glfwGetTime();
    float dt = g_clock < 0.0 ? 0.0f : (float)std::min(0.1, now - g_clock);
    g_clock = now;
    G.frames++;
    G.inSandbox = app.scene == Scene::SANDBOX;
    G.doneToast = std::max(0.0f, G.doneToast - dt);
    G.saveToast = std::max(0.0f, G.saveToast - dt);
    if (g_pending && G.frames >= 30) {
        g_pending = false;
        int ch = std::min(g_pendCh, (int)storyChapters().size() - 1);
        int objs = (int)storyChapters()[ch].objs.size();
        storyBegin(app, ch, std::min(g_pendObj, objs), true, flags);
        return flags;
    }
    if (G.mode == SM_TITLE) {
        flags |= storyTitleUpdate(app);
        return flags;
    }
    if (G.mode != SM_STORY) return flags;
    flags |= storyPauseUpdate(app);
    if (storyModal()) return flags;
    int spaces = storyTakeSpace();
    pdaUpdate(dt, spaces > 0);
    if (G.frames > 120 && g_storyUi && g_uiShown != g_storyUi) {
        g_uiShown = g_storyUi;
        if (g_storyUi == 1) G.historyOpen = true;
        if (g_storyUi == 2) G.menuOpen = true;
        G.histScroll = -1;
    }
    if (G.complete) return flags;
    G.objT += dt;
    if (objectiveDone(app)) completeObjective(app, flags);
    return flags;
}
