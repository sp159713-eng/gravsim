#include "help_page.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

const HelpRow GLOBAL_ROWS[] = {
    {1, "GLOBAL", ""},
    {2, "1 - 0, SHIFT + 1-8", "SWITCH SCENE"},
    {2, "Z", "SPACE QUIZ (SEE LAST HELP PAGE)"},
    {2, "H OR ?", "THIS HELP"},
    {2, "F12", "SCREENSHOT TO PICTURES/GRAVSIM"},
    {2, "SHIFT + F12", "START / STOP VIDEO (MAX 60 S)"},
    {2, "ESC", "BACK / CLOSE"},
};

struct Group {
    const char* name;
    std::vector<const char*> heads;
    std::vector<Scene> scenes;
};

const std::vector<Group>& groups() {
    static const std::vector<Group> g = {
        {"SCENE LIST", {"SCENES"}, {}},
        {"CAMERA, SIMULATION, VIEW", {"CAMERA", "SIMULATION", "VIEW"},
         {Scene::SOLAR, Scene::GALAXY, Scene::COLLISION, Scene::BHSIZES, Scene::LIFECYCLE, Scene::ANDROMEDA, Scene::STARSIZES, Scene::EARTHMOON}},
        {"THROW BODIES", {"THROW BODIES"}, {Scene::SANDBOX}},
        {"TIME DILATION", {"TIME DILATION"}, {Scene::TIMEDIL}},
        {"FUTURE SKY", {"FUTURE SKY"}, {Scene::FUTURESKY}},
        {"ECLIPSES", {"ECLIPSES"}, {Scene::ECLIPSE}},
        {"EXPANSION", {"EXPANSION"}, {Scene::EXPANSION}},
        {"BLACK HOLE FEEDING", {"BLACK HOLE FEEDING"}, {Scene::BHDISC}},
        {"ASTEROID IMPACT", {"ASTEROID IMPACT"}, {Scene::IMPACT}},
        {"NIGHT SKY", {"NIGHT SKY"}, {Scene::NIGHTSKY}},
        {"LAGRANGE POINTS", {"LAGRANGE POINTS"}, {Scene::LAGRANGE}},
        {"RIDE A LIGHT BEAM", {"RIDE A LIGHT BEAM"}, {Scene::LIGHTRIDE}},
        {"SPACE QUIZ", {"SPACE QUIZ"}, {}},
    };
    return g;
}

int g_page = 0;
bool g_pgUp = false, g_pgDn = false, g_wasOpen = false;

void appendSection(std::vector<HelpRow>& out, const char* head) {
    int n = 0;
    const HelpRow* r = helpRows(n);
    size_t len = std::strlen(head);
    for (int i = 1; i < n; i++) {
        if (r[i].kind != 1 || std::strncmp(r[i].key, head, len) != 0) continue;
        out.push_back(r[i]);
        for (int j = i + 1; j < n && r[j].kind == 2; j++) out.push_back(r[j]);
        return;
    }
}

void appendGroup(std::vector<HelpRow>& out, const Group& g) {
    for (const char* h : g.heads) appendSection(out, h);
}

float rowH(const HelpRow& r, float ts, bool first) {
    float gap = first ? 0.0f : (r.kind == 1 ? 6.0f : r.kind == 0 ? 4.0f : 0.0f);
    return (gap + 10.0f) * ts;
}

float spanH(const std::vector<HelpRow>& rows, int a, int b, float ts) {
    float h = 0.0f;
    for (int i = a; i < b; i++) h += rowH(rows[i], ts, i == a);
    return h;
}

void splitCols(const std::vector<HelpRow>& rows, int cols, float ts, int out[4]) {
    int n = (int)rows.size();
    out[0] = 0;
    out[cols] = n;
    if (cols == 1) return;
    float best = 1e9f;
    if (cols == 2) {
        for (int k = 1; k < n; k++) {
            if (rows[k - 1].kind == 1) continue;
            float m = std::max(spanH(rows, 0, k, ts), spanH(rows, k, n, ts));
            if (m < best) { best = m; out[1] = k; }
        }
    } else {
        for (int k = 1; k < n; k++) {
            if (rows[k - 1].kind == 1) continue;
            for (int l = k + 1; l < n; l++) {
                if (rows[l - 1].kind == 1) continue;
                float m = std::max(spanH(rows, 0, k, ts), std::max(spanH(rows, k, l, ts), spanH(rows, l, n, ts)));
                if (m < best) { best = m; out[1] = k; out[2] = l; }
            }
        }
    }
}

