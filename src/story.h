#pragma once
#include "quiz.h"
#include <string>
#include <vector>

enum StoryMode { SM_OFF, SM_TITLE, SM_STORY };
enum ObjType { OBJ_WAIT, OBJ_SCENE, OBJ_SCAN, OBJ_DIST, OBJ_CUSTOM };
enum ActType { ACT_SCENE, ACT_LOG, ACT_UNLOCK, ACT_CALL };
enum ObjFlag { OF_CAMERA_SKIP = 1, OF_WAIT_PDA = 2 };

struct StoryAction {
    int type = ACT_CALL;
    QuizShow show{};
    const char* s = nullptr;
    void (*fn)(AppContext&) = nullptr;
};

struct Objective {
    const char* text;
    int type;
    float p;
    int id;
    bool (*check)(AppContext&, float);
    int flags;
    std::vector<StoryAction> onStart;
    std::vector<StoryAction> onDone;
    const char* hint = nullptr;
};

struct Chapter {
    const char* title;
    std::vector<StoryAction> onEnter;
    std::vector<Objective> objs;
};

struct StoryLog {
    const char* id;
    const char* text;
};

struct StoryState {
    int mode = SM_OFF;
    int ch = 0, obj = 0;
    bool complete = false;
    float objT = 0.0f;
    std::vector<std::string> unlocked;
    std::vector<int> scanned;
    std::vector<std::string> logs;
    int beep = 1;
    bool menuOpen = false, historyOpen = false;
    int sel = 0;
    float doneToast = 0.0f, saveToast = 0.0f, noteToast = 0.0f;
    bool inSandbox = false;
    bool hasSave = false;
    int frames = 0;
    float camSnap[5] = {0, 0, 0, 0, 0};
    bool camSnapped = false;
    float eyeSnap[3] = {0, 0, 0};
    bool eyeSnapped = false;
    int histScroll = -1;
    int gameMode = 0;
    int screen = 0;
    int textSpeed = 1;
    bool fullscreen = false;
    bool logsOn = true;
    float hover[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int optSel = 0;
    int subSel = 0;
    float subSlide = 0.0f;
    int volume = 10;
    float sens = 1.0f;
    bool showFps = false;
};

enum GameMode { GM_SURVIVAL, GM_CREATIVE, GM_HARDCORE, GM_FREEDOM };

struct StoryModeCfg {
    int mode;
    bool drain;
    bool damage;
    bool food;
    bool permadeath;
    bool creative;
    float drainMul;
    float damageMul;
};

const StoryModeCfg& storyMode();
const char* storyModeName(int m);
float storyFade();
void createTitleBg(AppContext& app);
void destroyTitleBg(AppContext& app);
void drawTitleBg(AppContext& app, VkCommandBuffer cmd, float aspect);
bool storyTitleActive();
void storyToggleFullscreen(AppContext& app);
void storySettingsSave();
void storySettingsLoad();
extern float g_mouseSens;

extern StoryState g_story;
extern int g_storyBoot;
extern int g_storyAtCh;
extern int g_storyAtObj;
extern int g_storyUi;
extern bool g_storyMute;
extern int g_storyScreen;
extern int g_storySel;

const std::vector<Chapter>& storyChapters();
int storyLogCount();
const StoryLog& storyLogAt(int i);
int storyLogIndex(const char* id);

StoryAction actScene(Scene sc);
StoryAction actLog(const char* id);
StoryAction actUnlock(const char* tool);
StoryAction actCall(void (*fn)(AppContext&));

void storyInit(AppContext& app);
int storyUpdate(AppContext& app);
void storyOverlay(AppContext& app);
bool storyModal();
bool storyFrozen();
bool storyMaskKey(int key);
bool storyEatSpace();
bool storyPanelHit(const AppContext& app);
bool storyBlocksQuiz();
void storyScan(int id);
void storyUnlock(const char* tool);
bool storyHasTool(const char* tool);

void storySaveFile();
bool storyLoadFile();
bool storySaveExists();

void pdaPush(const char* id);
void pdaUpdate(float dt, bool space);
bool pdaBusy();
void pdaReset();
void pdaRecordSeen(const char* id);
bool pdaSeen(const char* id);
bool pdaActive();
int pdaCurrent();
float pdaShown();
bool pdaTyping();

void ch1BuildSystem(AppContext& app);
bool ch1OrbitCheck(AppContext& app, float t);
void ch1Reset();

int storyTitleUpdate(AppContext& app);
int storyPauseUpdate(AppContext& app);
bool storyEdge(AppContext& app, int key);
int storyTakeSpace();
void storyBegin(AppContext& app, int ch, int obj, bool fresh, int& flags);
void storyToTitle(AppContext& app, int& flags);
