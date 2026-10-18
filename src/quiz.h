#pragma once
#include "vk_app.h"
#include <string>
#include <vector>

enum QuizKind {
    QK_SCENE,
    QK_SOLAR,
    QK_NOSUN,
    QK_TOUR,
    QK_STARS,
    QK_LIGHT,
    QK_ECLIPSE,
    QK_IMPACT,
    QK_LAGRANGE,
    QK_NIGHT,
    QK_TIMEDIL,
    QK_EARTHMOON,
    QK_CLOSEUP,
    QK_FUTURE,
    QK_LIFE
};

struct QuizShow {
    int kind;
    int a, b, c;
    float x, y, z;
    const char* s;
    const char* caption;
};

struct QuizQ {
    const char* q;
    const char* ans[4];
    const char* why;
    QuizShow show;
};

constexpr int QUIZ_ROUND = 10;

extern const QuizQ QUIZ_BANK_A[];
extern const int QUIZ_BANK_A_N;
extern const QuizQ QUIZ_BANK_B[];
extern const int QUIZ_BANK_B_N;
extern const QuizQ QUIZ_BANK_C[];
extern const int QUIZ_BANK_C_N;

extern int g_quizStartQ;
extern int g_quizStartAnswer;
extern int g_quizFinal;

int quizBankCount();
const QuizQ& quizBankAt(int i);

enum QuizPhase { QP_ASK, QP_REVEAL, QP_FINAL };

struct QuizState {
    bool active = false;
    int phase = QP_ASK;
    int order[QUIZ_ROUND] = {};
    int idx = 0;
    int perm[4] = {0, 1, 2, 3};
    int correctSlot = 0;
    int picked = -1;
    int score = 0;
    int streak = 0;
    int roundStreak = 0;
    int bestScore = 0;
    int bestStreak = 0;
    bool newBest = false;
    bool escHold = false;
    bool jumped = false;
    int origScene = 0;
    Camera origCam{};
    int origFollow = -1;
    bool origTrue = false;
    bool origSky = false, origGalaxy = false, origGrid = false, origLabels = false, origOrbits = false;
};

extern QuizState g_quiz;

struct QuizRect {
    float x0, y0, x1, y1;
    bool has(float x, float y) const { return x >= x0 && x <= x1 && y >= y0 && y <= y1; }
};

struct QuizLayout {
    QuizRect card;
    QuizRect btn[4];
    QuizRect next;
    int scale = 2;
    float pad = 0.0f, line = 0.0f;
    int maxChars = 0;
    int btnChars = 0;
    std::vector<std::string> qLines;
    std::vector<std::string> ansLines[4];
    std::vector<std::string> whyLines;
    float qY = 0.0f, btnY = 0.0f, resY = 0.0f;
};

QuizLayout quizLayout(const AppContext& app);
std::string quizClean(const char* s);
std::vector<std::string> quizWrap(const std::string& s, int maxChars, int maxLines);
const char* quizRank(int score);

bool quizActive();
bool quizPanelHit(const AppContext& app);
int quizUpdate(AppContext& app);
float quizTourAt();
void quizJump(AppContext& app, const QuizShow& s, int& flags);
void quizJumpTick(AppContext& app);
void quizJumpCancel();
void quizEmitTris(AppContext& app, OverlayVertex* out, uint32_t& used);
void quizEmitText(AppContext& app, OverlayVertex* out, uint32_t& used);
