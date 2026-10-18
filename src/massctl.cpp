#include "massctl.h"
#include "camera.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

double massMult(int k) {
    return std::min(1000.0, std::max(0.01, std::pow(2.0, (double)k)));
}

float massSize(int k) {
    return (float)std::min(2.5, std::max(0.5, std::cbrt(massMult(k))));
}

int massSteps(GLFWwindow* w, Camera& cam, bool prev[2]) {
    bool up = glfwGetKey(w, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
    bool down = glfwGetKey(w, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS;
    int d = (up && !prev[1] ? 1 : 0) - (down && !prev[0] ? 1 : 0);
    prev[1] = up;
    prev[0] = down;
    cam.wheelAcc = std::max(-8.0f, std::min(8.0f, cam.wheelAcc));
    while (cam.wheelAcc >= 1.0f) { d++; cam.wheelAcc -= 1.0f; }
    while (cam.wheelAcc <= -1.0f) { d--; cam.wheelAcc += 1.0f; }
    return d;
}

namespace {

std::string grouped(double v) {
    char b[48];
    std::snprintf(b, sizeof(b), "%.0f", v);
    std::string s = b;
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ",");
    return s;
}

std::string num(double v) {
    char b[48];
    if (v >= 100.0) std::snprintf(b, sizeof(b), "%.0f", v);
    else if (v >= 10.0) std::snprintf(b, sizeof(b), "%.0f", v);
    else if (v >= 1.0) std::snprintf(b, sizeof(b), "%.1f", v);
    else std::snprintf(b, sizeof(b), "%.2g", v);
    return b;
}

}

static std::string withUnit(const std::string& n, const char* unit) {
    return n + " " + unit + (n == "1" || n == "1.0" ? "" : "S");
}

std::string fmtMass(double msun, bool planetary) {
    if (planetary) {
        double e = msun / 3.003e-6;
        if (e < 150.0) return withUnit(num(e), "EARTH");
        if (msun < 0.05) return withUnit(num(msun / 9.546e-4), "JUPITER");
    }
    if (msun >= 1e9) return num(msun / 1e9) + " BILLION SUNS";
    if (msun >= 1e6) return num(msun / 1e6) + " MILLION SUNS";
    if (msun >= 1000.0) return grouped(msun) + " SUNS";
    return withUnit(num(msun), "SUN");
}

void rayBasis(const Camera& c, double back[3], double right[3], double up[3]) {
    double cp = std::cos(c.pitch), sp = std::sin(c.pitch), sy = std::sin(c.yaw), cy = std::cos(c.yaw);
    back[0] = cp * sy; back[1] = sp; back[2] = cp * cy;
    right[0] = cy; right[1] = 0.0; right[2] = -sy;
    up[0] = back[1] * right[2] - back[2] * right[1];
    up[1] = back[2] * right[0] - back[0] * right[2];
    up[2] = back[0] * right[1] - back[1] * right[0];
}

bool rayPlane(const double e[3], const double d[3], const double p0[3], const double n[3], double out[3]) {
    double den = d[0] * n[0] + d[1] * n[1] + d[2] * n[2];
    if (std::fabs(den) < 1e-5) return false;
    double t = ((p0[0] - e[0]) * n[0] + (p0[1] - e[1]) * n[1] + (p0[2] - e[2]) * n[2]) / den;
    if (t <= 0.0) return false;
    for (int a = 0; a < 3; a++) out[a] = e[a] + t * d[a];
    return true;
}

void cursorRay(const Camera& c, double W, double H, double fx, double fy, double e[3], double d[3]) {
    double back[3], right[3], up[3];
    rayBasis(c, back, right, up);
    float eye[3];
    cameraEye(c, eye);
    double th = std::tan(30.0 * 3.14159265358979 / 180.0);
    double nx = (2.0 * fx / W - 1.0) * th * W / H, ny = -(2.0 * fy / H - 1.0) * th;
    for (int a = 0; a < 3; a++) {
        e[a] = eye[a];
        d[a] = -back[a] + right[a] * nx + up[a] * ny;
    }
}
