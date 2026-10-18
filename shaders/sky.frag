#version 450

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConsts {
    mat4 invViewProj;
} pc;

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

vec3 stars(vec3 dir, float scale, float density) {
    vec3 p = dir * scale;
    vec3 cell = floor(p);
    float h = hash(cell);
    if (h < 1.0 - density) return vec3(0.0);
    vec3 centre = cell + 0.5 + (vec3(hash(cell + 1.7), hash(cell + 3.1), hash(cell + 5.3)) - 0.5) * 0.6;
    float d = length(p - centre);
    float glow = exp(-d * d * 40.0);
    float warm = hash(cell + 9.9);
    vec3 tint = mix(vec3(0.7, 0.8, 1.0), vec3(1.0, 0.85, 0.7), warm);
    return tint * glow * (0.4 + 0.6 * fract(h * 37.0));
}

void main() {
    vec4 nearP = pc.invViewProj * vec4(inNdc, 0.0, 1.0);
    vec4 farP = pc.invViewProj * vec4(inNdc, 1.0, 1.0);
    vec3 dir = normalize(farP.xyz / farP.w - nearP.xyz / nearP.w);

    vec3 band = normalize(vec3(0.3, 1.0, 0.45));
    float lat = dot(dir, band);
    float n = noise(dir * 6.0) * 0.6 + noise(dir * 14.0) * 0.3 + noise(dir * 30.0) * 0.1;
    float milky = exp(-lat * lat / 0.04) * (0.4 + 0.8 * n);
    vec3 color = vec3(0.06, 0.05, 0.09) * milky;

    color += stars(dir, 120.0, 0.06);
    color += stars(dir, 300.0, 0.03) * 0.7;
    color += stars(dir, 300.0, 0.12) * exp(-lat * lat / 0.03) * 0.5;

    outColor = vec4(color, 1.0);
}
