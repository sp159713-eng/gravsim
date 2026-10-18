#version 450

layout(location = 0) in vec4 inColor;
layout(location = 1) flat in float inNamed;
layout(location = 2) in vec2 inSunDir;

layout(location = 0) out vec4 outFragColor;

void main() {
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    float r2 = dot(c, c);
    if (r2 > 1.0) discard;
    if (inNamed > 0.5) {
        vec3 n = vec3(c.x, -c.y, sqrt(1.0 - r2));
        vec3 l = normalize(vec3(inSunDir.x, -inSunDir.y, 0.45));
        float d = max(dot(n, l), 0.0);
        float rim = smoothstep(0.7, 1.0, r2);
        vec3 col = inColor.rgb * (0.25 + 0.9 * d) + rim * 0.25;
        outFragColor = vec4(col, 1.0);
        return;
    }
    float f = 1.0 - r2;
    outFragColor = vec4(inColor.rgb * f, inColor.a * f);
}
