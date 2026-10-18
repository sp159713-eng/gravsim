#include "story.h"
#include "story_json.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

float g_mouseSens = 1.0f;

namespace {

std::string settingsPath() {
    const char* ap = std::getenv("APPDATA");
    return std::string(ap ? ap : ".") + "/gravsim/settings.json";
}

}

void storySettingsSave() {
    const StoryState& G = g_story;
    g_mouseSens = G.sens;
    Json j = Json::object();
    j.set("fullscreen", Json::boolean(G.fullscreen));
    j.set("textSpeed", Json::number(G.textSpeed));
    j.set("volume", Json::number(G.volume));
    j.set("mouseSensitivity", Json::number(G.sens));
    j.set("showFps", Json::boolean(G.showFps));
    std::string path = settingsPath();
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::string text = jsonDump(j);
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
}

void storySettingsLoad() {
    StoryState& G = g_story;
    FILE* f = std::fopen(settingsPath().c_str(), "rb");
    if (!f) return;
    std::string text;
    char buf[1024];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
    std::fclose(f);
    Json j;
    if (!jsonParse(text, j)) return;
    G.fullscreen = j.num("fullscreen", 0) != 0;
    G.textSpeed = std::max(0, std::min(2, (int)j.num("textSpeed", 1)));
    G.volume = std::max(0, std::min(10, (int)j.num("volume", 10)));
    G.sens = std::max(0.25f, std::min(3.0f, (float)j.num("mouseSensitivity", 1.0)));
    G.showFps = j.num("showFps", 0) != 0;
    g_mouseSens = G.sens;
}
