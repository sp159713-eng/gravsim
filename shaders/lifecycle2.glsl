vec3 wolfRayet(vec2 p, float rr, float nebR, float u, float t) {
    float len = length(p);
    float ang = atan(p.y, p.x);
    float nr = max(nebR, 1e-4);
    float x = len / nr;
    vec2 q = p / nr;
    float n1 = fbm(vec2(ang * 3.0, x * 4.0) + t * 0.01 + 3.0);
    float n2 = fbm(q * 5.0 + 8.0);
    float fil = pow(1.0 - abs(2.0 * n2 - 1.0), 3.0);
    float edge = exp(-pow((x - 0.9) / (0.12 + 0.05 * n1), 2.0));
    vec3 tint = mix(vec3(1.0, 0.3, 0.16), vec3(0.2, 0.85, 1.0), smoothstep(0.38, 0.66, fbm(q * 2.0 + 21.0)));
    float body = smoothstep(1.0, 0.1, x) * fil * 0.9 * smoothstep(0.0, 0.25, x);
    vec3 col = tint * (edge * (0.4 + 2.2 * fil) + body) * (0.55 + n1);
    col += vec3(0.4, 0.6, 1.0) * smoothstep(1.15, 0.2, x) * 0.12 * (0.5 + n2);
    for (int k = 0; k < 4; k++) {
        float ph = fract(t * 0.04 + 0.25 * float(k));
        float rk = nr * (0.12 + 0.8 * ph) * (1.0 + 0.12 * (fbm(vec2(ang * 2.0, float(k) * 4.0)) - 0.5));
        float clump = fbm(vec2(ang * 4.0 + float(k) * 7.0, ph * 3.0) + 5.0);
        float sh = exp(-pow((len - rk) / (0.022 * nr + 2.0 * pc.e.x), 2.0)) * (0.05 + 3.2 * clump * clump * clump) * sin(3.14159265 * ph);
        col += sh * mix(vec3(0.4, 0.8, 1.0), vec3(1.0, 0.55, 0.3), float(k) * 0.3) * 0.8;
    }
    col *= smoothstep(0.0, 0.08, u);
    float d = max(len, rr * 0.5);
    col += vec3(0.75, 0.88, 1.0) * (exp(-len * len / (rr * rr * 2.5)) * 5.0 + exp(-d / (rr * 6.0)) * 0.9);
    col += spikes(p, 1.4, rr * 9.0);
    return col;
}

vec3 magnetarField(vec2 p, float Ru, float spin, float u, float t) {
    vec3 col = vec3(0.0);
    float tw = 0.3 + 0.7 * max(exp(-pow((u - 0.3) / 0.12, 2.0)), exp(-pow((u - 0.72) / 0.12, 2.0)));
    vec3 lineC = mix(vec3(0.3, 0.55, 1.0), vec3(1.0, 0.35, 0.75), tw);
    for (int a = 0; a < 3; a++) {
        vec2 q = rot(p, 0.5 + float(a) * 1.05 + spin * 0.3);
        float rho = length(q) + 1e-5;
        float c = q.y / rho;
        float an = atan(q.y, q.x);
        for (int i = 0; i < 5; i++) {
            float L = (3.0 + 2.1 * float(i)) * Ru;
            float target = L * (1.0 - c * c) * (1.0 + tw * 0.3 * sin(an * 5.0 + float(i) * 1.7 + t * 2.5 + float(a) * 2.0));
            float w = Ru * (0.06 + 0.015 * float(i));
            col += exp(-pow((rho - target) / w, 2.0)) * lineC * (0.9 / (1.0 + 0.35 * float(i))) * smoothstep(0.9 * Ru, 2.0 * Ru, rho);
        }
    }
    float len = length(p);
    for (int k = 0; k < 2; k++) {
        float age = u - (k == 0 ? 0.3 : 0.72);
        if (age > 0.0) col += exp(-pow((len - age * 28.0 * Ru) / (Ru * 0.45), 2.0)) * vec3(0.85, 0.92, 1.0) * 1.6 * exp(-age * 12.0);
    }
    return col;
}

