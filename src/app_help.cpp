#include "vk_app.h"
#include "help_page.h"
#include "milkyway.h"
#include "quiz.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

using Row = HelpRow;

const Row ROWS[] = {
    {0, "GRAVSIM CONTROLS", ""},
    {1, "SCENES", ""},
    {2, "1", "SOLAR SYSTEM"},
    {2, "2", "GALAXY"},
    {2, "3", "COLLISION"},
    {2, "4", "BLACK HOLE SIZES"},
    {2, "5", "STAR LIFE CYCLE"},
    {2, "6", "MILKY WAY + ANDROMEDA"},
    {2, "7", "STAR SIZES"},
    {2, "8", "SANDBOX: THROW PLANETS"},
    {2, "9", "EARTH AND MOON: PHASES + TIDES"},
    {2, "0", "TIME DILATION: PROBE NEAR A BLACK HOLE"},
    {2, "SHIFT + 1", "LAGRANGE POINTS: SUN-EARTH / SUN-JUPITER"},
    {2, "SHIFT + 2", "RIDE A LIGHT BEAM: LEAVE THE SUN AT 299,792 KM/S"},
    {2, "SHIFT + 3", "ANDROMEDA IN OUR FUTURE SKY: NOW TO +7 BILLION YEARS"},
    {2, "SHIFT + 4", "NIGHT SKY: STARS FROM EARTH, THEN FROM ANOTHER STAR"},
    {2, "SHIFT + 5", "ASTEROID IMPACT: ENERGY, CRATER, BLAST RADII"},
    {2, "SHIFT + 6", "ECLIPSES: SHADOW CONES, SOLAR AND LUNAR"},
    {2, "SHIFT + 7", "EXPANSION OF THE UNIVERSE: BIG BANG TO THE FAR FUTURE"},
    {2, "SHIFT + 8", "BLACK HOLE FEEDING: STAR TORN APART, DISC FORMS AND DRAINS"},
    {2, "Z", "SPACE QUIZ: 10 QUESTIONS, THE SIM SHOWS EACH ANSWER"},
    {1, "CAMERA", ""},
    {2, "W A S D", "MOVE (FREE-FLY)"},
    {2, "Q / E", "DOWN / UP"},
    {2, "DRAG MOUSE", "LOOK AROUND"},
    {2, "SCROLL", "ZOOM OUT TO THE UNIVERSE"},
    {2, "SHIFT + SCROLL", "FLY SPEED"},
    {2, "SHIFT", "BOOST"},
    {2, "PAGEUP / PAGEDOWN", "ZOOM OUT / IN, SHIFT = FASTER"},
    {2, "CLICK PLANET", "FOLLOW IT + INFO CARD"},
    {2, "ESC", "BACK TO FREE-FLY"},
    {2, "CLICK STAR", "VISIT IT (MILKY WAY)"},
    {2, "CLICK GALAXY", "FLY THERE (ZOOMED OUT)"},
    {2, "CLICK LABEL", "CLOSE-UP: NEBULA, SGR A*, BLACK HOLE"},
    {2, "BACKSPACE", "HOME (SUN SYSTEM)"},
    {1, "SIMULATION", ""},
    {2, "+ / -", "SIM SPEED (DAYS PER SECOND; SCENE 9: ORBIT SPEED)"},
    {2, ", / .", "SPEED PRESET: 1 DAY, WEEK, MONTH, YEAR"},
    {2, "J", "JUMP TO A DATE (YYYY OR YYYY-MM-DD)"},
    {2, "SPACE", "PAUSE"},
    {2, "R", "REPLAY BLACK HOLE TOUR / LIFE CYCLE / STAR SIZES"},
    {2, "P", "POWERS OF TEN TOUR  (SCENE 8: NEXT PRESET, SHIFT BACK)"},
    {2, "T", "TRUE SCALE / ENLARGED"},
    {1, "VIEW", ""},
    {2, "G", "GRID"},
    {2, "L", "LABELS"},
    {2, "B", "SKYBOX"},
    {2, "M", "MILKY WAY  (SCENE 5: CLASSIC MASS PRESETS)"},
    {2, "LEFT / RIGHT", "SCENE 5: STAR MASS 0.1 TO 100 SUNS"},
    {2, "LEFT / RIGHT", "SCENE 7: PREVIOUS / NEXT STAR"},
    {2, "U", "UNIVERSE (ZOOM OUT PAST GALAXY)"},
    {2, "V", "REDSHIFT COLOURS ON THE UNIVERSE LAYER, HOVER A LABEL FOR Z"},
    {2, "O", "ORBIT LINES"},
    {2, "K", "BLOOM"},
    {2, "X", "DARK MATTER  (SCENE 8: SUN OFF / ON)"},
    {2, "N", "DELETE / RESTORE THE SUN (SCENE 1)"},
    {2, "Y", "ASTEROID + KUIPER BELTS ON / OFF (SCENE 1)"},
    {2, "CLICK COMET / VOYAGER", "INFO CARD (HALE-BOPP, ENCKE, VOYAGERS)"},
    {1, "TIME DILATION (SCENE 0)", ""},
    {2, "UP / DOWN OR SCROLL", "MOVE PROBE TOWARD / AWAY FROM THE HORIZON"},
    {2, "LEFT / RIGHT", "PROBE SPEED (SPECIAL RELATIVITY)"},
    {2, "I", "MILLER'S PLANET PRESET (1 HOUR = 7 YEARS)"},
    {1, "FUTURE SKY (SHIFT + 3)", ""},
    {2, "LEFT / RIGHT", "SCRUB TIME, SHIFT = FASTER (OR DRAG THE SLIDER)"},
    {2, "SPACE", "AUTO-PLAY, ABOUT 1 BILLION YEARS PER 8 SECONDS"},
    {2, "ESC", "EXIT TO THE SOLAR SYSTEM"},
    {1, "ECLIPSES (SHIFT + 6)", ""},
    {2, "TAB", "VIEW: SPACE / SOLAR FROM GROUND / LUNAR"},
    {2, "LEFT / RIGHT", "STEP THE MONTH OR THE ECLIPSE, SHIFT = FASTER"},
    {2, "SPACE", "PLAY / PAUSE"},
    {2, "N", "SPACE VIEW: ECLIPSE SEASON / EDGE / OFF SEASON"},
    {2, "M", "GROUND VIEW: MOON AT PERIGEE / APOGEE (ANNULAR)"},
    {2, "W", "GROUND VIEW: SWAP CLOSE-UP AND ALL-SKY"},
    {2, "DRAG / SCROLL", "SPACE VIEW: TURN / ZOOM"},
    {2, "ESC", "EXIT TO THE SOLAR SYSTEM"},
    {1, "EXPANSION (SHIFT + 7)", ""},
    {2, "LEFT / RIGHT", "SCRUB TIME, SHIFT = FASTER (LOG SECONDS, THEN LINEAR GYR)"},
    {2, "UP / DOWN", "PREVIOUS / NEXT ERA"},
    {2, "SPACE", "PLAY / PAUSE THE COSMIC HISTORY"},
    {2, "N", "JUMP TO NOW (13.8 BILLION YEARS)"},
    {2, "ESC", "EXIT TO THE SOLAR SYSTEM"},
    {1, "BLACK HOLE FEEDING (SHIFT + 8)", ""},
    {2, "SPACE", "PAUSE / PLAY"},
    {2, "LEFT / RIGHT", "SCRUB TIME, SHIFT = FASTER"},
    {2, "R", "RESTART THE SAME FEEDING"},
    {2, "1 / 2 / 3", "FEED: STAR / GAS CLOUD / COMPANION"},
    {2, "DRAG / SCROLL", "ORBIT / ZOOM"},
    {2, "ESC", "EXIT TO THE SOLAR SYSTEM"},
    {1, "ASTEROID IMPACT (SHIFT + 5)", ""},
    {2, "[ / ]", "ASTEROID DIAMETER 10 M TO 15 KM"},
    {2, "; / '", "SPEED 11 TO 72 KM/S"},
    {2, ", / .", "IMPACT ANGLE 15 TO 90 DEG"},
    {2, "TAB", "TYPE: STONE / IRON / COMET ICE"},
    {2, "1 - 5", "CHELYABINSK, TUNGUSKA, METEOR CRATER, CHICXULUB, APOPHIS"},
    {2, "SPACE", "LAUNCH / REPLAY"},
    {2, "CLICK GLOBE", "CHOOSE THE TARGET (DEFAULT NEW DELHI)"},
    {2, "DRAG / SCROLL", "ROTATE / ZOOM THE GLOBE"},
    {2, "ESC", "EXIT TO THE SOLAR SYSTEM"},
    {1, "NIGHT SKY (SHIFT + 4)", ""},
    {2, "DRAG / SCROLL", "LOOK AROUND / ZOOM THE FIELD OF VIEW"},
    {2, "+ / -", "RUN TIME OF NIGHT, SHIFT = FASTER"},
    {2, "L", "PLACE: NEW DELHI, EQUATOR, LONDON, SYDNEY"},
    {2, "C", "FADE CONSTELLATION LINES + NAMES ON / OFF"},
    {2, "T", "SEE IT FROM ANOTHER STAR: PROXIMA, SIRIUS, VEGA, BETELGEUSE"},
    {2, "HOVER STAR", "NAME, DISTANCE, WHEN ITS LIGHT LEFT"},
    {2, "ESC", "EXIT TO THE SOLAR SYSTEM"},
    {1, "LAGRANGE POINTS (SHIFT + 1)", ""},
    {2, "HOVER / CLICK POINT", "INFO CARD FOR L1 TO L5"},
    {2, "CLICK SPACE", "DROP A TEST PARTICLE (RK4 IN THE ROTATING FRAME)"},
    {2, "TAB", "SWITCH SUN-EARTH / SUN-JUPITER (TROJANS)"},
    {2, "SPACE / R / C", "PAUSE / RESET / CLEAR TEST PARTICLES"},
    {1, "RIDE A LIGHT BEAM (SHIFT + 2)", ""},
    {2, "+ / -", "TIME WARP FASTER / SLOWER (1X TO 10,000,000X)"},
    {2, "TAB", "JUMP TO THE NEXT MILESTONE"},
    {2, "A / V / R", "AUTO-CRUISE / LOOK BACK / RESTART"},
    {2, "DRAG MOUSE", "LOOK AROUND"},
    {2, "ESC", "BACK TO THE SOLAR SYSTEM"},
    {1, "SPACE QUIZ (Z)", ""},
    {2, "Z", "START THE QUIZ FROM ANY SCENE (10 OF ~70 QUESTIONS)"},
    {2, "1 - 4 OR CLICK", "PICK AN ANSWER; THE SIM JUMPS TO THE SCENE THAT SHOWS IT"},
    {2, "N OR ENTER", "NEXT QUESTION / PLAY AGAIN AFTER THE FINAL SCORE"},
    {2, "MOUSE / WASD", "LOOK AROUND THE SHOWN SCENE BEFORE MOVING ON"},
    {2, "ESC", "QUIT THE QUIZ AND RETURN TO WHERE YOU WERE"},
    {1, "THROW BODIES (SCENE 8; F IN SCENES 1 + 2)", ""},
    {2, "F", "THROW MODE ON / OFF (SCENES 1 + 2)"},
    {2, "DRAG", "THROW A BODY (SLINGSHOT)"},
    {2, "CLICK", "DROP IN A CIRCULAR ORBIT"},
    {2, "SHIFT + DRAG", "THROW UP / DOWN (TILTED ORBIT)"},
    {2, "1 2 3 4", "BODY TYPE (OR CLICK THE BAR)"},
    {2, "[ / ] OR SCROLL", "MASS X2 / X0.5  (SCROLL WHILE DRAGGING)"},
    {2, "C", "CLEAR THROWN  (SCENE 8: ALL BUT THE SUN)"},
    {2, "RIGHT DRAG", "LOOK / ORBIT CAM  (SCENE 8: MIDDLE PAN, R RESET)"},
    {0, "H OR ESC OR ? CLOSES THIS PANEL", ""},
};
const int ROW_COUNT = (int)(sizeof(ROWS) / sizeof(ROWS[0]));

