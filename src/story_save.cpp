#include "story.h"
#include "story_json.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace {

std::string savePath() {
    const char* ap = std::getenv("APPDATA");
    return std::string(ap ? ap : ".") + "/gravsim/save.json";
}

}

bool storySaveExists() {
    std::error_code ec;
    return std::filesystem::exists(savePath(), ec);
}

void storySaveFile() {
    StoryState& G = g_story;
    Json j = Json::object();
    j.set("version", Json::number(1));
    j.set("chapter", Json::number(G.ch));
    j.set("objective", Json::number(G.obj));
    j.set("complete", Json::boolean(G.complete));
    j.set("mode", Json::number(G.gameMode));
    Json u = Json::array();
    for (const std::string& s : G.unlocked) u.push(Json::str(s));
    j.set("unlocked", u);
    Json sc = Json::array();
    for (int id : G.scanned) sc.push(Json::number(id));
    j.set("scanned", sc);
    Json lg = Json::array();
    for (const std::string& s : G.logs) lg.push(Json::str(s));
    j.set("logs", lg);
    Json st = Json::object();
    st.set("beep", Json::number(G.beep));
    st.set("textSpeed", Json::number(G.textSpeed));
    st.set("fullscreen", Json::boolean(G.fullscreen));
    st.set("logs", Json::boolean(G.logsOn));
    j.set("settings", st);
    std::string path = savePath();
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return;
    std::string text = jsonDump(j);
    std::fwrite(text.data(), 1, text.size(), f);
    std::fclose(f);
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec);
        std::filesystem::rename(tmp, path, ec);
    }
    G.hasSave = true;
    G.saveToast = 2.5f;
}

bool storyLoadFile() {
    FILE* f = std::fopen(savePath().c_str(), "rb");
    if (!f) return false;
    std::string text;
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
    std::fclose(f);
    Json j;
    if (!jsonParse(text, j)) return false;
    StoryState& G = g_story;
    G.ch = (int)j.num("chapter", 0);
    G.obj = (int)j.num("objective", 0);
    G.complete = j.num("complete", 0) != 0;
    G.gameMode = std::max(0, std::min(3, (int)j.num("mode", 0)));
    G.unlocked.clear();
    G.scanned.clear();
    G.logs.clear();
    if (const Json* u = j.get("unlocked"))
        for (const Json& e : u->a)
            if (e.k == Json::STR) G.unlocked.push_back(e.s);
    if (const Json* s = j.get("scanned"))
        for (const Json& e : s->a)
            if (e.k == Json::NUM) G.scanned.push_back((int)e.n);
    if (const Json* l = j.get("logs"))
        for (const Json& e : l->a)
            if (e.k == Json::STR) G.logs.push_back(e.s);
    if (const Json* st = j.get("settings")) {
        G.beep = (int)st->num("beep", 1);
        G.logsOn = st->num("logs", 1) != 0;
    }
    const std::vector<Chapter>& chs = storyChapters();
    if (G.ch < 0 || G.ch >= (int)chs.size()) {
        G.ch = 0;
        G.obj = 0;
        G.complete = false;
    }
    if (G.obj < 0 || G.obj > (int)chs[G.ch].objs.size()) G.obj = 0;
    return true;
}
