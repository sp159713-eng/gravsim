#include "story.h"
#include "story_ui.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#undef glfwGetKey

extern double g_storyWheel;

namespace {

double g_lastCx = -1.0, g_lastCy = -1.0;
bool g_mousePrev = false;

}

std::vector<SRect> storyButtons(float W, float H, int u, int count, float topFrac, float bw, float bh) {
    std::vector<SRect> v;
    float gap = 14.0f * (float)u;
    float y = topFrac * H;
    for (int i = 0; i < count; i++) {
        v.push_back({0.5f * (W - bw), y, 0.5f * (W + bw), y + bh});
        y += bh + gap;
    }
    return v;
}

void storyNavReset(const AppContext& app) {
    g_mousePrev = app.mouseHeld;
}

Nav storyReadNav(AppContext& app, const std::vector<SRect>& rects) {
    Nav n;
    if (storyEdge(app, GLFW_KEY_UP) || storyEdge(app, GLFW_KEY_W)) n.move--;
    if (storyEdge(app, GLFW_KEY_DOWN) || storyEdge(app, GLFW_KEY_S)) n.move++;
    bool enter = storyEdge(app, GLFW_KEY_ENTER);
    bool kpEnter = storyEdge(app, GLFW_KEY_KP_ENTER);
    n.activate = enter || kpEnter || storyTakeSpace() > 0;
    double cx = app.cursorFb[0], cy = app.cursorFb[1];
    bool moved = std::fabs(cx - g_lastCx) + std::fabs(cy - g_lastCy) > 0.5;
    g_lastCx = cx;
    g_lastCy = cy;
    bool held = app.mouseHeld;
    bool click = !held && g_mousePrev && app.cursorOk;
    g_mousePrev = held;
    for (size_t i = 0; i < rects.size(); i++)
        if (rects[i].has((float)cx, (float)cy)) {
            if (moved) n.hoverIdx = (int)i;
            if (click) n.clickIdx = (int)i;
        }
    return n;
}

int storyApplyNav(const Nav& n, int sel, int count) {
    if (n.move) sel = (sel + n.move + count) % count;
    if (n.hoverIdx >= 0) sel = n.hoverIdx;
    if (n.clickIdx >= 0) sel = n.clickIdx;
    return sel;
}

namespace {

void historyLines(const SPen& p, std::vector<std::string>& lines, std::vector<int>& num, SRect& panel, int& rows, int& ts, float& line) {
    int u = p.u;
    ts = 2 * u;
    line = 16.0f * (float)u;
    float w = std::min(p.W - 40.0f * (float)u, 900.0f * (float)u);
    float h = p.H - 90.0f * (float)u;
    panel = {0.5f * (p.W - w), 0.5f * (p.H - h), 0.5f * (p.W + w), 0.5f * (p.H + h)};
    int maxc = (int)((w - 40.0f * (float)u) / (6.0f * (float)ts)) - 4;
    lines.clear();
    num.clear();
    int k = 0;
    for (const std::string& id : g_story.logs) {
        int li = storyLogIndex(id.c_str());
        if (li < 0) continue;
        k++;
        std::vector<std::string> wrap = storyWrap(storyClean(storyLogAt(li).text), maxc);
        for (size_t i = 0; i < wrap.size(); i++) {
            lines.push_back(wrap[i]);
            num.push_back(i == 0 ? k : -k);
        }
        lines.push_back("");
        num.push_back(0);
    }
    rows = std::max(1, (int)((h - 70.0f * (float)u) / line));
}

}

double g_storyWheel = 0.0;

int storyPauseUpdate(AppContext& app) {
    int flags = 0;
    StoryState& G = g_story;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    int u = storyUnit(app);
    G.noteToast = std::max(0.0f, G.noteToast - 0.016f);
    bool esc = storyEdge(app, GLFW_KEY_ESCAPE);
    bool l = storyEdge(app, GLFW_KEY_L);
    if (G.historyOpen) {
        storyTakeSpace();
        SPen probe;
        probe.W = W;
        probe.H = H;
        probe.u = u;
        std::vector<std::string> lines;
        std::vector<int> num;
        SRect panel;
        int rows, ts;
        float line;
        historyLines(probe, lines, num, panel, rows, ts, line);
        int maxScroll = std::max(0, (int)lines.size() - rows);
        int cur = G.histScroll < 0 ? maxScroll : std::min(G.histScroll, maxScroll);
        if (storyEdge(app, GLFW_KEY_UP)) cur--;
        if (storyEdge(app, GLFW_KEY_DOWN)) cur++;
        if (storyEdge(app, GLFW_KEY_PAGE_UP)) cur -= rows - 1;
        if (storyEdge(app, GLFW_KEY_PAGE_DOWN)) cur += rows - 1;
        if (storyEdge(app, GLFW_KEY_HOME)) cur = 0;
        if (storyEdge(app, GLFW_KEY_END)) cur = maxScroll;
        cur -= (int)std::lround(g_storyWheel * 3.0);
        g_storyWheel = 0.0;
        G.histScroll = std::max(0, std::min(cur, maxScroll));
        if (G.histScroll == maxScroll) G.histScroll = -1;
        if (esc || l) G.historyOpen = false;
        return flags;
    }
    g_storyWheel = 0.0;
    if (G.menuOpen) {
        std::vector<SRect> rects = storyButtons(W, H, u, 4, 0.40f, 320.0f * (float)u, 34.0f * (float)u);
        Nav n = storyReadNav(app, rects);
        G.sel = storyApplyNav(n, G.sel, 4);
        if (esc) {
            G.menuOpen = false;
        } else if (n.activate || n.clickIdx >= 0) {
            if (G.sel == 0) G.menuOpen = false;
            else if (G.sel == 1) G.logsOn = !G.logsOn;
            else if (G.sel == 2) G.noteToast = 150.0f;
            else storyToTitle(app, flags);
        }
        return flags;
    }
    storyTakeSpace();
    g_mousePrev = app.mouseHeld;
    if (esc) {
        G.menuOpen = true;
        G.sel = 0;
    } else if (l) {
        G.historyOpen = true;
        G.histScroll = -1;
    }
    return flags;
}

