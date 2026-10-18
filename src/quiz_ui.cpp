#include "quiz.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

struct Pen {
    OverlayVertex* out;
    uint32_t& used;
    float W, H;

    void rect(const QuizRect& r, const float c[3]) {
        if (used + 6 > MAX_VERTS) return;
        float a = -1.0f + 2.0f * r.x0 / W, b = -1.0f + 2.0f * r.y0 / H;
        float e = -1.0f + 2.0f * r.x1 / W, f = -1.0f + 2.0f * r.y1 / H;
        out[used++] = {a, b, c[0], c[1], c[2]};
        out[used++] = {e, b, c[0], c[1], c[2]};
        out[used++] = {e, f, c[0], c[1], c[2]};
        out[used++] = {a, b, c[0], c[1], c[2]};
        out[used++] = {e, f, c[0], c[1], c[2]};
        out[used++] = {a, f, c[0], c[1], c[2]};
    }

    void panel(const QuizRect& r, float n, const float c[3]) {
        rect({r.x0 + n, r.y0, r.x1 - n, r.y1}, c);
        rect({r.x0, r.y0 + n, r.x0 + n, r.y1 - n}, c);
        rect({r.x1 - n, r.y0 + n, r.x1, r.y1 - n}, c);
    }

    void text(const std::string& s, float x, float y, int scale, const float c[3]) {
        float px = 2.0f / W, py = 2.0f / H;
        int last = std::max(0, scale - 2);
        for (int oy = 0; oy <= last; oy += 2)
            for (int ox = 0; ox <= last; ox += 2) {
                int cy = oy == last - 1 ? last : oy, cx = ox == last - 1 ? last : ox;
                emitText(out, used, s.c_str(), -1.0f + (x + (float)cx) * px, -1.0f + (y + (float)cy) * py, px, py, (float)scale, c[0], c[1], c[2]);
            }
    }

    void center(const std::string& s, float cx, float y, int scale, const float c[3]) {
        float w = (float)s.size() * 6.0f * (float)scale;
        text(s, std::floor(cx - 0.5f * w), y, scale, c);
    }
};

const float CARD[3] = {0.035f, 0.055f, 0.12f};
const float BTN[3] = {0.09f, 0.15f, 0.30f};
const float BTN_HOT[3] = {0.16f, 0.28f, 0.52f};
const float BTN_OK[3] = {0.04f, 0.50f, 0.20f};
const float BTN_BAD[3] = {0.62f, 0.10f, 0.12f};
const float BTN_DIM[3] = {0.045f, 0.065f, 0.12f};
const float WHITE[3] = {1.0f, 1.0f, 1.0f};
const float GOLD[3] = {1.0f, 0.78f, 0.35f};
const float TXT[3] = {0.86f, 0.90f, 1.0f};
const float HINT[3] = {0.62f, 0.68f, 0.80f};
const float GREEN[3] = {0.45f, 1.0f, 0.58f};
const float RED[3] = {1.0f, 0.5f, 0.45f};
const float CYAN[3] = {0.55f, 0.85f, 1.0f};

int uOf(const AppContext& app) {
    return std::max(1, (int)std::lround((float)app.swapchainExtent.height / 720.0f));
}

}

std::string quizClean(const char* s) {
    std::string o;
    for (const char* p = s; *p; p++) {
        char c = (char)std::toupper((unsigned char)*p);
        if (c == ' ' || glyph(c)) o.push_back(c);
        else o.push_back('-');
    }
    return o;
}

