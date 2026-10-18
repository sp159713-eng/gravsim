#include "vk_app.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

struct Box {
    float x0, y0, x1, y1;
};

bool overlaps(const Box& a, const Box& b) {
    return a.x0 < b.x1 && a.x1 > b.x0 && a.y0 < b.y1 && a.y1 > b.y0;
}

}

void declutterLabels(std::vector<ScreenLabel>& labels, float width, float height) {
    std::vector<size_t> heads;
    for (size_t i = 0; i < labels.size(); i++)
        if (!labels[i].follow && labels[i].text) heads.push_back(i);
    std::stable_sort(heads.begin(), heads.end(), [&](size_t a, size_t b) { return labels[a].prio < labels[b].prio; });
    std::vector<Box> placed;
    std::vector<bool> keep(labels.size(), false);
    std::vector<float> shift(labels.size(), 0.0f);
    const float nudges[5] = {0.0f, 15.0f, -15.0f, 30.0f, -30.0f};
    for (size_t h : heads) {
        size_t end = h + 1;
        while (end < labels.size() && labels[end].follow) end++;
        float cx = (labels[h].x + 1.0f) * 0.5f * width, cy = (labels[h].y + 1.0f) * 0.5f * height;
        float top = 1e9f, bottom = -1e9f, w = 0.0f;
        for (size_t k = h; k < end; k++) {
            top = std::min(top, labels[k].oy);
            bottom = std::max(bottom, labels[k].oy + 14.0f);
            w = std::max(w, 12.0f * (float)std::strlen(labels[k].text));
        }
        for (float nudge : nudges) {
            Box b = {cx + 7.0f, cy + top + nudge - 3.0f, cx + 13.0f + w, cy + bottom + nudge + 3.0f};
            bool hit = false;
            for (const Box& o : placed)
                if (overlaps(b, o)) { hit = true; break; }
            if (hit) continue;
            placed.push_back(b);
            for (size_t k = h; k < end; k++) { keep[k] = true; shift[k] = nudge; }
            break;
        }
    }
    std::vector<ScreenLabel> out;
    for (size_t i = 0; i < labels.size(); i++) {
        if (labels[i].ring > 0.0f) { out.push_back(labels[i]); continue; }
        if (!keep[i]) continue;
        ScreenLabel l = labels[i];
        l.y += shift[i] * 2.0f / height;
        out.push_back(l);
    }
    labels.swap(out);
}

void emitLabelRings(const std::vector<ScreenLabel>& labels, OverlayVertex* out, uint32_t& used, float width, float height) {
    const int segs = 28;
    for (const ScreenLabel& l : labels) {
        if (l.ring <= 0.0f) continue;
        for (int s = 0; s < segs && used + 2 <= MAX_VERTS; s++) {
            float a0 = 6.2831853f * (float)s / segs, a1 = 6.2831853f * (float)(s + 1) / segs;
            out[used++] = {l.x + std::cos(a0) * l.ring * 2.0f / width, l.y + std::sin(a0) * l.ring * 2.0f / height, l.color[0], l.color[1], l.color[2]};
            out[used++] = {l.x + std::cos(a1) * l.ring * 2.0f / width, l.y + std::sin(a1) * l.ring * 2.0f / height, l.color[0], l.color[1], l.color[2]};
        }
    }
}

void emitLabelTexts(const std::vector<ScreenLabel>& labels, OverlayVertex* out, uint32_t& used, float width, float height) {
    float px = 2.0f / width, py = 2.0f / height;
    closeupNoteLabels(labels, width, height);
    for (const ScreenLabel& l : labels)
        if (l.text && l.ring <= 0.0f)
            emitText(out, used, l.text, l.x + 10.0f * px, l.y + l.oy * py, px, py, 2.0f, l.color[0], l.color[1], l.color[2]);
}
