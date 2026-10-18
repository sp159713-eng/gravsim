#pragma once
#include "vk_app.h"

struct Primary {
    double p[3], v[3], m;
};

Primary primaryOf(const AppContext& app);
double distEye(const AppContext& app, const double p[3]);
void throwPreview(AppContext& app, const double v[3]);
