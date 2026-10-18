#include "story.h"
#include "story_ui.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#undef glfwGetKey

namespace {

const char* GLYPH[4][9] = {
    {".XX...XX.", "XXXX.XXXX", "XXXXXXXXX", "XXXXXXXXX", ".XXXXXXX.", "..XXXXX..", "...XXX...", "....X....", "........."},
    {"....X....", "...XXX...", "...XXX...", ".XXXXXXX.", "XXXXXXXXX", ".XXXXXXX.", "...XXX...", "...XXX...", "....X...."},
    {".XXXXXXX.", "XXXXXXXXX", "XXXXXXXXX", "XX.XXX.XX", "XX.XXX.XX", "XXXX.XXXX", ".XXXXXXX.", "..X.X.X..", "..XXXXX.."},
    {".........", "..XX.....", ".X..X..XX", "X....XX..", ".........", ".XX......", "X..X...XX", "....XXX..", "........."},
};

const char* DESC[4] = {
    "ALL STATS DRAIN: HEALTH, OXYGEN, FOOD, WATER AND RADIATION. SHIP HULL, FUEL AND POWER MATTER. DEATH RETURNS YOU TO THE LAST CHECKPOINT AND DROPS SOME RESOURCES.",
    "NO DRAIN AND NO DAMAGE. INFINITE RESOURCES. ALL TOOLS AND SHIP MODULES UNLOCKED. EXPLORE EVERY SCENE FREELY. OBJECTIVES ARE OPTIONAL AND PDA LOGS CAN BE TOGGLED.",
    "SURVIVAL, BUT WITH HARSHER DRAIN AND DAMAGE. PERMADEATH: DEATH DELETES YOUR SAVE.",
    "SURVIVAL WITHOUT FOOD AND WATER. EVERYTHING ELSE STILL TRIES TO KILL YOU.",
};

const float* modeColor(int m) {
    static const float c[4][3] = {{0.45f, 0.95f, 0.58f}, {0.55f, 0.88f, 1.0f}, {1.0f, 0.30f, 0.26f}, {1.0f, 0.80f, 0.38f}};
    return c[m];
}

double g_prevT = -1.0;
float g_dt = 0.016f;

std::vector<std::string> mainLabels() {
    return {"START", "SETTINGS", "QUIT GAME"};
}

std::vector<std::string> startLabels() {
    std::vector<std::string> v;
    if (g_story.hasSave) v.push_back("CONTINUE");
    v.push_back("NEW GAME");
    v.push_back("SANDBOX");
    v.push_back("BACK");
    return v;
}

std::vector<std::string> settingsLabels() {
    const StoryState& G = g_story;
    const char* sp[3] = {"SLOW", "NORMAL", "FAST"};
    char vol[24], sens[24];
    std::snprintf(vol, sizeof vol, "%d%%", G.volume * 10);
    std::snprintf(sens, sizeof sens, "%.2fX", G.sens);
    return {std::string("FULLSCREEN: ") + (G.fullscreen ? "ON" : "OFF"), std::string("TEXT SPEED: ") + sp[G.textSpeed],
            std::string("MASTER VOLUME: ") + vol, std::string("MOUSE SENSITIVITY: ") + sens, std::string("SHOW FPS: ") + (G.showFps ? "ON" : "OFF"), "BACK"};
}

float columnX(const AppContext& app, int col) {
    int u = storyUnit(app);
    return (col == 0 ? 40.0f : 300.0f + 40.0f * (1.0f - g_story.subSlide)) * (float)u;
}

std::vector<SRect> columnRects(const AppContext& app, int count, int col) {
    float H = (float)app.swapchainExtent.height;
    int u = storyUnit(app);
    std::vector<SRect> v;
    float y = 0.46f * H;
    float x = columnX(app, col);
    float w = (col == 0 ? 240.0f : 460.0f) * (float)u;
    for (int i = 0; i < count; i++) {
        v.push_back({x, y, x + w, y + 40.0f * (float)u});
        y += 46.0f * (float)u;
    }
    return v;
}

std::vector<SRect> cardRects(float W, float H, int u) {
    std::vector<SRect> v;
    float gap = 18.0f * (float)u;
    float cw = std::min((W - 80.0f * (float)u - 3.0f * gap) / 4.0f, 300.0f * (float)u);
    float total = 4.0f * cw + 3.0f * gap;
    float x = 0.5f * (W - total);
    float ch = 380.0f * (float)u;
    float y = 0.5f * (H - ch) + 10.0f * (float)u;
    for (int i = 0; i < 4; i++) {
        v.push_back({x, y, x + cw, y + ch});
        x += cw + gap;
    }
    return v;
}

void animate(int count, int sel) {
    for (int i = 0; i < 8; i++) {
        float target = i == sel && i < count ? 1.0f : 0.0f;
        g_story.hover[i] += (target - g_story.hover[i]) * std::min(1.0f, g_dt * 12.0f);
    }
}

void tick() {
    g_story.subSlide += ((g_story.screen == 3 ? 1.0f : 0.0f) - g_story.subSlide) * std::min(1.0f, g_dt * 10.0f);
    double now = glfwGetTime();
    g_dt = g_prevT < 0.0 ? 0.016f : (float)std::min(0.1, now - g_prevT);
    g_prevT = now;
}

}

