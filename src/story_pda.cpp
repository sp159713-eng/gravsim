#include "story.h"
#include <windows.h>
#include <cstring>
#include <deque>

namespace {

std::deque<int> g_queue;
int g_cur = -1;
float g_shown = 0.0f;
float g_hold = 0.0f;
bool g_done = false;
int g_beepStep = 0;
volatile LONG g_beepReq = 0;
volatile LONG g_beepFreq = 760;
HANDLE g_thread = nullptr;
const float HOLD = 2.6f;

DWORD WINAPI beepMain(LPVOID) {
    for (;;) {
        if (InterlockedExchange(&g_beepReq, 0)) Beep((DWORD)g_beepFreq, 9);
        else Sleep(5);
    }
    return 0;
}

void beep(int k) {
    if (!g_story.beep || g_storyMute || g_story.volume <= 0) return;
    if (!g_thread) g_thread = CreateThread(nullptr, 0, beepMain, nullptr, 0, nullptr);
    InterlockedExchange(&g_beepFreq, 700 + 45 * (k % 5));
    InterlockedExchange(&g_beepReq, 1);
}

void startNext() {
    if (g_queue.empty()) {
        g_cur = -1;
        return;
    }
    g_cur = g_queue.front();
    g_queue.pop_front();
    g_shown = 0.0f;
    g_hold = 0.0f;
    g_done = false;
    g_beepStep = 0;
    pdaRecordSeen(storyLogAt(g_cur).id);
    beep(2);
}

}

bool pdaSeen(const char* id) {
    for (const std::string& s : g_story.logs)
        if (s == id) return true;
    return false;
}

void pdaRecordSeen(const char* id) {
    if (!pdaSeen(id)) g_story.logs.push_back(id);
}

void pdaPush(const char* id) {
    int i = storyLogIndex(id);
    if (i < 0 || pdaSeen(id) || !g_story.logsOn) return;
    g_queue.push_back(i);
    if (g_cur < 0) startNext();
}

void pdaReset() {
    g_queue.clear();
    g_cur = -1;
    g_shown = 0.0f;
    g_hold = 0.0f;
    g_done = false;
}

bool pdaBusy() {
    return g_cur >= 0 || !g_queue.empty();
}

bool pdaActive() {
    return g_cur >= 0;
}

int pdaCurrent() {
    return g_cur;
}

float pdaShown() {
    return g_shown;
}

bool pdaTyping() {
    return g_cur >= 0 && !g_done;
}

void pdaUpdate(float dt, bool space) {
    if (g_cur < 0) return;
    int len = (int)std::strlen(storyLogAt(g_cur).text);
    if (!g_done) {
        if (space) {
            g_shown = (float)len;
        } else {
            float before = g_shown;
            g_shown += dt * (g_story.textSpeed == 0 ? 22.0f : g_story.textSpeed == 2 ? 90.0f : 40.0f);
            for (int c = (int)before; c < (int)g_shown && c < len; c++) {
                char ch = storyLogAt(g_cur).text[c];
                if (ch != ' ' && ++g_beepStep % 3 == 0) beep(c);
            }
        }
        if (g_shown >= (float)len) {
            g_shown = (float)len;
            g_done = true;
            g_hold = HOLD;
        }
        return;
    }
    g_hold -= dt;
    if (space || g_hold <= 0.0f) startNext();
}
