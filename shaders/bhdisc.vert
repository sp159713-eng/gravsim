#version 450

struct P { vec4 p; vec4 v; };
layout(std430, set = 0, binding = 0) readonly buffer Parts { P parts[]; };

layout(push_constant) uniform PC {
    mat4 viewProj;
    vec4 eye;
    vec4 prm;
    vec4 prm2;
} pc;

layout(location = 0) out vec3 outCol;
layout(location = 1) out float outSharp;

const float RE0 = 5.4;

float hash1(uint v) {
    v = (v ^ 61u) ^ (v >> 16);
    v *= 9u;
    v = v ^ (v >> 4);
    v *= 0x27d4eb2du;
    v = v ^ (v >> 15);
    return float(v) * (1.0 / 4294967296.0);
}

vec3 blackbody(float T) {
    vec3 c = vec3(0.7, 0.1, 0.03);
    c = mix(c, vec3(1.0, 0.4, 0.08), smoothstep(2500.0, 4000.0, T));
    c = mix(c, vec3(1.0, 0.7, 0.3), smoothstep(4000.0, 6000.0, T));
    c = mix(c, vec3(1.0, 0.92, 0.8), smoothstep(6000.0, 9000.0, T));
    c = mix(c, vec3(0.55, 0.72, 1.0), smoothstep(9000.0, 15000.0, T));
    return c;
}

float tempOf(float r, float heat, float floorT) {
    float tr = 18000.0 * pow(max(r, 6.0) / 6.0, -0.75);
    tr = max(tr, floorT);
    float ts = 9000.0 * sqrt(heat);
    return pow(pow(tr, 4.0) + pow(ts, 4.0), 0.25);
}

void kill() {
    gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
    gl_PointSize = 1.0;
    outCol = vec3(0.0);
    outSharp = 0.0;
}

void main() {
    uint vi = uint(gl_VertexIndex);
    uint N = uint(pc.prm2.x);
    float pxu = pc.prm.x;
    float pass = pc.prm.y;
    float time = pc.prm.z;
    vec3 E = pc.eye.xyz;
    vec3 nh = normalize(E);
    vec3 x;
    vec3 col;
    float inten;
    float size = 0.8;
    outSharp = 1.0;

    if (pass < 0.5) {
        bool second = vi >= N;
        uint i = second ? vi - N : vi;
        P q = parts[i];
        if (q.v.w < 0.5) { kill(); return; }
        float dens = pow(1.0 + (q.v.w - 1.0) * 1024.0 / 25.0, -1.3);
        x = q.p.xyz;
        float z = dot(x, nh);
        if (z >= 0.0) { kill(); return; }
        vec3 xp = x - z * nh;
        float b = max(length(xp), 0.001);
        vec3 bh = xp / b;
        float dB = -z;
        float RE = RE0 * sqrt(dB / (dB + 14.0));
        float s = sqrt(b * b + 4.0 * RE * RE);
        float th = second ? 0.5 * (b - s) : 0.5 * (b + s);
        float mu = abs(th / b * 0.5 * (1.0 + (second ? -1.0 : 1.0) * b / s));
        mu = min(mu, 5.0);
        if (second && mu < 0.03) { kill(); return; }
        vec3 xn = z * nh + bh * th;
        vec3 v = q.v.xyz;
        float r = length(q.p.xyz);
        float T = tempOf(r, q.p.w, pc.eye.w);
        vec3 no = normalize(E - q.p.xyz);
        float beta = clamp(dot(v, no), -0.7, 0.7);
        float D = 1.0 / (1.0 - beta);
        T *= D;
        float inner = 1.0 + 3.0 * (1.0 - smoothstep(3.5, 9.0, r));
        inten = pc.prm2.w * pow(T / 6000.0, 1.6) * inner * D * mu * dens;
        col = blackbody(T);
        x = xn;
        if (second) inten *= 0.8;
    } else {
        uint JET = uint(pc.prm2.z);
        if (vi < N) {
            P q = parts[vi];
            if (q.v.w < 0.5) { kill(); return; }
            float dens = pow(1.0 + (q.v.w - 1.0) * 1024.0 / 25.0, -1.3);
            x = q.p.xyz;
            if (dot(x, nh) < 0.0) { kill(); return; }
            vec3 v = q.v.xyz;
            float r = length(x);
            float T = tempOf(r, q.p.w, pc.eye.w);
            vec3 no = normalize(E - x);
            float beta = clamp(dot(v, no), -0.7, 0.7);
            float D = 1.0 / (1.0 - beta);
            T *= D;
            float inner = 1.0 + 3.0 * (1.0 - smoothstep(3.5, 9.0, r));
            inten = pc.prm2.w * pow(T / 6000.0, 1.6) * inner * D * dens;
            col = blackbody(T);
        } else if (vi < N + JET) {
            uint k = vi - N;
            float pw = pc.prm2.y;
            if (pw <= 0.001) { kill(); return; }
            float sgn = (k & 1u) == 0u ? 1.0 : -1.0;
            float ph = fract(hash1(k * 3u + 1u) + time * 0.12);
            float hgt = 5.0 + ph * 150.0;
            float rad = 0.8 + 0.1 * hgt;
            float a = hash1(k * 7u + 3u) * 6.2831853;
            float rr = rad * sqrt(hash1(k * 11u + 5u));
            x = vec3(rr * cos(a), sgn * hgt, rr * sin(a));
            inten = pw * 0.2 * (1.0 - ph) * (0.4 + 0.6 * hash1(k * 13u));
            col = mix(vec3(0.5, 0.65, 1.0), vec3(0.9, 0.95, 1.0), 1.0 - ph);
            size = 0.45;
            outSharp = 0.0;
        } else {
            if (pc.prm.w < 0.5) { kill(); return; }
            x = vec3(70.0, 0.0, 0.0);
            inten = 14.0;
            col = vec3(1.0, 0.78, 0.45);
            size = 4.0;
            outSharp = 0.0;
        }
    }
    gl_Position = pc.viewProj * vec4(x, 1.0);
    float w = max(gl_Position.w, 0.05);
    gl_PointSize = clamp(pxu * size / w, 1.3, 40.0);
    outCol = col * inten;
}