std::vector<std::string> quizWrap(const std::string& s, int maxChars, int maxLines) {
    std::vector<std::string> lines;
    std::string cur;
    size_t i = 0;
    while (i < s.size()) {
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
    if ((int)lines.size() > maxLines) lines.resize(maxLines);
    return lines;
}

QuizLayout quizLayout(const AppContext& app) {
    QuizLayout L;
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    int u = uOf(app);
    int sc = 2 * u;
    L.scale = sc;
    float charW = 6.0f * (float)sc;
    L.line = 9.0f * (float)sc;
    L.pad = 7.0f * (float)sc;
    float margin = 12.0f * (float)u;
    const QuizState& Q = g_quiz;
    if (Q.phase == QP_FINAL) {
        float w = std::min(W - 24.0f, 640.0f * (float)u), h = 250.0f * (float)u;
        float x0 = 0.5f * (W - w), y0 = std::max(margin, 0.5f * (H - h) - 40.0f * (float)u);
        L.card = {x0, y0, x0 + w, y0 + h};
        float bw = 22.0f * charW, bh = L.line + 8.0f * (float)u;
        L.next = {0.5f * (W - bw), L.card.y1 - L.pad - bh, 0.5f * (W + bw), L.card.y1 - L.pad};
        return L;
    }
    float cardW = std::min(W - 24.0f, 1180.0f * (float)u);
    float x0 = 0.5f * (W - cardW);
    L.maxChars = (int)((cardW - 2.0f * L.pad) / charW);
    float gap = 4.0f * (float)u;
    float btnW = (cardW - 2.0f * L.pad - gap) * 0.5f;
    float bpad = 4.0f * (float)u;
    L.btnChars = (int)((btnW - 2.0f * bpad) / charW) - 3;
    const QuizQ& q = quizBankAt(Q.order[std::min(Q.idx, QUIZ_ROUND - 1)]);
    L.qLines = quizWrap(quizClean(q.q), L.maxChars, 3);
    for (int k = 0; k < 4; k++) L.ansLines[k] = quizWrap(quizClean(q.ans[Q.perm[k]]), L.btnChars, 2);
    L.whyLines = quizWrap(quizClean(q.why), L.maxChars, 3);
    float y = L.pad;
    y += L.line + 3.0f * (float)u;
    float qY = y;
    y += (float)L.qLines.size() * L.line + 5.0f * (float)u;
    float btnY = y;
    float rowH[2];
    for (int r = 0; r < 2; r++) {
        size_t m = std::max(L.ansLines[r * 2].size(), L.ansLines[r * 2 + 1].size());
        rowH[r] = (float)m * L.line + 2.0f * bpad;
    }
    y += rowH[0] + gap + rowH[1] + 6.0f * (float)u;
    float resY = y;
    y += (float)(1 + L.whyLines.size() + 1) * L.line;
    float total = y + L.pad;
    float cy0 = H - margin - total;
    L.card = {x0, cy0, x0 + cardW, cy0 + total};
    L.qY = cy0 + qY;
    L.btnY = cy0 + btnY;
    L.resY = cy0 + resY;
    float by = L.btnY;
    for (int r = 0; r < 2; r++) {
        for (int c = 0; c < 2; c++) {
            float bx = x0 + L.pad + (float)c * (btnW + gap);
            L.btn[r * 2 + c] = {bx, by, bx + btnW, by + rowH[r]};
        }
        by += rowH[r] + gap;
    }
    float nw = 12.0f * charW + 2.0f * bpad;
    L.next = {L.card.x1 - L.pad - nw, L.resY - 2.0f * (float)u, L.card.x1 - L.pad, L.resY + L.line + 2.0f * (float)u};
    return L;
}

void quizEmitTris(AppContext& app, OverlayVertex* out, uint32_t& used) {
    if (!g_quiz.active) return;
    const QuizState& Q = g_quiz;
    Pen p{out, used, (float)app.swapchainExtent.width, (float)app.swapchainExtent.height};
    QuizLayout L = quizLayout(app);
    int u = uOf(app);
    p.panel(L.card, 4.0f * (float)u, CARD);
    float cx = (float)app.cursorFb[0], cy = (float)app.cursorFb[1];
    if (Q.phase == QP_FINAL) {
        p.panel(L.next, 3.0f * (float)u, L.next.has(cx, cy) ? BTN_HOT : BTN);
        return;
    }
    for (int k = 0; k < 4; k++) {
        const float* c = BTN;
        if (Q.phase == QP_ASK) c = L.btn[k].has(cx, cy) ? BTN_HOT : BTN;
        else if (k == Q.correctSlot) c = BTN_OK;
        else if (k == Q.picked) c = BTN_BAD;
        else c = BTN_DIM;
        p.panel(L.btn[k], 3.0f * (float)u, c);
    }
    if (Q.phase == QP_REVEAL) p.panel(L.next, 3.0f * (float)u, L.next.has(cx, cy) ? BTN_HOT : BTN);
}

void quizEmitText(AppContext& app, OverlayVertex* out, uint32_t& used) {
    if (!g_quiz.active) return;
    const QuizState& Q = g_quiz;
    Pen p{out, used, (float)app.swapchainExtent.width, (float)app.swapchainExtent.height};
    QuizLayout L = quizLayout(app);
    int u = uOf(app);
    int sc = L.scale;
    float W = p.W;
    if (Q.phase == QP_FINAL) {
        float cx = 0.5f * (L.card.x0 + L.card.x1);
        float y = L.card.y0 + L.pad;
        p.center("QUIZ COMPLETE", cx, y, sc + u, GOLD);
        y += 14.0f * (float)sc + 6.0f * (float)u;
        char b[64];
        std::snprintf(b, sizeof b, "SCORE %d / %d", Q.score, QUIZ_ROUND);
        p.center(b, cx, y, 2 * sc, WHITE);
        y += 18.0f * (float)sc + 4.0f * (float)u;
        const float* rc = Q.score >= 10 ? GOLD : Q.score >= 8 ? CYAN : Q.score >= 5 ? GREEN : TXT;
        p.center(quizRank(Q.score), cx, y, sc + u, rc);
        y += 14.0f * (float)sc + 4.0f * (float)u;
        std::snprintf(b, sizeof b, "BEST STREAK THIS ROUND %d", Q.roundStreak);
        p.center(b, cx, y, sc, TXT);
        y += L.line + 2.0f * (float)u;
        if (Q.newBest) p.center("NEW PERSONAL BEST", cx, y, sc, GREEN);
        else {
            std::snprintf(b, sizeof b, "PERSONAL BEST %d / %d   STREAK %d", Q.bestScore, QUIZ_ROUND, Q.bestStreak);
            p.center(b, cx, y, sc, HINT);
        }
        p.center("N  PLAY AGAIN", 0.5f * (L.next.x0 + L.next.x1), L.next.y0 + 4.0f * (float)u, sc, WHITE);
        p.center("ESC  CLOSE AND RETURN", cx, L.next.y1 + 6.0f * (float)u, sc, HINT);
        return;
    }
    const QuizQ& q = quizBankAt(Q.order[Q.idx]);
    char hdr[96];
    std::snprintf(hdr, sizeof hdr, "QUESTION %d/%d   SCORE %d   STREAK %d   BEST %d", Q.idx + 1, QUIZ_ROUND, Q.score, Q.streak, Q.bestScore);
    float x = L.card.x0 + L.pad;
    p.text(hdr, x, L.card.y0 + L.pad, sc, GOLD);
    std::string esc = "ESC  QUIT";
    p.text(esc, L.card.x1 - L.pad - (float)esc.size() * 6.0f * (float)sc, L.card.y0 + L.pad, sc, HINT);
    for (size_t i = 0; i < L.qLines.size(); i++) p.text(L.qLines[i], x, L.qY + (float)i * L.line, sc, WHITE);
    float bpad = 4.0f * (float)u;
    for (int k = 0; k < 4; k++) {
        const QuizRect& r = L.btn[k];
        char key[4] = {(char)('1' + k), 0, 0, 0};
        p.text(key, r.x0 + bpad, r.y0 + bpad, sc, Q.phase == QP_REVEAL && k == Q.correctSlot ? WHITE : CYAN);
        const float* tc = Q.phase == QP_REVEAL && k != Q.correctSlot && k != Q.picked ? HINT : WHITE;
        for (size_t i = 0; i < L.ansLines[k].size(); i++)
            p.text(L.ansLines[k][i], r.x0 + bpad + 3.0f * 6.0f * (float)sc, r.y0 + bpad + (float)i * L.line, sc, tc);
    }
    float y = L.resY;
    if (Q.phase == QP_ASK) {
        p.text("PRESS 1-4 OR CLICK AN ANSWER - THE SIM WILL SHOW YOU THE ANSWER", x, y, sc, HINT);
        (void)W;
        return;
    }
    bool ok = Q.picked == Q.correctSlot;
    std::string res;
    if (ok) res = "CORRECT - STREAK " + std::to_string(Q.streak);
    else res = "NOT QUITE - ANSWER " + std::to_string(Q.correctSlot + 1);
    p.text(res, x, y, sc, ok ? GREEN : RED);
    bool last = Q.idx + 1 >= QUIZ_ROUND;
    p.center(last ? "N  RESULTS" : "N  NEXT", 0.5f * (L.next.x0 + L.next.x1), L.next.y0 + 2.0f * (float)u, sc, WHITE);
    y += L.line;
    for (const std::string& w : L.whyLines) {
        p.text(w, x, y, sc, TXT);
        y += L.line;
    }
    std::string cap = std::string("SHOW ME - ") + quizClean(q.show.caption);
    p.text(cap, x, y, sc, HINT);
}
