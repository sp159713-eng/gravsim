#include "quiz.h"
#include "story.h"
#include "vk_app.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <string>
#undef glfwGetKey

int g_quizStartQ = -1;
int g_quizStartAnswer = -1;
int g_quizFinal = -1;
QuizState g_quiz;

namespace {

std::mt19937 g_rng((unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
int g_frames = 0;
bool g_demoFinal = false;
bool g_zPrev = false, g_escPrev = false, g_nPrev = false, g_mousePrev = false;
bool g_keyPrev[4] = {false, false, false, false};
double g_answerAt = -1.0;

std::string savePath() {
    const char* ap = std::getenv("APPDATA");
    std::string dir = std::string(ap ? ap : ".") + "/gravsim";
    return dir + "/quiz.txt";
}

void loadBest() {
    g_quiz.bestScore = 0;
    g_quiz.bestStreak = 0;
    FILE* f = std::fopen(savePath().c_str(), "r");
    if (!f) return;
    char line[64];
    while (std::fgets(line, sizeof line, f)) {
        int v = 0;
        if (std::sscanf(line, "best=%d", &v) == 1) g_quiz.bestScore = std::max(0, std::min(QUIZ_ROUND, v));
        if (std::sscanf(line, "streak=%d", &v) == 1) g_quiz.bestStreak = std::max(0, std::min(QUIZ_ROUND, v));
    }
    std::fclose(f);
}

void saveBest() {
    std::string path = savePath();
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) return;
    std::fprintf(f, "best=%d\nstreak=%d\n", g_quiz.bestScore, g_quiz.bestStreak);
    std::fclose(f);
}

bool realKey(AppContext& app, int key) {
    return glfwGetKey(app.window, key) == GLFW_PRESS;
}

void setQuestion(int seed) {
    QuizState& Q = g_quiz;
    std::mt19937 local((unsigned)seed * 2654435761u + 17u);
    std::mt19937& r = seed >= 0 ? local : g_rng;
    for (int k = 0; k < 4; k++) Q.perm[k] = k;
    std::shuffle(Q.perm, Q.perm + 4, r);
    for (int k = 0; k < 4; k++)
        if (Q.perm[k] == 0) Q.correctSlot = k;
    Q.picked = -1;
    Q.phase = QP_ASK;
}

void begin(AppContext& app, int firstQ) {
    QuizState& Q = g_quiz;
    if (!Q.active) {
        Q.origScene = (int)app.scene;
        Q.origCam = app.camera;
        Q.origFollow = app.follow;
        Q.origTrue = app.trueScale;
        Q.origSky = app.showSky;
        Q.origGalaxy = app.showGalaxy;
        Q.origGrid = app.showGrid;
        Q.origLabels = app.showLabels;
        Q.origOrbits = app.showOrbits;
        Q.jumped = false;
    }
    Q.active = true;
    g_demoFinal = false;
    loadBest();
    int total = quizBankCount();
    std::vector<int> ids(total);
    for (int i = 0; i < total; i++) ids[i] = i;
    std::shuffle(ids.begin(), ids.end(), g_rng);
    if (firstQ >= 0 && firstQ < total) {
        ids.erase(std::find(ids.begin(), ids.end(), firstQ));
        ids.insert(ids.begin(), firstQ);
    }
    for (int i = 0; i < QUIZ_ROUND; i++) Q.order[i] = ids[i];
    Q.idx = 0;
    Q.score = 0;
    Q.streak = 0;
    Q.roundStreak = 0;
    Q.newBest = false;
    setQuestion(firstQ >= 0 ? firstQ : -1);
}

void finish(int& flags) {
    QuizState& Q = g_quiz;
    Q.phase = QP_FINAL;
    Q.newBest = false;
    if (Q.score > Q.bestScore) {
        Q.bestScore = Q.score;
        Q.newBest = true;
    }
    if (Q.roundStreak > Q.bestStreak) {
        Q.bestStreak = Q.roundStreak;
        Q.newBest = true;
    }
    if (!g_demoFinal) saveBest();
    (void)flags;
}

void answer(AppContext& app, int slot, int& flags) {
    QuizState& Q = g_quiz;
    if (Q.phase != QP_ASK || slot < 0 || slot > 3) return;
    Q.picked = slot;
    Q.phase = QP_REVEAL;
    if (slot == Q.correctSlot) {
        Q.score++;
        Q.streak++;
        Q.roundStreak = std::max(Q.roundStreak, Q.streak);
    } else {
        Q.streak = 0;
    }
    Q.jumped = true;
    quizJump(app, quizBankAt(Q.order[Q.idx]).show, flags);
}

void nextQuestion(int& flags) {
    QuizState& Q = g_quiz;
    Q.idx++;
    if (Q.idx >= QUIZ_ROUND) {
        finish(flags);
        return;
    }
    setQuestion(-1);
}

void restore(AppContext& app, int& flags) {
    QuizState& Q = g_quiz;
    if (Q.jumped) {
        if (app.tour.mode != 0) tourStop(app);
        app.cu.mode = 0;
        app.trueScale = Q.origTrue;
        switchScene(app, (Scene)Q.origScene);
        app.camera = Q.origCam;
        app.follow = Q.origFollow;
        app.showSky = Q.origSky;
        app.showGalaxy = Q.origGalaxy;
        app.showGrid = Q.origGrid;
        app.showLabels = Q.origLabels;
        app.showOrbits = Q.origOrbits;
        flags |= 1;
    }
    Q.active = false;
    Q.jumped = false;
    quizJumpCancel();
}

}

int quizBankCount() {
    return QUIZ_BANK_A_N + QUIZ_BANK_B_N + QUIZ_BANK_C_N;
}

const QuizQ& quizBankAt(int i) {
    if (i < QUIZ_BANK_A_N) return QUIZ_BANK_A[i];
    i -= QUIZ_BANK_A_N;
    if (i < QUIZ_BANK_B_N) return QUIZ_BANK_B[i];
    return QUIZ_BANK_C[i - QUIZ_BANK_B_N];
}

const char* quizRank(int score) {
    if (score >= 10) return "COSMIC ENTITY";
    if (score >= 8) return "ASTROPHYSICIST";
    if (score >= 5) return "ASTRONAUT";
    return "SPACE CADET";
}

bool quizActive() {
    return g_quiz.active;
}

int quizGetKey(GLFWwindow* w, int key) {
    if (storyMaskKey(key)) return GLFW_RELEASE;
    if (g_quiz.active || g_quiz.escHold) {
        bool digit = (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) || (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9);
        if (g_quiz.active && (digit || key == GLFW_KEY_N)) return GLFW_RELEASE;
        if (key == GLFW_KEY_ESCAPE) return GLFW_RELEASE;
    }
    return glfwGetKey(w, key);
}

bool quizPanelHit(const AppContext& app) {
    if (!g_quiz.active) return false;
    QuizLayout l = quizLayout(app);
    return l.card.has((float)app.cursorFb[0], (float)app.cursorFb[1]);
}



int quizUpdate(AppContext& app) {
    QuizState& Q = g_quiz;
    int flags = 0;
    g_frames++;
    bool esc = realKey(app, GLFW_KEY_ESCAPE);
    if (Q.escHold && !esc) Q.escHold = false;
    bool z = !timeTyping() && realKey(app, GLFW_KEY_Z) && !realKey(app, GLFW_KEY_LEFT_SHIFT) && !realKey(app, GLFW_KEY_RIGHT_SHIFT);
    if (!Q.active) {
        bool autoOpen = g_quizStartQ >= 0 && g_frames > 40;
        if ((z && !g_zPrev) || autoOpen) {
            begin(app, g_quizStartQ);
            if (autoOpen) {
                g_quizStartQ = -1;
                if (g_quizStartAnswer >= 0) g_answerAt = glfwGetTime() + 1.0;
            }
            if (g_quizFinal >= 0) {
                Q.score = std::min(QUIZ_ROUND, g_quizFinal);
                Q.roundStreak = Q.score;
                g_quizFinal = -1;
                g_demoFinal = true;
                finish(flags);
            }
        } else if (g_quizFinal >= 0 && g_frames > 40) {
            begin(app, -1);
            Q.score = std::min(QUIZ_ROUND, g_quizFinal);
            Q.roundStreak = Q.score;
            g_quizFinal = -1;
            g_demoFinal = true;
            finish(flags);
        }
        g_zPrev = z;
        g_escPrev = esc;
        return flags;
    }
    g_zPrev = z;
    if (esc && !g_escPrev) {
        Q.escHold = true;
        restore(app, flags);
        g_escPrev = esc;
        return flags;
    }
    g_escPrev = esc;
    bool n = realKey(app, GLFW_KEY_N) || realKey(app, GLFW_KEY_ENTER) || realKey(app, GLFW_KEY_KP_ENTER);
    bool mouse = app.mouseHeld;
    QuizLayout lay = quizLayout(app);
    float cx = (float)app.cursorFb[0], cy = (float)app.cursorFb[1];
    bool click = !mouse && g_mousePrev && app.cursorOk;
    g_mousePrev = mouse;
    if (Q.phase == QP_ASK) {
        for (int k = 0; k < 4; k++) {
            bool now = realKey(app, GLFW_KEY_1 + k) || realKey(app, GLFW_KEY_KP_1 + k);
            if (now && !g_keyPrev[k]) answer(app, k, flags);
            g_keyPrev[k] = now;
            if (click && lay.btn[k].has(cx, cy) && Q.phase == QP_ASK) answer(app, k, flags);
        }
        if (g_answerAt > 0.0 && glfwGetTime() >= g_answerAt) {
            int want = g_quizStartAnswer;
            int slot = want == 0 ? Q.correctSlot : want == 9 ? (Q.correctSlot + 1) % 4 : want - 1;
            g_answerAt = -1.0;
            g_quizStartAnswer = -1;
            answer(app, slot, flags);
        }
    } else {
        for (int k = 0; k < 4; k++) g_keyPrev[k] = true;
        bool nextClick = click && lay.next.has(cx, cy);
        if ((n && !g_nPrev) || nextClick) {
            if (Q.phase == QP_REVEAL) nextQuestion(flags);
            else if (Q.phase == QP_FINAL) begin(app, -1);
        }
    }
    g_nPrev = n;
    quizJumpTick(app);
    return flags;
}
