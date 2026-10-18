#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct CapJob {
    int kind = 0;
    std::vector<uint8_t> px;
    int w = 0, h = 0;
    bool bgr = true;
    std::string path;
    int index = 0;
    std::string dir;
    std::string mp4;
};

double capClock();
std::string capTimestamp();
std::string capPicturesDir();
bool capFfmpegAvailable();
void capToastSet(const std::string& text);
bool capToastGet(std::string& text, double& age);
void capIoStart();
void capIoPush(CapJob&& job);
size_t capIoBacklog();
void capIoStop();
