#version 450

layout(location = 0) in vec4 inPos;
layout(location = 1) in vec4 inVel;

layout(push_constant) uniform PushConsts {
    mat4 viewProj;
    uint solar;
    uint starCol;
    float trueMode;
    float pxPerUnit;
} pc;

layout(location = 0) out vec3 outColor;
layout(location = 1) flat out int outBody;
layout(location = 2) out vec2 outSunDir;

void main() {
    gl_Position = pc.viewProj * vec4(inPos.xyz, 1.0);
    float m = inPos.w;
    outBody = -1;
    outSunDir = vec2(0.0);
    float w = max(gl_Position.w, 0.01);
    bool planet = pc.solar != 0u && gl_VertexIndex < 9 && (gl_VertexIndex == 0 ? m > 100.0 : (pc.solar != 0u && m > 0.0001 && m < 99.0));
    if (pc.solar == 0u && (m <= 0.0 || m > 100.0)) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        gl_PointSize = 1.0;
        outColor = vec3(0.0);
    } else if (pc.solar != 0u && m <= 0.0) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        gl_PointSize = 1.0;
        outColor = vec3(0.0);
    } else if (planet) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        gl_PointSize = 1.0;
        outColor = vec3(0.0);
    } else if (inVel.w > 0.5 && m < 0.0000006) {
        gl_PointSize = 1.5;
        outColor = vec3(0.75, 0.68, 0.5);
    } else if (inVel.w > 0.5) {
        gl_PointSize = 4.0;
        if (pc.pxPerUnit > 0.0 && gl_VertexIndex >= 9 && gl_VertexIndex <= 13) {
            const float realR[5] = float[5](1.467e-5, 1.539e-5, 1.318e-5, 2.225e-5, 2.036e-5);
            const float enlR[5] = float[5](0.15, 0.12, 0.10, 0.18, 0.17);
            int mi = gl_VertexIndex - 9;
            float rad = pc.trueMode > 0.5 ? realR[mi] : enlR[mi];
            gl_PointSize = clamp(2.0 * rad * pc.pxPerUnit / w, 2.0, 48.0);
        }
        outColor = vec3(0.8, 0.8, 0.78);
    } else if (m > 0.000004 && m < 0.000006) {
        gl_PointSize = 6.0;
        outColor = vec3(0.7, 0.95, 1.0);
    } else if (m > 100.0) {
        gl_PointSize = 14.0;
        outColor = pc.starCol == 0u ? vec3(1.0, 0.85, 0.3) : unpackUnorm4x8(pc.starCol).rgb;
    } else {
        gl_PointSize = 2.0;
        float speed = length(inVel.xyz);
        outColor = mix(vec3(0.4, 0.7, 1.0), vec3(1.0, 0.5, 0.1), clamp(speed / 25.0, 0.0, 1.0));
        if (inVel.w < -0.5) outColor = mix(vec3(1.0, 0.45, 0.8), vec3(1.0, 0.95, 0.6), clamp(speed / 25.0, 0.0, 1.0));
        if (abs(inVel.w + 0.3) < 0.02) {
            gl_PointSize = 7.0;
            outColor = vec3(1.0, 0.92, 0.45);
        }
        if (m < 0.0001) outColor = mix(vec3(0.45, 0.4, 0.35), vec3(0.75, 0.7, 0.6), clamp(speed / 25.0, 0.0, 1.0));
    }
}