void storyDrawPause(const AppContext& app, SPen& p) {
    const StoryState& G = g_story;
    int u = p.u;
    float cx = 0.5f * p.W;
    SRect panel{cx - 220.0f * (float)u, 0.40f * p.H - 130.0f * (float)u, cx + 220.0f * (float)u, 0.40f * p.H + 250.0f * (float)u};
    p.rect(panel, sc::BG);
    p.frame(panel, (float)u, sc::EDGE);
    p.center("PAUSED", cx, panel.y0 + 18.0f * (float)u, 5 * u, sc::WHITE);
    const Chapter& C = storyChapters()[G.ch];
    std::string where = std::string("CH ") + std::to_string(G.ch + 1) + "  " + C.title;
    p.center(where, cx, panel.y0 + 62.0f * (float)u, 2 * u, sc::CYAN);
    p.center(storyModeName(G.gameMode), cx, panel.y0 + 80.0f * (float)u, 2 * u, G.gameMode == GM_HARDCORE ? sc::RED : sc::GOLD);
    std::vector<SRect> rects = storyButtons(p.W, p.H, u, 4, 0.40f, 320.0f * (float)u, 34.0f * (float)u);
    std::string items[4] = {"RESUME", std::string("PDA LOGS: ") + (G.logsOn ? "ON" : "OFF"), "DATABANK", "SAVE AND QUIT TO TITLE"};
    for (int i = 0; i < 4; i++) {
        bool hot = i == G.sel;
        p.rect(rects[i], hot ? sc::BTN_HOT : sc::BTN);
        p.frame(rects[i], (float)u * (hot ? 2.0f : 1.0f), hot ? sc::CYAN : sc::EDGE);
        p.center(items[i], cx, rects[i].y0 + 0.5f * (34.0f * (float)u - 14.0f * (float)u), 2 * u, hot ? sc::WHITE : sc::PDA);
    }
    if (G.noteToast > 0.0f) p.center("DATABANK EMPTY - NOTHING SCANNED YET", cx, rects[3].y1 + 12.0f * (float)u, 2 * u, sc::GOLD);
    else p.center("ESC: RESUME", cx, rects[3].y1 + 12.0f * (float)u, 2 * u, sc::DIM);
    (void)app;
}

void storyDrawHistory(const AppContext& app, SPen& p) {
    const StoryState& G = g_story;
    int u = p.u;
    std::vector<std::string> lines;
    std::vector<int> num;
    SRect panel;
    int rows, ts;
    float line;
    historyLines(p, lines, num, panel, rows, ts, line);
    p.rect(panel, sc::BG);
    p.frame(panel, (float)u, sc::EDGE);
    p.rect(panel.x0, panel.y0, panel.x1, panel.y0 + 34.0f * (float)u, sc::HEAD);
    p.text("PDA LOG HISTORY", panel.x0 + 16.0f * (float)u, panel.y0 + 10.0f * (float)u, ts, sc::PDA);
    std::string cnt = std::to_string(G.logs.size()) + " MESSAGES";
    p.text(cnt, panel.x1 - 16.0f * (float)u - SPen::width(cnt, ts), panel.y0 + 10.0f * (float)u, ts, sc::DIM);
    int maxScroll = std::max(0, (int)lines.size() - rows);
    int first = G.histScroll < 0 ? maxScroll : std::min(G.histScroll, maxScroll);
    float y = panel.y0 + 46.0f * (float)u;
    for (int i = first; i < (int)lines.size() && i < first + rows; i++) {
        if (num[i] > 0) {
            char buf[8];
            std::snprintf(buf, sizeof buf, "%02d", num[i]);
            p.text(buf, panel.x0 + 16.0f * (float)u, y, ts, sc::GOLD);
        }
        p.text(lines[i], panel.x0 + 16.0f * (float)u + 4.0f * 6.0f * (float)ts, y, ts, sc::PDA);
        y += line;
    }
    if (lines.empty()) p.text("NO MESSAGES YET", panel.x0 + 16.0f * (float)u, y, ts, sc::DIM);
    if (maxScroll > 0) {
        float th = panel.y1 - 40.0f * (float)u - (panel.y0 + 46.0f * (float)u);
        float bh = std::max(12.0f * (float)u, th * (float)rows / (float)lines.size());
        float by = panel.y0 + 46.0f * (float)u + (th - bh) * (float)first / (float)maxScroll;
        p.rect(panel.x1 - 10.0f * (float)u, by, panel.x1 - 5.0f * (float)u, by + bh, sc::EDGE);
    }
    p.center("UP / DOWN / WHEEL: SCROLL      L OR ESC: CLOSE", 0.5f * (panel.x0 + panel.x1), panel.y1 - 24.0f * (float)u, ts, sc::DIM);
    (void)app;
}
