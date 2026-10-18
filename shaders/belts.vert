#version 450

layout(location = 0) in vec4 inA;
layout(location = 1) in vec4 inB;
layout(location = 2) in vec4 inC;
layout(location = 3) in vec4 inD;

layout(push_constant) uniform PC {
    mat4 vp;
    vec4 sun;
    vec4 eye;
    vec4 tm;
} pc;

layout(location = 0) out vec4 outColor;
layout(location = 1) flat out float outNamed;
layout(location = 2) out vec2 outSunDir;

const float TAU = 6.28318530718;

void main() {
    float t = pc.tm.x;
    int mode = int(inC.z + 0.5);
    float base = inC.x * t;
    if (mode == 1) base = pc.tm.y;
    else if (mode == 2) base = 1.5 * pc.tm.y;
    else if (mode == 3) base = 0.6666667 * pc.tm.z;
    float M = inB.y + base + inB.z * sin(inC.y * t + inB.w);
    M = M - TAU * floor(M / TAU);
    float e = inA.y;
    float E = e > 0.6 ? 3.14159265 : M;
    for (int i = 0; i < 9; i++) E -= (E - e * sin(E) - M) / (1.0 - e * cos(E));
    float a = inA.x;
    float px = a * (cos(E) - e), py = a * sqrt(1.0 - e * e) * sin(E);
    float cw = cos(inB.x), sw = sin(inB.x), ci = cos(inA.z), si = sin(inA.z), cn = cos(inA.w), sn = sin(inA.w);
    float x1 = px * cw - py * sw, y1 = px * sw + py * cw;
    float y2 = y1 * ci, z2 = y1 * si;
    vec3 rel = vec3(x1 * cn - y2 * sn, -z2, x1 * sn + y2 * cn);
    vec3 world = pc.sun.xyz + rel * pc.sun.w;
    gl_Position = pc.vp * vec4(world, 1.0);
    float upa = pc.sun.w;
    float dAu = length(world - pc.eye.xyz) / upa;
    int cls = int(inC.w + 0.5);
    float h = fract(sin(float(gl_VertexIndex) * 12.9898) * 43758.5453);
    float gain = pc.eye.w;
    float alpha, size;
    outNamed = 0.0;
    if (cls == 2) {
        float w = max(gl_Position.w, 0.001);
        float truePx = 2.0 * inD.w * upa * pc.tm.w / w;
        size = clamp(max(truePx, 7.0 + 38.0 * smoothstep(0.5, 0.004, dAu * upa)), 7.0, 46.0);
        alpha = 1.0;
        outNamed = 1.0;
    } else if (cls == 1) {
        size = 1.8 + 3.5 * smoothstep(1.5, 0.05, dAu) + 0.8 * h;
        alpha = gain * 0.55 * (0.55 + 0.45 * h) * (1.0 - smoothstep(500.0, 1500.0, dAu));
    } else {
        size = 1.2 + 3.4 * smoothstep(0.6, 0.03, dAu) + 0.5 * h;
        alpha = gain * (0.45 + 0.55 * h) * (1.0 - smoothstep(30.0, 90.0, dAu));
    }
    alpha *= smoothstep(0.0003, 0.01, dAu);
    gl_PointSize = size;
    outColor = vec4(inD.rgb * alpha, alpha);
    vec4 sc = pc.vp * vec4(pc.sun.xyz, 1.0);
    vec2 dir = sc.xy / max(abs(sc.w), 1e-6) - gl_Position.xy / max(gl_Position.w, 1e-6);
    outSunDir = length(dir) > 1e-6 ? normalize(dir) : vec2(0.0, 1.0);
}
