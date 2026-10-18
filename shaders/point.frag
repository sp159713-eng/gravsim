#version 450

layout(location = 0) in vec3 inColor;
layout(location = 1) flat in int inBody;
layout(location = 2) in vec2 inSunDir;

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
    return bands(lat, n, vec3(0.15, 0.3, 0.75), vec3(0.3, 0.45, 0.9), 10.0, 1.2);
}

void main() {
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(coord, coord);
    if (r2 > 1.0) discard;
    if (inBody < 0) {
        outFragColor = vec4(inColor, 1.0);
        return;
    }
    vec3 n = vec3(coord.x, -coord.y, sqrt(1.0 - r2));
    vec3 col = surface(inBody, n);
    if (inBody == 0) {
        col *= 0.75 + 0.35 * n.z;
        outFragColor = vec4(col, 1.0);
        return;
    }
    vec3 light = normalize(vec3(inSunDir.x, -inSunDir.y, 0.35));
    float diffuse = max(dot(n, light), 0.0);
    outFragColor = vec4(col * (0.06 + 0.94 * diffuse), 1.0);
}