void storyToggleFullscreen(AppContext& app) {
    static int wx = 100, wy = 100, ww = 1280, wh = 720;
    GLFWwindow* w = app.window;
    StoryState& G = g_story;
    if (!G.fullscreen) {
        glfwGetWindowPos(w, &wx, &wy);
        glfwGetWindowSize(w, &ww, &wh);
        GLFWmonitor* m = glfwGetPrimaryMonitor();
        const GLFWvidmode* vm = m ? glfwGetVideoMode(m) : nullptr;
        if (!vm) return;
        glfwSetWindowMonitor(w, m, 0, 0, vm->width, vm->height, vm->refreshRate);
        G.fullscreen = true;
    } else {
        glfwSetWindowMonitor(w, nullptr, wx, wy, ww, wh, 0);
        G.fullscreen = false;
    }
}

int storyTitleUpdate(AppContext& app) {
    int flags = 0;
    StoryState& G = g_story;
    tick();
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    int u = storyUnit(app);
    bool esc = storyEdge(app, GLFW_KEY_ESCAPE);
    if (G.screen == 1) {
        std::vector<SRect> cards = cardRects(W, H, u);
        bool left = storyEdge(app, GLFW_KEY_LEFT) || storyEdge(app, GLFW_KEY_A);
        bool right = storyEdge(app, GLFW_KEY_RIGHT) || storyEdge(app, GLFW_KEY_D);
        Nav n = storyReadNav(app, cards);
        if (left) G.optSel = (G.optSel + 3) % 4;
        if (right) G.optSel = (G.optSel + 1) % 4;
        if (n.hoverIdx >= 0) G.optSel = n.hoverIdx;
        if (n.clickIdx >= 0) G.optSel = n.clickIdx;
        animate(4, G.optSel);
        if (esc) {
            G.screen = 3;
            storyNavReset(app);
        } else if (n.activate || n.clickIdx >= 0) {
            G.gameMode = G.optSel;
            G.screen = 0;
            storyBegin(app, 0, 0, true, flags);
        }
        return flags;
    }
    if (G.screen == 2) {
        std::vector<SRect> rects = columnRects(app, 6, 0);
        for (SRect& r : rects) r.x1 = r.x0 + 520.0f * (float)u;
        bool left = storyEdge(app, GLFW_KEY_LEFT);
        bool right = storyEdge(app, GLFW_KEY_RIGHT);
        Nav n = storyReadNav(app, rects);
        G.optSel = storyApplyNav(n, std::min(G.optSel, 5), 6);
        animate(6, G.optSel);
        int d = right ? 1 : left ? -1 : 0;
        bool act = n.activate || n.clickIdx >= 0;
        bool changed = true;
        if (esc || (act && G.optSel == 5)) {
            G.screen = 0;
            G.sel = 1;
            storySettingsSave();
            storyNavReset(app);
            changed = false;
        } else if (G.optSel == 0 && (act || d)) {
            storyToggleFullscreen(app);
        } else if (G.optSel == 1 && (act || d)) {
            G.textSpeed = (G.textSpeed + (d ? d : 1) + 3) % 3;
        } else if (G.optSel == 2 && (act || d)) {
            G.volume = d ? std::max(0, std::min(10, G.volume + d)) : (G.volume + 1) % 11;
        } else if (G.optSel == 3 && (act || d)) {
            G.sens = d ? std::max(0.25f, std::min(3.0f, G.sens + 0.25f * (float)d)) : (G.sens >= 3.0f ? 0.25f : G.sens + 0.25f);
        } else if (G.optSel == 4 && (act || d)) {
            G.showFps = !G.showFps;
        } else {
            changed = false;
        }
        if (changed) storySettingsSave();
        return flags;
    }
    if (G.screen == 3) {
        std::vector<std::string> labels = startLabels();
        int count = (int)labels.size();
        std::vector<SRect> rects = columnRects(app, count, 1);
        Nav n = storyReadNav(app, rects);
        G.subSel = storyApplyNav(n, std::min(G.subSel, count - 1), count);
        animate(count, G.subSel);
        if (esc) {
            G.screen = 0;
            storyNavReset(app);
        } else if (n.activate || n.clickIdx >= 0) {
            const std::string& label = labels[G.subSel];
            if (label == "BACK") {
                G.screen = 0;
            } else if (label == "SANDBOX") {
                G.mode = SM_OFF;
                G.screen = 0;
            } else if (label == "CONTINUE") {
                if (storyLoadFile()) storyBegin(app, G.ch, G.obj, false, flags);
                else storyBegin(app, 0, 0, true, flags);
            } else {
                G.screen = 1;
                G.optSel = G.gameMode;
            }
            storyNavReset(app);
        }
        return flags;
    }
    std::vector<SRect> rects = columnRects(app, 3, 0);
    Nav n = storyReadNav(app, rects);
    G.sel = storyApplyNav(n, std::min(G.sel, 2), 3);
    animate(3, G.sel);
    if (!(n.activate || n.clickIdx >= 0)) return flags;
    if (G.sel == 0) {
        G.screen = 3;
        G.subSel = 0;
        storyNavReset(app);
    } else if (G.sel == 1) {
        G.screen = 2;
        G.optSel = 0;
        storyNavReset(app);
    } else {
        glfwSetWindowShouldClose(app.window, 1);
    }
    return flags;
}

