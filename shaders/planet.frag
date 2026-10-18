#version 450

layout(location = 0) in vec3 inLocal;
layout(location = 1) in vec3 inWorldN;
layout(location = 2) in vec3 inWorldPos;
layout(location = 3) flat in int inBody;
layout(location = 4) flat in vec3 inTint;

layout(push_constant) uniform PushConsts {
    mat4 viewProj;
    vec4 camPos;
    float time;
    float pxPerUnit;
    float trueMode;
    float sunT;
    float auPerUnit;
} pc;

layout(location = 0) out vec4 outFragColor;

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
    for (int i = 0; i < 5; i++) {
        v += a * noise(p);
        p *= 2.03;
        a *= 0.5;
    }
    return v;
}

vec3 bands(float lat, vec3 n, vec3 c0, vec3 c1, float freq, float wobble) {
    float t = sin(lat * freq + fbm(n * 3.0) * wobble) * 0.5 + 0.5;
    return mix(c0, c1, t);
}

vec3 surface(int body, vec3 n) {
    float lat = n.y;
    if (body == 0) {
        float g = fbm(n * 8.0);
        return mix(vec3(1.0, 0.55, 0.1), vec3(1.0, 0.95, 0.6), g);
    }
    if (body == 1) {
        float g = fbm(n * 6.0);
        float crater = smoothstep(0.62, 0.7, noise(n * 14.0));
        return vec3(0.45 + 0.25 * g) - crater * 0.15;
    }
    if (body == 2) {
        return bands(lat, n, vec3(0.85, 0.7, 0.45), vec3(0.95, 0.88, 0.65), 6.0, 3.0);
    }
    if (body == 3) {
        float land = fbm(n * 2.5 + 3.0);
        vec3 col = land > 0.52 ? mix(vec3(0.2, 0.45, 0.15), vec3(0.55, 0.45, 0.25), smoothstep(0.55, 0.7, land))
                               : vec3(0.05, 0.2, 0.55);
        float cloud = smoothstep(0.55, 0.75, fbm(n * 4.0 + 10.0));
        col = mix(col, vec3(1.0), cloud * 0.8);
        return mix(col, vec3(0.95), smoothstep(0.85, 0.9, abs(lat)));
    }
    if (body == 4) {
        float g = fbm(n * 4.0);
        vec3 col = mix(vec3(0.6, 0.22, 0.08), vec3(0.8, 0.4, 0.2), g);
        return mix(col, vec3(0.95), smoothstep(0.88, 0.93, lat));
    }
    if (body == 5) {
        vec3 col = bands(lat, n, vec3(0.6, 0.45, 0.3), vec3(0.95, 0.9, 0.8), 22.0, 1.5);
        vec2 spot = vec2(n.x - 0.35, (lat + 0.35) * 1.8);
        float s = smoothstep(0.2, 0.12, length(spot));
        return mix(col, vec3(0.75, 0.3, 0.15), s);
    }
    if (body == 6) {
        return bands(lat, n, vec3(0.8, 0.7, 0.45), vec3(0.95, 0.88, 0.65), 16.0, 0.8);
    }
    if (body == 7) {
        return bands(lat, n, vec3(0.55, 0.8, 0.85), vec3(0.65, 0.88, 0.9), 8.0, 0.4);
    }
    if (body == 9) {
        float g = fbm(n * 6.0);
        float crater = smoothstep(0.62, 0.7, noise(n * 14.0));
        return vec3(0.55 + 0.3 * g) - crater * 0.15;
    }
    if (body == 10) {
        vec3 col = bands(lat, n, vec3(0.5), vec3(1.0), 20.0, 1.6);
        vec2 spot = vec2(n.x + 0.3, (lat - 0.25) * 2.0);
        return mix(col, col * vec3(0.8, 0.5, 0.4), smoothstep(0.17, 0.1, length(spot)) * 0.8);
    }
    if (body == 11) {
        return bands(lat, n, vec3(0.62), vec3(0.95), 8.0, 0.7);
    }
    if (body == 12) {
        return vec3(0.75 + 0.5 * fbm(n * 8.0));
    }
    return bands(lat, n, vec3(0.15, 0.3, 0.75), vec3(0.3, 0.45, 0.9), 10.0, 1.2);
}

void main() {
    vec3 n = normalize(inWorldN);
    if (dot(n, pc.camPos.xyz - inWorldPos) < 0.0) discard;
    vec3 col = surface(inBody, normalize(inLocal)) * inTint;
    if (inBody == 0 && pc.sunT > 0.5) {
        float f = clamp(dot(n, normalize(pc.camPos.xyz - inWorldPos)), 0.0, 1.0);
        if (f > 0.45) discard;
        outFragColor = vec4(vec3(0.34, 0.27, 0.1) * (1.0 - f), 1.0);
        return;
    }
    if (inBody == 0 || inBody == 12) {
        float facing = clamp(dot(n, normalize(pc.camPos.xyz - inWorldPos)), 0.0, 1.0);
        outFragColor = vec4(col * (0.7 + 0.4 * facing), 1.0);
        return;
    }
    float diffuse = max(dot(n, normalize(-inWorldPos)), 0.0);
    float lit = 1.0;
    if (pc.sunT > 0.5) {
        float travel = length(inWorldPos) * pc.auPerUnit * 499.0;
        lit = 1.0 - smoothstep(travel - 25.0, travel, pc.sunT - 1.0);
    }
    outFragColor = vec4(col * (0.05 + 0.95 * diffuse * lit), 1.0);
}
