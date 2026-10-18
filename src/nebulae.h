#pragma once
#include <string>
#include <vector>

struct NebulaVertex {
    float pos[3];
    float size;
    float color[3];
    float seed;
};

struct NebulaInfo {
    const char* name;
    const char* type;
    const char* fact1;
    const char* fact2;
    float distLy, diameterLy;
    float pos[3];
    float color[3];
    std::string label;
};

std::vector<NebulaVertex> makeNebulae();
const std::vector<NebulaInfo>& nebulaInfos();
