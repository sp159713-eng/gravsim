#include "vk_app.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <initializer_list>

namespace {

constexpr int MAXL = 24;
constexpr float TAU = 6.2831853f;

struct Piece {
    const char* s;
    float c[3];
};

struct Lab {
    Piece p[6];
    int np = 0;
    float ax = 0.0f, ay = 0.0f, dx = 0.0f, dy = 0.0f, len = 0.0f, a = 1.0f;
    bool leader = true;
    int sc = 2;
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
};

struct View {
    float cx, cy, u, a;
};

struct Ctx {
    Lab l[MAXL];
    int n = 0;
    int sc = 2;
    float W = 0.0f, H = 0.0f, t = 0.0f;
};

const float WHITE[3] = {0.92f, 0.95f, 1.0f};
const float RED[3] = {1.0f, 0.4f, 0.35f};
const float GREEN[3] = {0.35f, 1.0f, 0.5f};
const float BLUE[3] = {0.45f, 0.6f, 1.0f};
const float GREY[3] = {0.78f, 0.8f, 0.86f};
const float GOLD[3] = {1.0f, 0.85f, 0.4f};
const float SOFT[3] = {0.6f, 0.7f, 0.9f};
const float YEL[3] = {0.95f, 0.85f, 0.25f};
const float MAG[3] = {0.95f, 0.45f, 0.75f};
const float GRN[3] = {0.3f, 0.9f, 0.55f};
const float BLU[3] = {0.4f, 0.65f, 1.0f};

const float NUC[11][3] = {
    {0.0532f, -0.2107f, -0.1995f}, {0.1057f, 0.0613f, 0.1141f}, {0.3899f, -0.1293f, -0.2279f},
    {-0.2429f, 0.2098f, 0.1157f}, {0.0459f, -0.3682f, -0.0030f}, {0.4171f, -0.2226f, -0.0174f},
    {-0.1485f, -0.1902f, -0.4646f}, {0.2339f, 0.0175f, -0.3001f}, {0.2654f, 0.2760f, -0.1116f},
    {-0.4641f, -0.0356f, 0.0764f}, {0.3975f, 0.0830f, -0.2378f}};
const int NUC_PROTONS = 860;
const int DNA_PAIRS = 14106858;

Piece pc(const char* s, const float* c) { return Piece{s, {c[0], c[1], c[2]}}; }

void add(Ctx& c, const View& v, float qx, float qy, float dx, float dy, float rad, float len, std::initializer_list<Piece> ps) {
    if (c.n >= MAXL || v.a < 0.01f) return;
    Lab& l = c.l[c.n++];
    l = Lab{};
    float m = std::sqrt(dx * dx + dy * dy);
    if (m < 1e-4f) { dx = 0.0f; dy = -1.0f; m = 1.0f; }
    l.dx = dx / m;
    l.dy = dy / m;
    l.ax = v.cx + qx * v.u + l.dx * rad * v.u;
    l.ay = v.cy - qy * v.u + l.dy * rad * v.u;
    l.len = std::max(len * v.u, 14.0f);
    l.a = v.a;
    l.sc = c.sc;
    for (const Piece& p : ps)
        if (l.np < 6) l.p[l.np++] = p;
}

void note(Ctx& c, const View& v, float yFrac, std::initializer_list<Piece> ps) {
    if (c.n >= MAXL || v.a < 0.01f) return;
    Lab& l = c.l[c.n++];
    l = Lab{};
    l.leader = false;
    l.ax = 0.5f * c.W;
    l.ay = yFrac * c.H;
    l.a = v.a;
    l.sc = std::max(1, c.sc - 1);
    for (const Piece& p : ps)
        if (l.np < 6) l.p[l.np++] = p;
}

void foam(Ctx& c, const View& v) {
    add(c, v, -0.45f, 0.35f, -1.0f, -0.5f, 0.0f, 0.3f, {pc("SPACETIME FOAM (A GUESS)", WHITE)});
    add(c, v, 0.35f, -0.2f, 1.0f, 0.6f, 0.0f, 0.3f, {pc("BUBBLES OF SPACE ITSELF", WHITE)});
}

void quarks(Ctx& c, const View& v) {
    const float* col[3] = {RED, GREEN, BLUE};
    const char* nm[3] = {"UP QUARK", "UP QUARK", "DOWN QUARK"};
    float px[3], py[3];
    for (int i = 0; i < 3; i++) {
        float a = c.t * 0.35f + (float)i * TAU / 3.0f;
        px[i] = 0.38f * std::cos(a) + 0.03f * std::sin(c.t * 2.1f + (float)i * 3.0f);
        py[i] = 0.38f * std::sin(a) + 0.03f * std::cos(c.t * 1.7f + (float)i);
        add(c, v, px[i], py[i], px[i], -py[i], 0.075f, 0.12f, {pc(nm[i], col[i])});
    }
    float mx = 0.5f * (px[1] + px[2]), my = 0.5f * (py[1] + py[2]);
    add(c, v, mx, my, mx, -my, 0.0f, 0.42f, {pc("GLUONS (HOLD QUARKS TOGETHER)", GOLD)});
    note(c, v, 0.06f, {pc("RED / GREEN / BLUE = 'COLOUR CHARGE' - A QUANTUM LABEL, NOT REAL COLOUR", SOFT)});
}

void nucleus(Ctx& c, const View& v) {
    static int curP = -1, curN = -1;
    float ca = std::cos(c.t * 0.45f), sa = std::sin(c.t * 0.45f);
    float X[11], Y[11], Z[11];
    for (int i = 0; i < 11; i++) {
        float fi = (float)i;
        float x = NUC[i][0] + 0.02f * std::sin(c.t * 3.0f + fi);
        float y = NUC[i][1] + 0.02f * std::cos(c.t * 2.3f + fi * 1.7f);
        float z = NUC[i][2] + 0.02f * std::sin(c.t * 2.7f + fi * 2.9f);
        X[i] = x * ca + z * sa;
        Y[i] = y;
        Z[i] = -x * sa + z * ca;
    }
    int bestP = -1, bestN = -1;
    for (int i = 0; i < 11; i++) {
        bool prot = ((NUC_PROTONS >> i) & 1) == 1;
        int& b = prot ? bestP : bestN;
        if (b < 0 || Z[i] > Z[b]) b = i;
    }
    if (curP < 0 || Z[bestP] > Z[curP] + 0.08f) curP = bestP;
    if (curN < 0 || Z[bestN] > Z[curN] + 0.08f) curN = bestN;
    add(c, v, X[curP], Y[curP], X[curP] + 0.01f, -Y[curP] - 0.01f, 0.21f, 0.28f, {pc("PROTON (+ CHARGE)", RED)});
    add(c, v, X[curN], Y[curN], X[curN] + 0.01f, -Y[curN] - 0.01f, 0.21f, 0.28f, {pc("NEUTRON (NO CHARGE)", GREY)});
}

void atom(Ctx& c, const View& v) {
    auto ring = [&](float r, float deg, float side, float dy, std::initializer_list<Piece> ps) {
        float a = deg * 3.14159265f / 180.0f, ax = r * std::cos(a);
        float m = std::sqrt(1.0f + dy * dy), dx = side / m;
        float len = (side * 0.95f - ax) / dx;
        add(c, v, ax, r * std::sin(a), side, dy, 0.0f, std::max(len, 0.1f), ps);
    };
    ring(0.0f, 0.0f, -1.0f, 0.5f, {pc("NUCLEUS", RED)});
    ring(0.2f, 150.0f, -1.0f, -0.35f, {pc("1ST SHELL (2 ELECTRONS MAX)", BLUE)});
    ring(0.55f, 125.0f, -1.0f, -0.6f, {pc("ELECTRON SHELLS (ORBITALS)", WHITE)});
    ring(0.55f, 10.0f, 1.0f, 0.2f, {pc("2ND SHELL (8 MAX)", BLUE)});
    ring(0.55f, 55.0f, 1.0f, -0.55f, {pc("ELECTRON CLOUD (WHERE THE|ELECTRON PROBABLY IS)", BLUE)});
    ring(0.55f, -30.0f, 1.0f, 0.4f, {pc("EACH DOT = ONE PLACE|THE ELECTRON MIGHT|BE FOUND - IT IS A|PROBABILITY CLOUD,|NOT A TINY PLANET", GOLD)});
}

void water(Ctx& c, const View& v) {
    float ca = std::cos(c.t * 0.5f), hh = std::sin(52.0f * 3.14159265f / 180.0f) * 0.46f * ca;
    float hy = -0.1f + std::cos(52.0f * 3.14159265f / 180.0f) * 0.46f;
    add(c, v, 0.0f, -0.1f, -0.7f, 0.7f, 0.26f, 0.18f, {pc("OXYGEN", RED)});
    add(c, v, hh, hy, std::max(-1.0f, std::min(1.0f, hh * 6.0f)) * 0.8f, -0.7f, 0.15f, 0.2f, {pc("HYDROGEN", WHITE)});
    add(c, v, -hh, hy, -std::max(-1.0f, std::min(1.0f, hh * 6.0f)) * 0.8f, -0.7f, 0.15f, 0.2f, {pc("HYDROGEN", WHITE)});
}

void helixToQ(float wx, float wy, float& qx, float& qy) {
    float rx = wx / 1.35f, ry = (wy - 0.2f) / 1.35f;
    float cs = std::cos(0.5f), sn = std::sin(0.5f);
    qx = rx * cs - ry * sn;
    qy = rx * sn + ry * cs;
}

void dna(Ctx& c, const View& v) {
    int atI = -1, gcI = -1, bbI = -1;
    float atD = 1e9f, gcD = 1e9f, bbD = 1e9f;
    float qx, qy;
    for (int i = 0; i < 26; i++) {
        float s = ((float)i / 25.0f - 0.5f) * 2.7f;
        helixToQ(s, 0.0f, qx, qy);
        bool at = ((DNA_PAIRS >> i) & 1) == 1;
        float d = std::fabs(qx - (at ? -0.35f : 0.45f));
        if (at && d < atD) { atD = d; atI = i; }
        if (!at && d < gcD) { gcD = d; gcI = i; }
        float db = std::fabs(qx + 0.95f);
        if (db < bbD) { bbD = db; bbI = i; }
    }
    auto mid = [&](int i, float& x, float& y) { helixToQ(((float)i / 25.0f - 0.5f) * 2.7f, 0.0f, x, y); };
    float x, y;
    mid(atI, x, y);
    add(c, v, x, y, -0.5f, -0.85f, 0.0f, 0.22f, {pc("A", YEL), pc("-", WHITE), pc("T", MAG)});
    mid(gcI, x, y);
    add(c, v, x, y, 0.5f, 0.85f, 0.0f, 0.22f, {pc("G", GRN), pc("-", WHITE), pc("C", BLU)});
    float s = ((float)bbI / 25.0f - 0.5f) * 2.7f;
    helixToQ(s, 0.27f * std::sin(s * 4.2f + c.t * 0.9f), x, y);
    add(c, v, x, y, -0.4f, -0.9f, 0.048f, 0.3f, {pc("SUGAR-PHOSPHATE BACKBONE", GOLD)});
    note(c, v, 0.06f, {pc("KEY: ", SOFT), pc("A=YELLOW ", YEL), pc("T=MAGENTA ", MAG), pc("G=GREEN ", GRN), pc("C=BLUE", BLU)});
}

void cell(Ctx& c, const View& v) {
    add(c, v, -0.14f, 0.04f, -0.8f, -0.6f, 0.27f, 0.2f, {pc("NUCLEUS (HOLDS DNA)", BLUE)});
    float mx = 0.42f * std::cos(0.7f) + 0.1f + 0.01f * std::sin(c.t), my = 0.2f * std::sin(1.4f) - 0.12f + 0.01f * std::cos(c.t * 1.3f);
    add(c, v, mx, my, 1.0f, 0.0f, 0.06f, 0.62f, {pc("MITOCHONDRIA (POWER)", GOLD)});
    add(c, v, -0.165f, 0.709f, 0.3f, -1.0f, 0.0f, 0.1f, {pc("MEMBRANE", GRN)});
}

void human(Ctx& c, const View& v) {
    add(c, v, 0.5f, -0.45f + 0.5f * 1.15f / 1.7f, 1.0f, 0.0f, 0.012f, 0.07f, {pc("1 METRE", GOLD)});
}

void scene(int id, Ctx& c, const View& v) {
    if (id == 0) foam(c, v);
    else if (id == 1) quarks(c, v);
    else if (id == 2) nucleus(c, v);
    else if (id == 3) atom(c, v);
    else if (id == 4) water(c, v);
    else if (id == 5) dna(c, v);
    else if (id == 6) cell(c, v);
    else if (id == 7) human(c, v);
}

void place(Ctx& c) {
    float W = c.W, H = c.H, lim = 0.69f * H, top = 0.04f * H, pad = 5.0f;
    for (int i = 0; i < c.n; i++) {
        Lab& l = c.l[i];
        int chars = 0, nl = 1, run = 0;
        for (int k = 0; k < l.np; k++)
            for (const char* q = l.p[k].s; *q; q++) {
                if (*q == '|') { nl++; run = 0; continue; }
                run++;
                chars = std::max(chars, run);
            }
        if (l.np > 1)
            for (int k = 0, tot = 0; k < l.np; k++) { tot += (int)std::strlen(l.p[k].s); chars = tot; }
        l.w = (float)(chars * 6 * l.sc - l.sc);
        l.h = (float)(nl * 7 * l.sc + (nl - 1) * 3);
        if (l.leader) {
            float ex = l.ax + l.dx * l.len, ey = l.ay + l.dy * l.len;
            l.x = ex + l.dx * (l.w * 0.5f + 4.0f) - l.w * 0.5f;
            l.y = ey + l.dy * (l.h * 0.5f + 4.0f) - l.h * 0.5f;
        } else {
            l.x = l.ax - l.w * 0.5f;
            l.y = l.ay;
        }
        l.x = std::max(6.0f, std::min(W - (l.y < 0.12f * H ? 44.0f : 12.0f) - l.w, l.x));
        l.y = std::max(top, std::min(lim - l.h, l.y));
    }
    for (int it = 0; it < 8; it++)
        for (int i = 0; i < c.n; i++)
            for (int j = i + 1; j < c.n; j++) {
                Lab &a = c.l[i], &b = c.l[j];
                if (a.x > b.x + b.w + pad || b.x > a.x + a.w + pad) continue;
                float ov = std::min(a.y + a.h, b.y + b.h) - std::max(a.y, b.y) + pad;
                if (ov <= 0.0f) continue;
                float dir = (b.y + 0.5f * b.h) >= (a.y + 0.5f * a.h) ? 1.0f : -1.0f;
                a.y = std::max(top, std::min(lim - a.h, a.y - dir * ov * 0.5f));
                b.y = std::max(top, std::min(lim - b.h, b.y + dir * ov * 0.5f));
            }
}

void build(const AppContext& app, Ctx& c) {
    const TourState& t = app.tour;
    c.W = (float)app.swapchainExtent.width;
    c.H = (float)app.swapchainExtent.height;
    c.t = t.clock;
    c.sc = std::max(1, (int)std::lround(2.0f * c.H / 720.0f));
    float f = tourMicroFade(app) * t.alpha;
    if (f < 0.01f) return;
    float wa = (1.0f - t.mix) * f, wb = t.mix * f;
    if (wa > 0.01f) scene((int)(t.sceneA + 0.5f), c, View{0.5f * c.W, 0.43f * c.H, t.zoomA * 0.5f * c.H, wa});
    if (wb > 0.01f) scene((int)(t.sceneB + 0.5f), c, View{0.5f * c.W, 0.43f * c.H, t.zoomB * 0.5f * c.H, wb});
    place(c);
}

}

