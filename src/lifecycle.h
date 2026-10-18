#pragma once
#include <cmath>
#include <string>
#include <vector>

struct LifeState;

struct Stage {
    const char* name;
    const char* tag;
    const char* span;
    const char* fact;
    const char* fact2;
    const char* ex;
    int kind;
    int special;
    float secs;
    float r0, r1, t0, t1, v0, v1, a0, a1, g0, g1;
    float zEnd;
    float pitch, roll;
    bool cut;
};

struct StageText {
    std::string name, span, fact, fact2, ex;
};

enum StageId {
    ST_RED_DWARF = 13,
    ST_BLUE_DWARF = 14,
    ST_WD_BARE = 15,
    ST_WOLF_RAYET = 16,
    ST_MAGNETAR = 17,
    ST_KILONOVA = 18,
    ST_SUPERNOVA = 10 + 1
};

extern const Stage STAGES[];
extern const int STAGE_COUNT;
extern const float MASS_PRESETS[];
extern const int PRESET_COUNT;

std::vector<int> lifeSequence(const LifeState& L);
float lifeStageMass(const LifeState& L, int stage);
StageText lifeStageText(int stage, float mass);
std::string lifeLifetimeText(float mass);
std::string lifeMassText(float mass);
float lifeEpoch(int stage, float u);
float elementBirth(int z);
int elementCell(int row, int col);
const char* elementSymbol(int z);
const char* lifeEpochNote(float epoch);

inline float lifeUiScale(float H) {
    float s = std::floor(H / 720.0f + 0.5f);
    return s < 1.0f ? 1.0f : s;
}