bool tryFit(HelpPage& p, float W, float H, float topY, float ts, int cols) {
    int n = (int)p.rows.size();
    if (cols > n) return false;
    int cf[4] = {0, 0, 0, 0};
    splitCols(p.rows, cols, ts, cf);
    float pad = 8.0f * ts;
    float gap = 18.0f * ts;
    float x = 0.0f, bodyH = 0.0f, widest = 0.0f;
    float colX[3] = {0, 0, 0}, colKey[3] = {0, 0, 0};
    for (int c = 0; c < cols; c++) {
        size_t mk = 0, mt = 0, mh = 0;
        for (int i = cf[c]; i < cf[c + 1]; i++) {
            const HelpRow& r = p.rows[i];
            if (r.kind == 2) {
                mk = std::max(mk, std::strlen(r.key));
                mt = std::max(mt, std::strlen(r.text));
            } else mh = std::max(mh, std::strlen(r.key));
        }
        float kw = (float)(mk + 2) * 6.0f * ts;
        float cw = std::max(kw + (float)mt * 6.0f * ts, (float)mh * 6.0f * ts);
        colX[c] = x;
        colKey[c] = kw;
        x += cw + gap;
        bodyH = std::max(bodyH, spanH(p.rows, cf[c], cf[c + 1], ts));
    }
    float bodyW = x - gap;
    widest = std::max(bodyW, std::max((float)p.title.size(), (float)p.footer.size()) * 6.0f * ts);
    float pw = 2.0f * pad + widest;
    float titleH = 14.0f * ts, footH = 14.0f * ts;
    float ph = 2.0f * pad + titleH + bodyH + footH;
    if (pw > W - 24.0f || topY + ph > H - 12.0f) return false;
    p.cols = cols;
    for (int i = 0; i < 4; i++) p.colFirst[i] = cf[i];
    for (int i = 0; i < 3; i++) { p.colX[i] = colX[i]; p.colKey[i] = colKey[i]; }
    p.ts = ts;
    p.pad = pad;
    p.x0 = std::max(12.0f, (W - pw) * 0.5f);
    p.y0 = std::max(topY, (H - ph) * 0.5f);
    p.x1 = p.x0 + pw;
    p.y1 = p.y0 + ph;
    p.bodyY = p.y0 + pad + titleH;
    p.footY = p.y1 - pad - 10.0f * ts;
    return true;
}

void build(HelpPage& p, const AppContext& app, float W, float H, float topY, int page) {
    p = HelpPage();
    const auto& gs = groups();
    int total = (int)gs.size();
    if (page == 0) {
        p.title = "GRAVSIM CONTROLS";
        for (const HelpRow& r : GLOBAL_ROWS) p.rows.push_back(r);
        for (const Group& g : gs)
            if (std::find(g.scenes.begin(), g.scenes.end(), app.scene) != g.scenes.end()) appendGroup(p.rows, g);
        if (app.scene == Scene::SOLAR || app.scene == Scene::GALAXY) appendGroup(p.rows, gs[2]);
        p.footer = "PAGE UP/DOWN FOR ALL SCENES    H OR ESC CLOSES";
    } else {
        const Group& g = gs[page - 1];
        p.title = std::string("GRAVSIM CONTROLS - ") + g.name;
        appendGroup(p.rows, g);
        p.footer = "PAGE " + std::to_string(page) + " OF " + std::to_string(total) + "    PAGE UP/DOWN FOR MORE    H OR ESC CLOSES";
    }
    for (float ts = 2.0f; ts >= 0.79f; ts -= 0.1f)
        for (int cols = 1; cols <= 3; cols++)
            if (tryFit(p, W, H, topY, ts, cols)) return;
    tryFit(p, 1e9f, 1e9f, topY, 0.8f, 3);
}

}

const HelpPage& helpPage(const AppContext& app, float topY) {
    static HelpPage p;
    static int kScene = -1, kPage = -1;
    static float kW = 0, kH = 0, kTop = 0;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    if (kScene != (int)app.scene || kPage != g_page || kW != W || kH != H || kTop != topY) {
        build(p, app, W, H, topY, g_page);
        kScene = (int)app.scene;
        kPage = g_page;
        kW = W;
        kH = H;
        kTop = topY;
    }
    return p;
}

void helpKeys(AppContext& app) {
    if (app.showHelp && !g_wasOpen) g_page = 0;
    g_wasOpen = app.showHelp;
    bool up = glfwGetKey(app.window, GLFW_KEY_PAGE_UP) == GLFW_PRESS;
    bool dn = glfwGetKey(app.window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS;
    int total = (int)groups().size() + 1;
    if (app.showHelp) {
        if (dn && !g_pgDn) g_page = (g_page + 1) % total;
        if (up && !g_pgUp) g_page = (g_page + total - 1) % total;
    }
    g_pgUp = up;
    g_pgDn = dn;
}