struct Rect {
    float x0, y0, x1, y1;
    bool has(float x, float y) const { return x >= x0 && x <= x1 && y >= y0 && y <= y1; }
};

struct Layout {
    Rect button;
    Rect panel;
};

Layout makeLayout(const AppContext& app) {
    Layout l{};
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float s = std::max(24.0f, std::floor(H / 26.0f));
    l.button = {W - 12.0f - s, 12.0f, W - 12.0f, 12.0f + s};
    if (app.showHelp) {
        const HelpPage& p = helpPage(app, l.button.y1 + 12.0f);
        l.panel = {p.x0, p.y0, p.x1, p.y1};
    }
    return l;
}

void toFramebuffer(const AppContext& app, double wx, double wy, float& x, float& y) {
    int ww = 0, wh = 0;
    glfwGetWindowSize(app.window, &ww, &wh);
    x = ww > 0 ? (float)(wx * app.swapchainExtent.width / ww) : (float)wx;
    y = wh > 0 ? (float)(wy * app.swapchainExtent.height / wh) : (float)wy;
}

struct Emitter {
    OverlayVertex* out;
    uint32_t& used;
    float W, H;

    void rect(float x0, float y0, float x1, float y1, const float c[3]) {
        if (used + 6 > MAX_VERTS) return;
        float a = -1.0f + 2.0f * x0 / W, b = -1.0f + 2.0f * y0 / H;
        float e = -1.0f + 2.0f * x1 / W, f = -1.0f + 2.0f * y1 / H;
        out[used++] = {a, b, c[0], c[1], c[2]};
        out[used++] = {e, b, c[0], c[1], c[2]};
        out[used++] = {e, f, c[0], c[1], c[2]};
        out[used++] = {a, b, c[0], c[1], c[2]};
        out[used++] = {e, f, c[0], c[1], c[2]};
        out[used++] = {a, f, c[0], c[1], c[2]};
    }

