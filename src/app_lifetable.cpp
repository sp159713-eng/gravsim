#include "vk_app.h"
#include "lifecycle.h"
#include <cctype>
#include <cstring>

void emitLifeTable(AppContext& app, OverlayVertex* out, uint32_t& used, float epoch) {
    float W = (float)app.swapchainExtent.width, H = (float)app.swapchainExtent.height;
    float px = 2.0f / W, py = 2.0f / H;
    float s = lifeUiScale(H);
    float cw = 14.0f * s, ch = 10.0f * s;
    float x0 = W - 56.0f * s - 18.0f * cw, y0 = 30.0f * s;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 18; c++) {
            int z = elementCell(r, c);
            if (z == 0) continue;
            char sym[4] = {};
            for (int i = 0; i < 2 && elementSymbol(z)[i]; i++) sym[i] = (char)std::toupper((unsigned char)elementSymbol(z)[i]);
            float tl = ((float)std::strlen(sym) * 6.0f - 1.0f) * s;
            float x = x0 + (float)c * cw + 0.5f * (cw - tl);
            float y = y0 + (float)r * ch + (r == 7 ? 5.0f * s : 0.0f) + 0.5f * (ch - 7.0f * s);
            bool lit = epoch >= elementBirth(z) - 0.3f;
            float v = lit ? 1.0f : 0.38f;
            emitText(out, used, sym, -1.0f + x * px, -1.0f + y * py, px, py, s, v, v, lit ? v : 0.5f);
        }
    }
    const char* note = lifeEpochNote(epoch);
    float nl = ((float)std::strlen(note) * 6.0f) * s;
    emitText(out, used, note, -1.0f + (W - 56.0f * s - nl) * px, -1.0f + 14.0f * s * py, px, py, s, 1.0f, 0.85f, 0.45f);
}
