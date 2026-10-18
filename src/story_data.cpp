#include "story.h"
#include <cstring>

namespace {

const StoryLog LOGS[] = {
    {"w1", "CRYOSLEEP CYCLE INTERRUPTED. HULL INTEGRITY 31%."},
    {"w2", "WELCOME BACK. I HAVE BAD NEWS. EARTH IS GONE."},
    {"w3", "IT WAS DESTROYED BY AN ALIEN SPECIES WHILE YOU SLEPT. MY RECORDS CALL THEM THE VEY."},
    {"w4", "YOU ARE THE LAST HUMAN. OUR MISSION: FIND THEM."},
    {"relay", "COMMS RELAY STABILISED. PICKING UP A SIGNAL..."},
    {"end1", "END OF DEMO - CHAPTER 2 COMING SOON"},
};

std::vector<Chapter> build() {
    std::vector<Chapter> v;
    Chapter c;
    c.title = "WRECK";
    c.onEnter = {actScene(Scene::SANDBOX), actCall(ch1BuildSystem)};
    Objective look{"LOOK AROUND", OBJ_WAIT, 10.0f, 0, nullptr, OF_CAMERA_SKIP | OF_WAIT_PDA,
                   {actLog("w1"), actLog("w2"), actLog("w3"), actLog("w4")}, {}, "RIGHT DRAG: ORBIT CAMERA   SCROLL: ZOOM"};
    Objective rock{"THROW A ROCK INTO ORBIT AROUND THE STAR", OBJ_CUSTOM, 0.0f, 0, ch1OrbitCheck, 0,
                   {actUnlock("THROW")}, {actLog("relay"), actLog("end1")}, "DRAG TO THROW   CLICK: CIRCULAR ORBIT"};
    c.objs = {look, rock};
    v.push_back(c);
    return v;
}

}

const std::vector<Chapter>& storyChapters() {
    static const std::vector<Chapter> chapters = build();
    return chapters;
}

int storyLogCount() {
    return (int)(sizeof LOGS / sizeof LOGS[0]);
}

const StoryLog& storyLogAt(int i) {
    return LOGS[i];
}

int storyLogIndex(const char* id) {
    for (int i = 0; i < storyLogCount(); i++)
        if (std::strcmp(LOGS[i].id, id) == 0) return i;
    return -1;
}

StoryAction actScene(Scene sc) {
    StoryAction a;
    a.type = ACT_SCENE;
    a.show = QuizShow{QK_SCENE, (int)sc, 0, 0, 0.0f, 0.0f, 0.0f, nullptr, nullptr};
    return a;
}

StoryAction actLog(const char* id) {
    StoryAction a;
    a.type = ACT_LOG;
    a.s = id;
    return a;
}

StoryAction actUnlock(const char* tool) {
    StoryAction a;
    a.type = ACT_UNLOCK;
    a.s = tool;
    return a;
}

StoryAction actCall(void (*fn)(AppContext&)) {
    StoryAction a;
    a.type = ACT_CALL;
    a.fn = fn;
    return a;
}
