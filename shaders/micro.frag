#version 450

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    vec4 a;
    vec4 b;
} pc;

const float TAU = 6.2831853;

float hash11(float n) { return fract(sin(n * 127.1) * 43758.5453); }
float hash21(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
vec2 hash22(vec2 p) { return fract(sin(vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)))) * 43758.5453); }
vec3 hash31(float n) { return fract(sin(vec3(n * 12.9898, n * 78.233, n * 37.719)) * 43758.5453); }

float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i), hash21(i + vec2(1, 0)), f.x), mix(hash21(i + vec2(0, 1)), hash21(i + vec2(1, 1)), f.x), f.y);
}

float fbm(vec2 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { s += a * vnoise(p); p = p * 2.03 + 7.1; a *= 0.5; }
    return s;
}

float segDist(vec2 p, vec2 a, vec2 b, out float h) {
    vec2 pa = p - a, ba = b - a;
    h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h);
}

float sdCapsule(vec2 p, vec2 a, vec2 b, float r) {
    float h;
    return segDist(p, a, b, h) - r;
}

float smin(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

void sph(vec2 p, vec3 c, float R, vec3 base, inout float best, inout vec3 col) {
    vec2 d = p - c.xy;
    float r = length(d);
    if (r >= R) return;
    float z = sqrt(R * R - r * r);
    float depth = c.z + z;
    if (depth <= best) return;
    best = depth;
    vec3 n = vec3(d, z) / R;
    float lam = max(dot(n, normalize(vec3(-0.45, 0.55, 0.7))), 0.0);
    float rim = pow(1.0 - n.z, 3.0);
    float spec = pow(max(dot(reflect(-normalize(vec3(-0.45, 0.55, 0.7)), n), vec3(0, 0, 1)), 0.0), 24.0);
    col = base * (0.22 + 0.85 * lam) + vec3(0.9) * spec * 0.5 + base * rim * 0.35;
}

vec3 foam(vec2 p, float t) {
    vec3 acc = vec3(0.0);
    for (int o = 0; o < 2; o++) {
        float sc = o == 0 ? 3.0 : 7.0;
        vec2 q = p * sc + float(o) * 11.0;
        vec2 ip = floor(q), fp = fract(q);
        float f1 = 8.0, f2 = 8.0;
        for (int j = -1; j <= 1; j++)
            for (int i = -1; i <= 1; i++) {
                vec2 g = vec2(i, j);
                vec2 h = hash22(ip + g);
                vec2 off = 0.5 + 0.45 * sin(t * (0.5 + h * 1.3) + TAU * h);
                vec2 r = g + off - fp;
                float d = dot(r, r);
                if (d < f1) { f2 = f1; f1 = d; } else if (d < f2) f2 = d;
            }
        float edge = sqrt(f2) - sqrt(f1);
        float film = exp(-edge * (o == 0 ? 8.0 : 12.0));
        float shim = 0.5 + 0.5 * sin(edge * 26.0 - t * 2.2 + sqrt(f1) * 9.0 + float(o) * 2.0);
        vec3 c = mix(vec3(0.12, 0.38, 0.95), vec3(0.95, 0.35, 0.85), shim) * film * (o == 0 ? 1.1 : 0.55);
        c += vec3(0.02, 0.05, 0.14) * (1.0 - sqrt(f1)) * (o == 0 ? 1.0 : 0.4);
        acc += c;
    }
    return acc * (0.55 + 0.9 * fbm(p * 2.0 + t * 0.12)) * smoothstep(1.9, 0.3, length(p));
}

vec3 quarks(vec2 p, float t) {
    vec3 cols[3] = vec3[3](vec3(1.0, 0.15, 0.12), vec3(0.15, 1.0, 0.25), vec3(0.2, 0.35, 1.0));
    vec2 pos[3];
    for (int i = 0; i < 3; i++) {
        float a = t * 0.35 + float(i) * TAU / 3.0;
        pos[i] = 0.38 * vec2(cos(a), sin(a)) + 0.03 * vec2(sin(t * 2.1 + float(i) * 3.0), cos(t * 1.7 + float(i)));
    }
    vec3 c = vec3(0.05, 0.05, 0.08) * smoothstep(0.85, 0.2, length(p)) * (0.7 + 0.5 * fbm(p * 4.0 + t * 0.4));
    for (int i = 0; i < 3; i++) {
        int j = (i + 1) % 3;
        float h;
        vec2 ba = pos[j] - pos[i];
        float d0 = segDist(p, pos[i], pos[j], h);
        vec2 nrm = normalize(vec2(-ba.y, ba.x));
        float side = dot(p - pos[i], nrm);
        float wob = 0.03 * sin(h * 26.0 - t * 14.0 + float(i) * 2.0) * sin(h * 3.14159) + 0.018 * sin(h * 61.0 + t * 23.0);
        float line = 0.0035 / (abs(side - wob) + 0.004) * step(0.0, h) * smoothstep(0.0, 0.05, h) * smoothstep(1.0, 0.95, h);
        line *= 0.7 + 0.5 * sin(t * 30.0 + h * 9.0 + float(i));
        c += mix(cols[i], cols[j], h) * line * 0.9 * exp(-d0 * 6.0);
    }
    for (int i = 0; i < 3; i++) {
        float d = length(p - pos[i]);
        c += cols[i] * (0.012 / (d * d + 0.0025)) * 0.45;
        c += cols[i] * smoothstep(0.075, 0.03, d) * 1.2;
        c += vec3(1.0) * smoothstep(0.03, 0.0, d) * 0.6;
    }
    return c;
}

const vec3 NUC[11] = vec3[11](
    vec3(0.0532, -0.2107, -0.1995),
    vec3(0.1057, 0.0613, 0.1141),
    vec3(0.3899, -0.1293, -0.2279),
    vec3(-0.2429, 0.2098, 0.1157),
    vec3(0.0459, -0.3682, -0.0030),
    vec3(0.4171, -0.2226, -0.0174),
    vec3(-0.1485, -0.1902, -0.4646),
    vec3(0.2339, 0.0175, -0.3001),
    vec3(0.2654, 0.2760, -0.1116),
    vec3(-0.4641, -0.0356, 0.0764),
    vec3(0.3975, 0.0830, -0.2378));
const int NUC_PROTONS = 860;
const int DNA_PAIRS = 14106858;

vec3 nucleus(vec2 p, float t) {
    float best = -9.0;
    vec3 col = vec3(0.0);
    float ca = cos(t * 0.45), sa = sin(t * 0.45);
    for (int i = 0; i < 11; i++) {
        vec3 c = NUC[i];
        c += 0.02 * vec3(sin(t * 3.0 + float(i)), cos(t * 2.3 + float(i) * 1.7), sin(t * 2.7 + float(i) * 2.9));
        c = vec3(c.x * ca + c.z * sa, c.y, -c.x * sa + c.z * ca);
        bool prot = ((NUC_PROTONS >> i) & 1) == 1;
        sph(p, c, 0.21, prot ? vec3(0.95, 0.2, 0.18) : vec3(0.62, 0.64, 0.7), best, col);
    }
    float halo = exp(-length(p) * 2.6) * 0.18;
    return col + vec3(0.5, 0.25, 0.3) * halo;
}

vec3 atom(vec2 p, float t) {
    float r = length(p);
    float ang = atan(p.y, p.x);
    float shell1 = exp(-pow((r - 0.2) / 0.1, 2.0));
    float shell2 = exp(-pow((r - 0.55) / 0.2, 2.0)) * (0.6 + 0.4 * cos(2.0 * ang + t * 0.5));
    float cloud = (shell1 * 0.8 + shell2 * 0.9 + exp(-r * r / 0.5) * 0.25) * (0.55 + 0.9 * fbm(p * 5.0 + vec2(t * 0.3, -t * 0.2)));
    vec3 c = vec3(0.1, 0.35, 0.9) * cloud * 0.95;
    vec2 q = p * 34.0;
    vec2 ip = floor(q);
    vec2 h = hash22(ip);
    vec2 pt = ip + 0.5 + 0.35 * sin(t * 1.8 + TAU * h);
    float d = length(q - pt);
    c += vec3(0.5, 0.8, 1.0) * smoothstep(0.35, 0.0, d) * step(hash21(ip + 4.0), cloud * 1.2) * 1.1;
    float nd = length(p);
    c += vec3(1.0, 0.45, 0.35) * smoothstep(0.034, 0.012, nd) * 1.6;
    c += vec3(1.0, 0.4, 0.3) * 0.0012 / (nd * nd + 0.0006);
    return c;
}

vec3 water(vec2 p, float t) {
    float best = -9.0;
    vec3 col = vec3(0.0);
    float ca = cos(t * 0.5), sa = sin(t * 0.5);
    vec3 o = vec3(0.0, -0.1, 0.0);
    float half_ = radians(52.0);
    vec3 h1 = vec3(sin(half_) * 0.46, -0.1 + cos(half_) * 0.46, 0.0);
    vec3 h2 = vec3(-h1.x, h1.y, 0.0);
    vec3 pts[3] = vec3[3](o, h1, h2);
    for (int i = 0; i < 3; i++) pts[i] = vec3(pts[i].x * ca + pts[i].z * sa, pts[i].y, -pts[i].x * sa + pts[i].z * ca);
    for (int i = 1; i < 3; i++) {
        float h;
        float d = segDist(p, pts[0].xy, pts[i].xy, h);
        if (d < 0.05) {
            float dep = mix(pts[0].z, pts[i].z, h);
            if (dep > best) { best = dep; col = vec3(0.55, 0.58, 0.65) * (0.5 + 0.7 * (1.0 - d / 0.05)); }
        }
    }
    sph(p, pts[0], 0.26, vec3(0.95, 0.15, 0.12), best, col);
    sph(p, pts[1], 0.15, vec3(0.92, 0.94, 1.0), best, col);
    sph(p, pts[2], 0.15, vec3(0.92, 0.94, 1.0), best, col);
    return col + vec3(0.1, 0.2, 0.5) * exp(-length(p) * 2.2) * 0.25;
}

vec3 dna(vec2 p, float t) {
    float best = -9.0;
    vec3 col = vec3(0.0);
    float ang = 0.5;
    p = vec2(p.x * cos(ang) + p.y * sin(ang), -p.x * sin(ang) + p.y * cos(ang));
    p = p * 1.35 + vec2(0.0, 0.2);
    for (int i = 0; i < 26; i++) {
        float s = (float(i) / 25.0 - 0.5) * 2.7;
        float ph = s * 4.2 + t * 0.9;
        float R = 0.27;
        vec3 a = vec3(s, R * sin(ph), R * cos(ph));
        vec3 b = vec3(s, -R * sin(ph), -R * cos(ph));
        float h;
        float d = segDist(p, a.xy, b.xy, h);
        if (d < 0.012) {
            float dep = mix(a.z, b.z, h);
            if (dep > best) {
                best = dep;
                bool at = ((DNA_PAIRS >> i) & 1) == 1;
                vec3 bc = at ? vec3(0.95, 0.85, 0.25) : vec3(0.3, 0.9, 0.55);
                vec3 bd = at ? vec3(0.9, 0.35, 0.7) : vec3(0.35, 0.6, 1.0);
                col = h < 0.5 ? bc : bd;
                col *= 0.55 + 0.45 * (dep / R * 0.5 + 0.5);
            }
        }
        sph(p, a, 0.065, vec3(1.0, 0.62, 0.2), best, col);
        sph(p, b, 0.065, vec3(0.25, 0.8, 1.0), best, col);
    }
    return col + vec3(0.1, 0.15, 0.3) * exp(-abs(p.y) * 3.0) * 0.12;
}

vec3 cell(vec2 p, float t) {
    float wob = 1.0 + 0.012 * sin(atan(p.y, p.x) * 5.0 + t * 1.3);
    vec2 e = p / vec2(0.95, 0.72);
    float rr = length(e) / wob;
    vec3 c = vec3(0.04, 0.16, 0.14) * smoothstep(1.0, 0.0, rr) * (0.7 + 0.6 * fbm(p * 6.0 + t * 0.1));
    c *= step(rr, 1.0);
    c += vec3(0.4, 0.95, 0.8) * exp(-pow((rr - 1.0) / 0.012, 2.0)) * 1.1;
    c += vec3(0.25, 0.7, 0.6) * exp(-pow((rr - 0.975) / 0.01, 2.0)) * 0.5;
    vec2 nc = vec2(-0.14, 0.04);
    float nd = length(p - nc);
    c = mix(c, vec3(0.07, 0.12, 0.32) * (0.8 + 0.4 * fbm(p * 9.0 + 3.0)), smoothstep(0.27, 0.26, nd));
    c += vec3(0.45, 0.6, 1.0) * exp(-pow((nd - 0.27) / 0.01, 2.0)) * 0.9;
    c += vec3(0.7, 0.45, 1.0) * smoothstep(0.07, 0.04, length(p - nc - vec2(0.05, -0.04))) * 0.9;
    for (int i = 0; i < 4; i++) {
        float fi = float(i);
        vec2 mc = vec2(0.42, 0.2) * vec2(cos(fi * 2.2 + 0.7), sin(fi * 1.9 + 1.4)) + vec2(0.1, -0.12) + 0.01 * vec2(sin(t + fi), cos(t * 1.3 + fi));
        float ang = fi * 1.1;
        vec2 dir = vec2(cos(ang), sin(ang)) * 0.06;
        float d = sdCapsule(p, mc - dir, mc + dir, 0.045);
        c = mix(c, vec3(0.95, 0.5, 0.15) * (0.7 + 0.3 * sin((dot(p - mc, normalize(dir)) * 90.0))), smoothstep(0.004, -0.004, d));
        c += vec3(1.0, 0.75, 0.4) * exp(-pow(d / 0.006, 2.0)) * 0.5;
    }
    for (int i = 0; i < 40; i++) {
        float fi = float(i);
        vec2 q = (hash22(vec2(fi, 3.7)) * 2.0 - 1.0) * vec2(0.8, 0.6);
        q += 0.01 * vec2(sin(t * 0.8 + fi), cos(t * 0.9 + fi * 1.3));
        float inside = step(length(q / vec2(0.9, 0.68)), 1.0) * step(0.3, length(q - nc));
        c += vec3(0.9, 0.9, 0.5) * smoothstep(0.011, 0.004, length(p - q)) * 0.7 * inside;
    }
    float er = abs(fract(length(p - nc) * 14.0 + 0.3 * sin(atan(p.y, p.x - nc.x) * 5.0)) - 0.5);
    c += vec3(0.3, 0.5, 0.9) * smoothstep(0.08, 0.0, er) * smoothstep(0.55, 0.3, length(p - nc)) * step(0.3, length(p - nc)) * 0.18 * step(rr, 0.95);
    return c;
}

vec3 human(vec2 p, float t) {
    float u = 1.15 / 1.7;
    vec2 q = (p - vec2(-0.32, -0.45)) / u;
    float d = length(q - vec2(0.0, 1.6)) - 0.105;
    d = smin(d, sdCapsule(q, vec2(0.0, 1.5), vec2(0.0, 1.4), 0.045), 0.03);
    d = smin(d, sdCapsule(q, vec2(0.0, 1.38), vec2(0.0, 1.0), 0.15), 0.04);
    d = smin(d, sdCapsule(q, vec2(0.17, 1.36), vec2(0.27, 0.92), 0.045), 0.03);
    d = smin(d, sdCapsule(q, vec2(-0.17, 1.36), vec2(-0.27, 0.92), 0.045), 0.03);
    d = smin(d, sdCapsule(q, vec2(0.085, 0.95), vec2(0.1, 0.07), 0.07), 0.03);
    d = smin(d, sdCapsule(q, vec2(-0.085, 0.95), vec2(-0.1, 0.07), 0.07), 0.03);
    d = min(d, sdCapsule(q, vec2(0.1, 0.03), vec2(0.2, 0.03), 0.035));
    d = min(d, sdCapsule(q, vec2(-0.1, 0.03), vec2(-0.2, 0.03), 0.035));
    float px = 0.0035 / u;
    vec3 c = vec3(0.0);
    c += vec3(0.88, 0.9, 0.96) * smoothstep(px, -px, d) * (0.55 + 0.45 * clamp(q.y / 1.7 + 0.25 + 0.2 * (q.x + 0.3), 0.0, 1.0));
    c += vec3(0.4, 0.6, 1.0) * exp(-max(d, 0.0) * 30.0) * 0.12;
    float gy = p.y + 0.45;
    c += vec3(0.5, 0.6, 0.8) * smoothstep(0.004, 0.0, abs(gy)) * smoothstep(1.4, 0.2, abs(p.x)) * 0.8;
    float bx = 0.5;
    float top = u;
    float bar = sdCapsule(p - vec2(bx, -0.45), vec2(0.0, 0.0), vec2(0.0, top), 0.006);
    c += vec3(1.0, 0.85, 0.4) * smoothstep(0.003, -0.003, bar);
    for (int i = 0; i <= 10; i++) {
        float ty = -0.45 + top * float(i) / 10.0;
        float len = (i == 0 || i == 10) ? 0.06 : (i == 5 ? 0.045 : 0.025);
        float tk = sdCapsule(p, vec2(bx - len, ty), vec2(bx + len, ty), 0.0035);
        c += vec3(1.0, 0.85, 0.4) * smoothstep(0.002, -0.002, tk);
    }
    return c;
}

vec3 scene(int id, vec2 p, float t) {
    if (id == 0) return foam(p, t);
    if (id == 1) return quarks(p, t);
    if (id == 2) return nucleus(p, t);
    if (id == 3) return atom(p, t);
    if (id == 4) return water(p, t);
    if (id == 5) return dna(p, t);
    if (id == 6) return cell(p, t);
    return human(p, t);
}

void main() {
    float t = pc.a.x, aspect = pc.a.y;
    vec2 p = vec2(inNdc.x * aspect, -inNdc.y - 0.14);
    float mixAB = pc.b.y;
    vec3 col = vec3(0.012, 0.014, 0.03) + vec3(0.01, 0.012, 0.03) * (1.0 - length(inNdc) * 0.5);
    float wa = 1.0 - mixAB, wb = mixAB;
    if (wa > 0.003) col += scene(int(pc.a.z + 0.5), p / pc.b.z, t) * wa;
    if (wb > 0.003) col += scene(int(pc.a.w + 0.5), p / pc.b.w, t) * wb;
    outColor = vec4(col, pc.b.x);
}
