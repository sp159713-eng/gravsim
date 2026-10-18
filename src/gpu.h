#pragma once
#include <cstdint>
#include <string>
#include "particles.h"

std::string shaderPath(const char* name);
int runSelfTest();
int runApp(uint32_t n, Scene scene, float tourAt = -1.0f);
