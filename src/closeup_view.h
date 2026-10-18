#pragma once
#include "vk_app.h"

struct CuDraw {
    float push[32];
    bool bh = false;
    float bhPos[3] = {0.0f, 0.0f, 0.0f};
    float bhNormal[3] = {0.25f, 0.9f, 0.35f};
    float bhRadius = 1.0f;
    float galMode = 1.0f;
    float vp[16];
    Camera cam;
};

bool closeupFill(const AppContext& app, float aspect, CuDraw& out);
void closeupViewMatrix(const AppContext& app, float aspect, float vp[16], Camera& cam);

float cuHalfHeight(const AppContext& app);
bool cuAllowed(const AppContext& app);