namespace {

void drawColumn(const AppContext& app, SPen& p, const std::vector<std::string>& labels, int sel, int col, float bright, bool anim) {
    int u = p.u;
    std::vector<SRect> rects = columnRects(app, (int)labels.size(), col);
    for (size_t i = 0; i < labels.size(); i++) {
        float h = anim ? g_story.hover[i] : ((int)i == sel ? 1.0f : 0.0f);
        float c[3];
        for (int k = 0; k < 3; k++) c[k] = (sc::DIM[k] * 0.8f * (1.0f - h) + sc::WHITE[k] * h) * bright;
        float x = rects[i].x0 + 16.0f * (float)u + 12.0f * (float)u * h;
        float y = rects[i].y0 + 0.5f * (40.0f * (float)u - 21.0f * (float)u);
        if (h > 0.04f) {
            float bar[3] = {sc::GOLD[0] * h * bright, sc::GOLD[1] * h * bright, sc::GOLD[2] * h * bright};
            p.rect(rects[i].x0 + 4.0f * (float)u, y - 2.0f * (float)u, rects[i].x0 + 7.0f * (float)u, y + 23.0f * (float)u, bar);
        }
        p.text(labels[i], x, y, 3 * u, c);
    }
}

void drawLogo(SPen& p, const char* sub) {
    int u = p.u;
    float x = 56.0f * (float)u;
    p.text("STRANDED", x, 0.12f * p.H, 10 * u, sc::WHITE);
    p.rect(x, 0.12f * p.H + 84.0f * (float)u, x + SPen::width("STRANDED", 10 * u) - 10.0f * (float)u, 0.12f * p.H + 85.0f * (float)u, sc::EDGE);
    p.text(sub, x + 4.0f * (float)u, 0.12f * p.H + 96.0f * (float)u, 2 * u, sc::DIM);
}

}

