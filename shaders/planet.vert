#version 450

layout(location = 0) in vec3 inLocal;
layout(location = 1) in vec4 inCenter;
layout(location = 2) in vec4 inStyle0;
layout(location = 3) in vec4 inStyle1;

layout(push_constant) uniform PushConsts {
    mat4 viewProj;
    vec4 camPos;
    float time;
    float pxPerUnit;
    float trueMode;
    float sunT;
    float auPerUnit;
} pc;

layout(location = 0) out vec3 outLocal;
layout(location = 1) out vec3 outWorldN;
layout(location = 2) out vec3 outWorldPos;
layout(location = 3) flat out int outBody;
layout(location = 4) flat out vec3 outTint;

void main() {
    outLocal = inLocal;
    outWorldN = vec3(0.0, 1.0, 0.0);
    outWorldPos = inCenter.xyz;
    outBody = int(inStyle0.z + 0.5);
    outTint = inStyle1.xyz;
    if (inStyle0.x < 0.0) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }
    float spin = pc.time / inStyle0.w * 6.2831853;
    float cs = cos(spin), sn = sin(spin);
    vec3 p = vec3(inLocal.x * cs - inLocal.z * sn, inLocal.y, inLocal.x * sn + inLocal.z * cs);
    float t = radians(inStyle1.w);
    float ct = cos(t), st = sin(t);
    vec3 n = vec3(p.x, p.y * ct + p.z * st, -p.y * st + p.z * ct);

    vec4 centreClip = pc.viewProj * vec4(inCenter.xyz, 1.0);
    float minPx = gl_InstanceIndex == 0 ? 8.0 : 3.0;
    float base = pc.trueMode > 0.5 ? inStyle0.x : inStyle0.y;
    float r = max(base, minPx * max(centreClip.w, 0.01) / pc.pxPerUnit);
    vec3 world = inCenter.xyz + n * r;

    outWorldN = n;
    outWorldPos = world;
    gl_Position = pc.viewProj * vec4(world, 1.0);
}
