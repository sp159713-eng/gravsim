#version 450

layout(push_constant) uniform PushConsts {
    mat4 viewProj;
    vec4 right;
    vec4 up;
    vec4 back;
    vec4 proj;
} pc;

layout(location = 0) in vec2 inQ;
layout(location = 1) flat in vec3 inN;
layout(location = 2) flat in float inSeed;

layout(location = 0) out vec4 outColor;

const float R_IN = 1.5;
const float RING = 1.1;

float rOut() {
    return pc.back.w > 1.5 ? 3.0 : 5.0;
}

float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float noise(vec3 x) {
    vec3 i = floor(x);
    vec3 f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash(i), hash(i + vec3(1, 0, 0)), f.x),
                   mix(hash(i + vec3(0, 1, 0)), hash(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(hash(i + vec3(0, 0, 1)), hash(i + vec3(1, 0, 1)), f.x),
                   mix(hash(i + vec3(0, 1, 1)), hash(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

float fbm(vec3 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) {
        v += a * noise(p);
        p *= 2.03;
        a *= 0.5;
    }
    return v;
}

vec3 heat(float r) {
    float t = clamp(R_IN / r, 0.0, 1.0);
    vec3 cool = vec3(0.9, 0.22, 0.04);
    vec3 warm = vec3(1.0, 0.62, 0.22);
    vec3 hot = vec3(1.0, 0.95, 0.85);
    vec3 c = mix(cool, warm, smoothstep(0.25, 0.65, t));
    return mix(c, hot, smoothstep(0.7, 1.0, t));
}

float emission(float r, vec3 p, float time) {
    float t = clamp(R_IN / r, 0.0, 1.0);
    float omega = 2.2 / pow(r, 1.5);
    float phi = atan(p.y, p.x) - omega * time;
    vec3 np = vec3(cos(phi), sin(phi), 0.0) * 2.6 + vec3(0.0, 0.0, r * 4.5 + inSeed);
    float turb = 0.3 + 1.35 * fbm(np);
    float bands = 0.9 + 0.1 * sin(r * 30.0 + 5.0 * fbm(np * 0.5));
    return (0.35 + 1.9 * pow(t, 2.2)) * turb * bands;
}

float beaming(vec3 ls, vec3 p, float r) {
    float vz = (ls.x * p.y - ls.y * p.x) / max(r, 0.001);
    float beta = 0.55 * sqrt(R_IN / max(r, R_IN));
    return clamp(pow(1.0 / (1.0 - beta * vz), 2.6), 0.15, 5.0);
}

void main() {
    float time = pc.right.w;
    float rho = length(inQ);
    vec3 n = inN;
    float sXY = length(n.xy);
    vec3 e1 = sXY > 1e-4 ? vec3(-n.y, n.x, 0.0) / sXY : vec3(1.0, 0.0, 0.0);
    vec3 e2 = cross(n, e1);
    vec3 ls = -n;
    float det = abs(n.z) < 0.06 ? (n.z < 0.0 ? -0.06 : 0.06) : n.z;
    float a = dot(inQ, e1.xy);
    float b = (-e1.y * inQ.x + e1.x * inQ.y) / det;
    float r = sqrt(a * a + b * b);

    float discK = pc.proj.z > 1.5 ? 0.04 : 1.0;
    vec3 col = discK * vec3(1.0, 0.5, 0.18) * 0.30 * exp(-(rho - 1.0) * 1.5) * smoothstep(6.0, 3.0, rho);
    float alpha = 0.0;

    float ci = abs(n.z);
    vec2 qh = inQ / max(rho, 0.001);
    vec2 minor = sXY > 1e-4 ? e2.xy / max(length(e2.xy), 1e-4) : vec2(0.0, 1.0);
    float along = abs(dot(qh, minor));
    float lens = mix(pow(along, 2.2), 1.0, ci * ci);
    float tl = pow(clamp((rho - 1.08) / 0.75, 0.0, 1.0), 1.5);
    float rl = R_IN + tl * (rOut() - R_IN);
    float al = clamp(dot(inQ, e1.xy), -rl, rl);
    vec3 pl = e1 * al - e2 * sqrt(max(rl * rl - al * al, 0.0));
    float arcMask = smoothstep(1.02, 1.1, rho) * smoothstep(1.9, 1.4, rho) * lens;
    float arcFall = 0.35 + 0.65 * pow(1.0 - tl, 1.5);
    vec3 arcCol = heat(rl) * emission(rl, pl, time) * beaming(ls, pl, rl) * arcMask * arcFall * 0.9;
    arcCol *= discK;
    float ring = exp(-pow((rho - RING) / 0.028, 2.0));
    float ringSide = 0.5 + 0.5 * clamp(beaming(ls, vec3(inQ, 0.0), 1.5) * 0.6, 0.0, 2.0);
    vec3 lensCol = arcCol + vec3(1.0, 0.86, 0.62) * ring * ringSide * 1.7;
    lensCol += vec3(1.0, 0.7, 0.4) * exp(-pow((rho - RING) / 0.12, 2.0)) * 0.35 * ringSide;

    bool inDisc = r > R_IN && r < rOut();
    vec3 pd = e1 * a + e2 * b;
    float dAlpha = 0.0;
    vec3 dCol = vec3(0.0);
    if (inDisc) {
        float edge = smoothstep(R_IN, R_IN * 1.06, r) * smoothstep(rOut(), rOut() * 0.72, r);
        dAlpha = 0.95 * edge * discK;
        if (b <= 0.0) dAlpha *= 1.0 - 0.95 * (1.0 - ci) * pow(along, 2.2) * (1.0 - smoothstep(1.0, 2.4, rho));
        dCol = heat(r) * emission(r, pd, time) * beaming(ls, pd, r);
    }
    bool nearSide = b > 0.0;
    if (!nearSide) {
        col = dCol * dAlpha + col * (1.0 - dAlpha);
        alpha = dAlpha + alpha * (1.0 - dAlpha);
    }
    col += lensCol;
    float sh = 1.0 - smoothstep(0.97, 1.0, rho);
    col *= 1.0 - sh;
    alpha = alpha * (1.0 - sh) + sh;
    if (nearSide) {
        col = dCol * dAlpha + col * (1.0 - dAlpha);
        alpha = dAlpha + alpha * (1.0 - dAlpha);
    }
    outColor = vec4(col, alpha);
}
