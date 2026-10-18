#pragma once
#include "vk_app.h"
#include <string>
#include <vector>

struct HelpRow {
    int kind;
    const char* key;
    const char* text;
};

struct HelpPage {
    std::vector<HelpRow> rows;
    int cols = 1;
    int colFirst[4] = {0, 0, 0, 0};
    float colX[3] = {0.0f, 0.0f, 0.0f};
    float colKey[3] = {0.0f, 0.0f, 0.0f};
    float ts = 1.0f;
    float pad = 8.0f;
    float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
    float bodyY = 0.0f;
    float footY = 0.0f;
    std::string title;
    std::string footer;
};

const HelpRow* helpRows(int& count);
const HelpPage& helpPage(const AppContext& app, float topY);
void helpKeys(AppContext& app);
