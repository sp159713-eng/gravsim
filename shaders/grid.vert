#version 450

struct Particle {
    vec4 pos;
    vec4 vel;
};

layout(std430, set = 0, binding = 1) readonly buffer Stars {
    Particle stars[];
};

layout(push_constant) uniform PushConsts {
    mat4 viewProj;
    uint n;
    uint stride;
    uint solar;
    float extent;
    float cx;
    float cz;
    float scale;
    float gx;
    float gz;
    float gph;
    float gk;
    float gamp;
    float gfront;
    float gs;
    float gq;
} pc;

layout(location = 0) out vec3 outColor;

const float WAVE_C = 14.0;
const float G = 1.0;
const float SOFT = 2.0;

float gwHeight(vec2 xz) {
    if (pc.gamp <= 0.0) return 0.0;
    vec2 q = xz - vec2(pc.gx, pc.gz);
    float r = length(q);
    float th = atan(q.y, q.x);
    float ph = 2.0 * th - pc.gph + pc.gk * r - pc.gq * r * r;
    float inner = pc.gs >= 0.0 ? WAVE_C * pc.gs : 0.0;
    float env = exp(-r / (WAVE_C * 3.0)) * 8.0 / (8.0 + r);
    float m = smoothstep(pc.gfront, pc.gfront - 4.0, r) * smoothstep(inner, inner + 3.0, r) * smoothstep(1.0, 5.0, r);
    float w = pc.gamp * env * m * cos(ph);
    if (pc.gs >= 0.0) {
        float e = (r - inner) / 2.5;
        float burst = 2.2 * exp(-e * e) * (0.7 + 0.3 * cos(ph)) * 10.0 / (10.0 + r) * exp(-pc.gs / 5.0);
        float ring = 1.4 * exp(-pc.gs / 2.5) * cos(1.2 * pc.gk * WAVE_C * (pc.gs - r / WAVE_C)) * 8.0 / (8.0 + r) * smoothstep(inner, inner - 2.0, r) * smoothstep(0.0, 3.0, r);
        w += pc.gamp * (burst + ring);
    }
    return w;
}

const int LINES = 500;
const int SEGS = 499;
const float BASE_Y = -3.0;
const float DEPTH_SCALE = 0.02;
const float MAX_DEPTH = 30.0;

void main() {
    int v = gl_VertexIndex;
    int perDir = LINES * SEGS * 2;
    int dir = v / perDir;
    int r = v % perDir;
    int line = r / (SEGS * 2);
    int k = r % (SEGS * 2);
    int along = k / 2 + (k % 2);

    float a = -pc.extent + 2.0 * pc.extent * float(line) / float(LINES - 1);
    float b = -pc.extent + 2.0 * pc.extent * float(along) / float(SEGS);
    vec2 xz = vec2(pc.cx, pc.cz) + ((dir == 0) ? vec2(a, b) : vec2(b, a));
    float ws = pc.solar != 0u ? pc.scale : 1.0;

    float phi = 0.0;
    uint exact = min(pc.n, 16u);
    for (uint j = 0u; j < exact; j++) {
        vec4 p = stars[j].pos;
        vec2 d = (p.xz - xz) / ws;
        float m = p.w;
        float s = SOFT;
        if (pc.solar != 0u && m < 100.0) {
            m = 30.0 + 60.0 * pow(m / 0.95, 1.0 / 3.0);
            s = 0.8;
        }
        phi -= G * m * inversesqrt(dot(d, d) + s * s);
    }
    for (uint j = exact; j < pc.n; j += pc.stride) {
        vec4 p = stars[j].pos;
        vec2 d = (p.xz - xz) / ws;
        phi -= G * p.w * float(pc.stride) * inversesqrt(dot(d, d) + SOFT * SOFT);
    }

    float raw = max(-phi * DEPTH_SCALE, 0.0);
    float depth = raw / pow(1.0 + pow(raw / MAX_DEPTH, 4.0), 0.25);
    float wave = gwHeight(xz);
    gl_Position = pc.viewProj * vec4(xz.x, (BASE_Y - depth) * ws + wave, xz.y, 1.0);
    float t = depth / MAX_DEPTH;
    float far = gl_Position.w / ws;
    float fade = min(1.0, 35.0 / max(far, 1.0)) * (1.0 - smoothstep(180.0, 450.0, far));
    vec3 tint = vec3(0.1, 0.5, 0.9) * smoothstep(0.1, 0.9, wave);
    outColor = (mix(vec3(0.05, 0.12, 0.25), vec3(0.35, 0.2, 0.6), t) * 0.3 + tint * 0.5) * fade;
}