void emitMicroLines(AppContext& app, OverlayVertex* out, uint32_t& used) {
    Ctx c;
    build(app, c);
    float px = 2.0f / c.W, py = 2.0f / c.H;
    auto seg = [&](float xa, float ya, float xb, float yb, const float* col, float a) {
        out[used++] = {-1.0f + xa * px, -1.0f + ya * py, col[0] * a, col[1] * a, col[2] * a};
        out[used++] = {-1.0f + xb * px, -1.0f + yb * py, col[0] * a, col[1] * a, col[2] * a};
    };
    for (int i = 0; i < c.n; i++) {
        const Lab& l = c.l[i];
        if (!l.leader || used + 20 > MAX_VERTS) continue;
        float bx = std::max(l.x - 3.0f, std::min(l.x + l.w + 3.0f, l.ax));
        float by = std::max(l.y - 3.0f, std::min(l.y + l.h + 3.0f, l.ay));
        float a = 0.7f * l.a;
        const float* col = l.p[0].c;
        if (std::hypot(bx - l.ax, by - l.ay) > 6.0f) seg(l.ax, l.ay, bx, by, col, a);
        seg(l.ax - 3.0f, l.ay, l.ax, l.ay - 3.0f, col, a);
        seg(l.ax, l.ay - 3.0f, l.ax + 3.0f, l.ay, col, a);
        seg(l.ax + 3.0f, l.ay, l.ax, l.ay + 3.0f, col, a);
        seg(l.ax, l.ay + 3.0f, l.ax - 3.0f, l.ay, col, a);
    }
}