void storyDrawTitle(const AppContext& app, SPen& p) {
    const StoryState& G = g_story;
    int u = p.u;
    p.k = storyFade();
    if (G.screen == 1) {
        storyDrawModes(app, p);
        return;
    }
    drawLogo(p, "A GRAVSIM STORY");
    if (G.screen == 2) {
        std::vector<std::string> s = settingsLabels();
        drawColumn(app, p, s, G.optSel, 0, 1.0f, true);
        p.text("UP / DOWN + ENTER      LEFT / RIGHT: ADJUST      ESC: BACK", 56.0f * (float)u, p.H - 56.0f * (float)u, 2 * u, sc::DIM);
    } else if (G.screen == 3) {
        std::vector<std::string> m = mainLabels();
        drawColumn(app, p, m, 0, 0, 0.55f, false);
        std::vector<std::string> s = startLabels();
        drawColumn(app, p, s, G.subSel, 1, G.subSlide, true);
        p.text("ESC: BACK", 56.0f * (float)u, p.H - 56.0f * (float)u, 2 * u, sc::DIM);
    } else {
        drawColumn(app, p, mainLabels(), G.sel, 0, 1.0f, true);
    }
    p.text("STRANDED  V0.1  -  GRAVSIM", 20.0f * (float)u, p.H - 26.0f * (float)u, 2 * u, sc::DIM);
}

void storyDrawModes(const AppContext& app, SPen& p) {
    (void)app;
    const StoryState& G = g_story;
    int u = p.u;
    p.center("SELECT GAME MODE", 0.5f * p.W, 0.07f * p.H, 5 * u, sc::WHITE);
    std::vector<SRect> cards = cardRects(p.W, p.H, u);
    const char* names[4] = {"SURVIVAL", "CREATIVE", "HARDCORE", "FREEDOM"};
    for (int i = 0; i < 4; i++) {
        bool hot = i == G.optSel;
        const float* mc = modeColor(i);
        float lift = hot ? -8.0f * (float)u : 0.0f;
        SRect r{cards[i].x0, cards[i].y0 + lift, cards[i].x1, cards[i].y1 + lift};
        const float bg[3] = {hot ? 0.010f : 0.003f, hot ? 0.018f : 0.007f, hot ? 0.035f : 0.014f};
        p.rect(r, bg);
        float ec[3] = {mc[0] * (hot ? 1.0f : 0.35f), mc[1] * (hot ? 1.0f : 0.35f), mc[2] * (hot ? 1.0f : 0.35f)};
        p.frame(r, (hot ? 3.0f : 1.0f) * (float)u, ec);
        float g = 6.0f * (float)u;
        float gx = 0.5f * (r.x0 + r.x1) - 4.5f * g, gy = r.y0 + 22.0f * (float)u;
        float gc[3] = {mc[0] * (hot ? 1.0f : 0.6f), mc[1] * (hot ? 1.0f : 0.6f), mc[2] * (hot ? 1.0f : 0.6f)};
        for (int yy = 0; yy < 9; yy++)
            for (int xx = 0; xx < 9; xx++)
                if (GLYPH[i][yy][xx] == 'X') p.rect(gx + (float)xx * g, gy + (float)yy * g, gx + (float)(xx + 1) * g, gy + (float)(yy + 1) * g, gc);
        p.center(names[i], 0.5f * (r.x0 + r.x1), r.y0 + 92.0f * (float)u, 3 * u, hot ? sc::WHITE : sc::PDA);
        int maxc = (int)((r.x1 - r.x0 - 24.0f * (float)u) / (12.0f * (float)u));
        std::vector<std::string> lines = storyWrap(DESC[i], maxc);
        float y = r.y0 + 128.0f * (float)u;
        for (const std::string& l : lines) {
            p.text(l, r.x0 + 12.0f * (float)u, y, 2 * u, hot ? sc::PDA : sc::DIM);
            y += 16.0f * (float)u;
        }
    }
    if (G.optSel == GM_HARDCORE) {
        p.center("WARNING: PERMADEATH - IF YOU DIE, YOUR SAVE IS DELETED", 0.5f * p.W, p.H - 92.0f * (float)u, 3 * u, sc::RED);
    }
    p.center("LEFT / RIGHT + ENTER OR CLICK A CARD      ESC: BACK", 0.5f * p.W, p.H - 44.0f * (float)u, 2 * u, sc::DIM);
}
