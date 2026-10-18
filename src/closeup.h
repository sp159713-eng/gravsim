#pragma once
#include "camera.h"

struct CloseupState {
    int mode = 0;
    int kind = 0;
    int obj = -1;
    float t = 0.0f, dur = 2.6f;
    float alpha = 0.0f;
    float targetLy[3] = {0.0f, 0.0f, 0.0f};
    float dir[3] = {0.0f, 0.0f, 1.0f};
    double d0 = 1.0, d1 = 1.0;
    Camera saved;
    int savedFollow = -1;
    int savedRate = 0;
    float vyaw = 0.0f, vpitch = 0.0f;
    float panX = 0.0f, panY = 0.0f;
    float zoom = 1.0f;
    double time = 0.0;
    float rate = 1.0f;
    float clock = 0.0f;
    bool escDown = false, plusDown = false, minusDown = false, helpWas = false;
};