void emitMicroText(AppContext& app, OverlayVertex* out, uint32_t& used) {
    Ctx c;
    build(app, c);
    float px = 2.0f / c.W, py = 2.0f / c.H;
    for (int i = 0; i < c.n; i++) {
        const Lab& l = c.l[i];
        float x = l.x;
        if (l.np == 1 && std::strchr(l.p[0].s, '|')) {
            char buf[96];
            float y = l.y;
            const char* q = l.p[0].s;
            while (*q) {
                int n = 0;
                while (q[n] && q[n] != '|' && n < 95) n++;
                std::memcpy(buf, q, (size_t)n);
                buf[n] = 0;
                emitText(out, used, buf, -1.0f + x * px, -1.0f + y * py, px, py, (float)l.sc, l.p[0].c[0] * l.a, l.p[0].c[1] * l.a, l.p[0].c[2] * l.a);
                y += (float)(7 * l.sc + 3);
                q += n + (q[n] == '|' ? 1 : 0);
            }
            continue;
        }
        for (int k = 0; k < l.np; k++) {
            emitText(out, used, l.p[k].s, -1.0f + x * px, -1.0f + l.y * py, px, py, (float)l.sc, l.p[k].c[0] * l.a, l.p[k].c[1] * l.a, l.p[k].c[2] * l.a);
            x += (float)(std::strlen(l.p[k].s) * 6 * l.sc);
        }
    }
}