vec3 kilonova(vec2 p, float u, float t) {
    vec3 col = vec3(0.0);
    float tin = clamp(u / 0.57, 0.0, 1.0);
    float rem = 1.0 - tin;
    float d = 0.45 * pow(rem, 0.5) + 0.012;
    float phase = 52.0 * (1.0 - pow(rem, 0.6));
    float pre = 1.0 - smoothstep(0.55, 0.575, u);
    vec2 q = vec2(p.x, p.y / 0.45);
    float rho = length(q);
    float a = atan(q.y, q.x);
    float arm = pow(0.5 + 0.5 * cos(2.0 * (a - phase) - 9.0 * rho), 4.0) * exp(-rho * 3.2) * smoothstep(0.08, 0.5, u);
    col += vec3(0.55, 0.65, 1.0) * arm * 0.7 * pre;
    col += vec3(0.2, 0.7, 0.9) * 0.05 * sin(rho * 46.0 - phase * 0.7) * exp(-rho * 2.4) * pre;
    for (int s = -1; s <= 1; s += 2) {
        vec2 c = 0.5 * d * float(s) * vec2(cos(phase), sin(phase) * 0.45);
        vec2 dp = p - c;
        float r2 = dot(dp, dp);
        float sz = 0.02;
        col += vec3(0.7, 0.85, 1.0) * (exp(-r2 / (sz * sz)) * 3.2 + exp(-r2 / (sz * sz * 30.0)) * 0.5) * pre;
    }
    float ag = max(u - 0.57, 0.0);
    if (u > 0.57) {
        float rE = 0.04 + 0.95 * (1.0 - exp(-ag * 5.0));
        float re = length(vec2(p.x, p.y * 1.8));
        float fade = exp(-ag * 1.8);
        float n = fbm(p * 4.0 / (rE + 0.1) + 5.0);
        float fil = pow(1.0 - abs(2.0 * n - 1.0), 2.5);
        float shell = exp(-pow((re - rE) / (0.2 * rE + 0.01), 2.0));
        col += vec3(1.0, 0.25, 0.08) * (shell * (0.4 + 1.8 * fil) + smoothstep(rE, 0.0, re) * fil * 0.5) * fade * 1.4;
        float jw = 0.05 + 0.22 * abs(p.y);
        col += vec3(0.3, 0.55, 1.0) * exp(-p.x * p.x / (jw * jw)) * smoothstep(rE * 1.3, rE * 0.1, abs(p.y)) * (0.4 + fil) * fade * 1.1;
        float h = hash(floor(p * 70.0));
        col += vec3(1.0, 0.85, 0.3) * step(0.985, h) * smoothstep(rE, rE * 0.3, length(p)) * (0.5 + 0.5 * sin(t * 6.0 + h * 50.0)) * 1.8 * fade;
        col += vec3(1.0, 0.9, 0.85) * exp(-dot(p, p) / 0.0016) * 4.0 * exp(-ag * 6.0);
    }
    return col;
}

float cellBirth(int z) {
    if (z <= 3) return 0.0;
    if (z <= 5) return 5.0;
    if (z <= 8) return 2.0;
    if (z <= 20) return 3.0;
    if (z <= 24) return 3.6;
    if (z <= 28) return 4.0;
    if (z == 43 || z == 61 || z >= 95) return 99.0;
    if (z <= 47) return 5.0;
    if (z <= 55) return 5.4;
    return 6.0;
}

int cellZ(int r, int c) {
    if (r == 0) return c == 0 ? 1 : (c == 17 ? 2 : 0);
    if (r == 1) return c < 2 ? 3 + c : (c >= 12 ? c - 7 : 0);
    if (r == 2) return c < 2 ? 11 + c : (c >= 12 ? c + 1 : 0);
    if (r == 3) return 19 + c;
    if (r == 4) return 37 + c;
    if (r == 5) return c < 2 ? 55 + c : (c >= 3 ? 69 + c : 0);
    if (r == 6) return c < 2 ? 87 + c : (c >= 3 ? 101 + c : 0);
    if (c >= 3 && c <= 7) {
        int f[5] = int[](60, 63, 90, 92, 94);
        return f[c - 3];
    }
    return 0;
}

vec3 elementTable(vec3 col, vec2 fp, float W) {
    float s = pc.f.z;
    float cw = 14.0 * s, ch = 10.0 * s;
    float x0 = W - 56.0 * s - 18.0 * cw, y0 = 30.0 * s;
    vec2 rel = fp - vec2(x0, y0);
    float plateB = 8.0 * ch + 9.0 * s;
    if (rel.x < -6.0 * s || rel.x > 18.0 * cw + 6.0 * s || rel.y < -20.0 * s || rel.y > plateB) return col;
    col = col * 0.3 + vec3(0.01, 0.015, 0.03);
    float e = pc.f.x;
    float ry = rel.y;
    float gapTop = 7.0 * ch;
    if (rel.x < 0.0 || rel.x >= 18.0 * cw || ry < 0.0) return col;
    int r;
    float ly;
    if (ry >= gapTop + 5.0 * s) { r = 7; ly = ry - gapTop - 5.0 * s; }
    else if (ry >= gapTop) return col;
    else { r = int(floor(ry / ch)); ly = ry - float(r) * ch; }
    int c = int(floor(rel.x / cw));
    float lx = rel.x - float(c) * cw;
    int z = cellZ(r, c);
    if (z == 0 || ly > ch || lx < s || lx > cw - s || ly < s * 0.5 || ly > ch - s * 0.5) return col;
    float b = cellBirth(z);
    float lv = smoothstep(b - 0.5, b - 0.1, e);
    vec3 g = b < 0.5 ? vec3(0.22, 0.42, 0.95) : b < 2.5 ? vec3(0.1, 0.6, 0.38) : b < 4.5 ? vec3(0.8, 0.42, 0.1) : b < 5.7 ? vec3(0.7, 0.18, 0.55) : b < 90.0 ? vec3(0.85, 0.6, 0.05) : vec3(0.15, 0.15, 0.18);
    vec3 cell = mix(vec3(0.12, 0.14, 0.2), g * 0.85, lv);
    if (b > 5.9 && b < 90.0 && e >= 6.0) cell += vec3(0.9, 0.7, 0.2) * (e - 6.0) * (0.4 + 0.3 * sin(pc.a.y * 5.0 + float(z)));
    cell += vec3(0.6) * lv * (1.0 - lv) * 1.5;
    return mix(col, cell, 0.92);
}