    void rounded(const Rect& r, float n, const float c[3]) {
        rect(r.x0 + n, r.y0, r.x1 - n, r.y1, c);
        rect(r.x0, r.y0 + n, r.x0 + n, r.y1 - n, c);
        rect(r.x1 - n, r.y0 + n, r.x1, r.y1 - n, c);
    }

    void text(const char* s, float x, float y, float ts, const float c[3]) {
        emitText(out, used, s, -1.0f + 2.0f * x / W, -1.0f + 2.0f * y / H, 2.0f / W, 2.0f / H, ts, c[0], c[1], c[2]);
    }
};

}

const HelpRow* helpRows(int& count) {
    count = ROW_COUNT;
    return ROWS;
}

bool helpButtonHit(const AppContext& app, double wx, double wy) {
    float x, y;
    toFramebuffer(app, wx, wy, x, y);
    return makeLayout(app).button.has(x, y);
}

bool helpPanelHit(const AppContext& app, double wx, double wy) {
    if (!app.showHelp) return false;
    float x, y;
    toFramebuffer(app, wx, wy, x, y);
    return makeLayout(app).panel.has(x, y);
}

void emitHelpUi(AppContext& app, OverlayVertex* out, uint32_t& used) {
    Layout l = makeLayout(app);
    Emitter e{out, used, (float)app.swapchainExtent.width, (float)app.swapchainExtent.height};
    const float panelBg[3] = {0.012f, 0.022f, 0.05f};
    const float buttonBg[3] = {0.04f, 0.07f, 0.14f};
    const float buttonHot[3] = {0.12f, 0.22f, 0.40f};
    const float titleC[3] = {1.0f, 1.0f, 1.0f};
    const float headC[3] = {1.0f, 0.78f, 0.35f};
    const float keyC[3] = {0.55f, 0.85f, 1.0f};
    const float textC[3] = {0.85f, 0.87f, 0.92f};
    const float hintC[3] = {0.6f, 0.65f, 0.75f};
    const float qIdle[3] = {0.7f, 0.85f, 1.0f};
    bool hot = app.helpHover || app.showHelp;
    uint32_t start = used;
    if (app.showHelp) e.rounded(l.panel, 3.0f * helpPage(app, l.button.y1 + 12.0f).ts, panelBg);
    e.rounded(l.button, 4.0f, hot ? buttonHot : buttonBg);
    bool legend = !app.showHelp && !closeupActive(app) && app.tour.mode == 0 && galaxyActive(app) && galaxyDistanceLy(app) >= GALAXY_LABEL_LY && galaxyLayerFade(app) > 0.5f;
    const char* legendTitle = "STAR COLOUR = TYPE / LIFETIME";
    int lts = e.W >= 1000.0f ? 2 : 1;
    float lpad = 8.0f * lts, lrow = 11.0f * lts, lsw = 7.0f * lts;
    float lw = 2.0f * lpad + 29.0f * 6.0f * lts;
    float lh = 2.0f * lpad + lrow * 7.0f + 4.0f * lts;
    float lx = 12.0f, ly = e.H - 12.0f - lh;
    if (legend) {
        e.rounded({lx, ly, lx + lw, ly + lh}, 3.0f * lts, panelBg);
        for (int c = 0; c < 6; c++) {
            float sy = ly + lpad + lrow * (float)(c + 1) + 4.0f * lts;
            e.rect(lx + lpad, sy, lx + lpad + lsw * 1.6f, sy + lsw, SPECTRAL[c].rgb);
        }
    }
    std::vector<CardLine> card = app.showHelp ? std::vector<CardLine>{} : visitCard(app);
    int cts = e.W >= 1000.0f ? 2 : 1;
    float cpad = 8.0f * cts, crow = 11.0f * cts;
    size_t cmax = 0;
    for (const CardLine& c : card) cmax = std::max(cmax, c.text.size());
    float cw = 2.0f * cpad + (float)cmax * 6.0f * cts, ch = 2.0f * cpad + crow * (float)card.size();
    Rect cr = {e.W - 12.0f - cw, l.button.y1 + 10.0f, e.W - 12.0f, l.button.y1 + 10.0f + ch};
    if (!card.empty()) e.rounded(cr, 3.0f * cts, panelBg);
    quizEmitTris(app, out, used);
    app.overlayTriVerts[app.currentFrame] = used - start;
    start = used;
    for (size_t i = 0; i < card.size(); i++)
        e.text(card[i].text.c_str(), cr.x0 + cpad, cr.y0 + cpad + crow * (float)i, cts, card[i].c);
    if (legend) {
        e.text(legendTitle, lx + lpad, ly + lpad, lts, titleC);
        for (int c = 0; c < 6; c++) {
            float ty = ly + lpad + lrow * (float)(c + 1) + 4.0f * lts;
            e.text(SPECTRAL[c].name, lx + lpad + lsw * 1.6f + 6.0f * lts, ty, lts, SPECTRAL[c].rgb);
            e.text(SPECTRAL[c].life, lx + lpad + lsw * 1.6f + 6.0f * lts + 5.0f * 6.0f * lts, ty, lts, textC);
        }
    }
    float bw = l.button.x1 - l.button.x0, bh = l.button.y1 - l.button.y0;
    e.text("?", l.button.x0 + (bw - 10.0f) * 0.5f, l.button.y0 + (bh - 14.0f) * 0.5f, 2, hot ? titleC : qIdle);
    if (app.showHelp) {
        const HelpPage& p = helpPage(app, l.button.y1 + 12.0f);
        e.text(p.title.c_str(), p.x0 + p.pad, p.y0 + p.pad, p.ts, titleC);
        for (int c = 0; c < p.cols; c++) {
            float y = p.bodyY;
            float x = p.x0 + p.pad + p.colX[c];
            for (int i = p.colFirst[c]; i < p.colFirst[c + 1]; i++) {
                const Row& r = p.rows[i];
                if (i != p.colFirst[c]) y += (r.kind == 1 ? 6.0f : r.kind == 0 ? 4.0f : 0.0f) * p.ts;
                if (r.kind == 1) e.text(r.key, x, y, p.ts, headC);
                else if (r.kind == 0) e.text(r.key, x, y, p.ts, hintC);
                else {
                    e.text(r.key, x, y, p.ts, keyC);
                    e.text(r.text, x + p.colKey[c], y, p.ts, textC);
                }
                y += 10.0f * p.ts;
            }
        }
        e.text(p.footer.c_str(), p.x0 + p.pad, p.footY, p.ts, hintC);
    }
    quizEmitText(app, out, used);
    app.overlayUiVerts[app.currentFrame] = used - start;
}
