#include "story.h"
#include "story_ui.h"
#include <cctype>
#include <cmath>
#include <cstring>

namespace {

OverlayVertex g_tri[MAX_VERTS];
OverlayVertex g_txt[MAX_VERTS];
OverlayVertex g_keep[MAX_VERTS];

}

int storyUnit(const AppContext& app) {
    return std::max(1, (int)std::lround((float)app.swapchainExtent.height / 720.0f));
}

std::string storyClean(const char* s) {
    std::string o;
    for (const char* p = s; *p; p++) {
        char c = (char)std::toupper((unsigned char)*p);
        o.push_back(c == ' ' || glyph(c) ? c : '-');
    }
    return o;
}

std::vector<std::string> storyWrap(const std::string& s, int maxChars) {
    std::vector<std::string> lines;
    std::string cur;
    size_t i = 0;
    maxChars = std::max(4, maxChars);
    while (i <= s.size()) {
        size_t j = s.find(' ', i);
        if (j == std::string::npos) j = s.size();
        std::string w = s.substr(i, j - i);
        if (!cur.empty() && (int)(cur.size() + 1 + w.size()) > maxChars) {
            lines.push_back(cur);
            cur.clear();
        }
        if (!cur.empty()) cur.push_back(' ');
        cur += w;
        i = j + 1;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

void storyDrawHud(const AppContext& app, SPen& p) {
    const StoryState& G = g_story;
    const Chapter& C = storyChapters()[G.ch];
    int u = p.u, ts = 2 * u;
    float pad = 7.0f * (float)u, line = 9.0f * (float)ts, cw = 6.0f * (float)ts;
    float w = 330.0f * (float)u;
    float x1 = p.W - 14.0f * (float)u, x0 = x1 - w;
    float y0 = 54.0f * (float)u;
    int maxc = (int)((w - 2.0f * pad) / cw);
    std::vector<std::string> cur;
    std::string head = std::string("CH ") + std::to_string(G.ch + 1) + "  " + C.title;
    if (G.complete) cur = storyWrap("END OF DEMO - CHAPTER 2 COMING SOON", maxc);
    else cur = storyWrap(storyClean(C.objs[G.obj].text), maxc - 2);
    std::vector<std::string> hint;
    if (!G.complete && C.objs[G.obj].hint) hint = storyWrap(storyClean(C.objs[G.obj].hint), maxc);
    std::vector<std::vector<std::string>> list;
    for (size_t i = 0; i < C.objs.size(); i++) list.push_back(storyWrap(storyClean(C.objs[i].text), maxc - 4));
    float h = pad * 2.0f + line * 1.6f + line * (float)cur.size() + (hint.empty() ? 0.0f : line * (float)hint.size() + 4.0f * (float)u) + 8.0f * (float)u;
    for (const auto& l : list) h += line * (float)l.size();
    SRect r{x0, y0, x1, y0 + h};
    p.rect(r, sc::PANEL);
    p.frame(r, (float)u, sc::EDGE);
    float y = y0 + pad;
    p.text(head, x0 + pad, y, ts, sc::CYAN);
    {
        const char* mn = storyModeName(G.gameMode);
        p.text(mn, x1 - pad - SPen::width(mn, ts), y, ts, G.gameMode == GM_HARDCORE ? sc::RED : sc::GOLD);
    }
    y += line * 1.6f;
    for (size_t i = 0; i < cur.size(); i++) {
        p.text((i == 0 && !G.complete ? "* " : "  ") + cur[i], x0 + pad, y, ts, sc::GOLD);
        y += line;
    }
    if (!hint.empty()) {
        y += 2.0f * (float)u;
        for (const std::string& hl : hint) {
            p.text(hl, x0 + pad, y, ts, sc::DIM);
            y += line;
        }
        y += 2.0f * (float)u;
    }
    p.rect(x0 + pad, y + 2.0f * (float)u, x1 - pad, y + 3.0f * (float)u, sc::EDGE);
    y += 6.0f * (float)u;
    for (size_t i = 0; i < list.size(); i++) {
        bool done = G.complete || (int)i < G.obj;
        bool now = !G.complete && (int)i == G.obj;
        const float* col = done ? sc::GREEN : now ? sc::WHITE : sc::DIM;
        const char* mark = done ? "[X] " : now ? "[*] " : "[ ] ";
        for (size_t k = 0; k < list[i].size(); k++) {
            p.text((k == 0 ? std::string(mark) : std::string("    ")) + list[i][k], x0 + pad, y, ts, col);
            y += line;
        }
    }
    if (G.saveToast > 0.0f) p.text("CHECKPOINT SAVED", x1 - pad - SPen::width("CHECKPOINT SAVED", ts), y0 + h + 6.0f * (float)u, ts, sc::DIM);
    if (G.doneToast > 0.0f) {
        float bw = SPen::width("OBJECTIVE COMPLETE", 3 * u) + 28.0f * (float)u, bh = 30.0f * (float)u;
        SRect b{0.5f * (p.W - bw), 14.0f * (float)u, 0.5f * (p.W + bw), 14.0f * (float)u + bh};
        p.rect(b, sc::PANEL);
        p.frame(b, (float)u, sc::GREEN);
        p.center("OBJECTIVE COMPLETE", 0.5f * p.W, b.y0 + 8.0f * (float)u, 3 * u, sc::GREEN);
    }
    (void)app;
}

void storyDrawPda(const AppContext& app, SPen& p) {
    int cur = pdaCurrent();
    if (cur < 0) return;
    int u = p.u, ts = 2 * u;
    float pad = 8.0f * (float)u, line = 10.0f * (float)u, cw = 6.0f * (float)ts;
    float pw = std::min(p.W * 0.46f, 560.0f * (float)u);
    int maxc = (int)((pw - 2.0f * pad) / cw);
    std::string full = storyClean(storyLogAt(cur).text);
    std::vector<std::string> lines = storyWrap(full, maxc);
    float headH = 13.0f * (float)ts;
    float h = headH + pad * 2.0f + line * 2.0f * (float)lines.size();
    float reserve = app.scene == Scene::SANDBOX ? 128.0f * (float)u : 0.0f;
    float x0 = 14.0f * (float)u, y1 = p.H - 14.0f * (float)u - reserve;
    SRect r{x0, y1 - h, x0 + pw, y1};
    p.rect(r, sc::BG);
    p.rect(r.x0, r.y0, r.x1, r.y0 + headH, sc::HEAD);
    p.frame(r, (float)u, sc::EDGE);
    bool blink = pdaTyping() ? std::fmod((float)glfwGetTime() * 6.0f, 2.0f) < 1.0f : true;
    p.rect(r.x0 + pad, r.y0 + 0.5f * (headH - 6.0f * (float)u), r.x0 + pad + 6.0f * (float)u, r.y0 + 0.5f * (headH + 6.0f * (float)u), blink ? sc::GREEN : sc::HEAD);
    p.text("PDA", r.x0 + pad + 12.0f * (float)u, r.y0 + 0.5f * (headH - 7.0f * (float)ts), ts, sc::PDA);
    std::string hint = "SPACE: NEXT   L: LOG";
    p.text(hint, r.x1 - pad - SPen::width(hint, ts), r.y0 + 0.5f * (headH - 7.0f * (float)ts), ts, sc::DIM);
    int shown = (int)pdaShown();
    int used = 0;
    bool placed = false;
    bool typing = pdaTyping();
    bool cursorOn = std::fmod((float)glfwGetTime() * 3.0f, 2.0f) < 1.0f;
    float y = r.y0 + headH + pad;
    for (size_t i = 0; i < lines.size(); i++) {
        const std::string& l = lines[i];
        int n = std::max(0, std::min((int)l.size(), shown - used));
        std::string part = l.substr(0, (size_t)n);
        if (typing && !placed && (n < (int)l.size() || i + 1 == lines.size())) {
            placed = true;
            if (cursorOn) part.push_back(0x23);
        }
        p.text(part, r.x0 + pad, y, ts, sc::PDA);
        used += (int)l.size() + 1;
        y += line * 2.0f;
    }
}

void storyOverlay(AppContext& app) {
    StoryState& G = g_story;
    if (G.mode == SM_OFF && !G.showFps) return;
    int f = app.currentFrame;
    OverlayVertex* out = static_cast<OverlayVertex*>(app.overlayMapped[f]);
    SPen pen;
    pen.tri = g_tri;
    pen.txt = g_txt;
    pen.W = (float)app.swapchainExtent.width;
    pen.H = (float)app.swapchainExtent.height;
    pen.u = storyUnit(app);
    bool modal = storyModal();
    if (G.mode == SM_TITLE) {
        storyDrawTitle(app, pen);
    } else if (G.mode == SM_STORY) {
        if (G.historyOpen) {
            storyDrawHistory(app, pen);
        } else if (G.menuOpen) {
            storyDrawPause(app, pen);
        } else {
            storyDrawHud(app, pen);
            storyDrawPda(app, pen);
        }
    }
    if (G.showFps) {
        static double last = 0.0;
        static int frames = 0;
        static int fps = 0;
        double now = glfwGetTime();
        frames++;
        if (now - last >= 0.5) {
            fps = (int)std::lround(frames / (now - last));
            frames = 0;
            last = now;
        }
        pen.k = 1.0f;
        pen.text(std::to_string(fps) + " FPS", 12.0f * (float)pen.u, 10.0f * (float)pen.u, 2 * pen.u, sc::GREEN);
    }
    uint32_t L = app.overlayLineVerts[f], P = app.overlayPointVerts[f], T = app.overlayTriVerts[f], U = app.overlayUiVerts[f];
    uint32_t used = 0;
    if (modal) {
        L = P = T = U = 0;
    } else {
        std::memcpy(g_keep, out + L + P + T, sizeof(OverlayVertex) * U);
        used = L + P + T;
    }
    uint32_t nTri = std::min(pen.nTri, MAX_VERTS - used);
    std::memcpy(out + used, g_tri, sizeof(OverlayVertex) * nTri);
    used += nTri;
    uint32_t nKeep = std::min(U, MAX_VERTS - used);
    if (!modal) std::memcpy(out + used, g_keep, sizeof(OverlayVertex) * nKeep);
    else nKeep = 0;
    used += nKeep;
    uint32_t nTxt = std::min(pen.nTxt, MAX_VERTS - used);
    std::memcpy(out + used, g_txt, sizeof(OverlayVertex) * nTxt);
    app.overlayLineVerts[f] = L;
    app.overlayPointVerts[f] = P;
    app.overlayTriVerts[f] = T + nTri;
    app.overlayUiVerts[f] = nKeep + nTxt;
}
