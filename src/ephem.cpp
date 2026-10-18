#include "ephem.h"
#include <cmath>

namespace {

const double PI_D = 3.14159265358979323846;
const double DEG = PI_D / 180.0;
long long g_epochDay = 20629;

struct Elem {
    double a0, aR, e0, eR, i0, iR, l0, lR, w0, wR, n0, nR;
};

const Elem TABLE[8] = {
    {0.38709927, 0.00000037, 0.20563593, 0.00001906, 7.00497902, -0.00594749, 252.25032350, 149472.67411175, 77.45779628, 0.16047689, 48.33076593, -0.12534081},
    {0.72333566, 0.00000390, 0.00677672, -0.00004107, 3.39467605, -0.00078890, 181.97909950, 58517.81538729, 131.60246718, 0.00268329, 76.67984255, -0.27769418},
    {1.00000261, 0.00000562, 0.01671123, -0.00004392, -0.00001531, -0.01294668, 100.46457166, 35999.37244981, 102.93768193, 0.32327364, 0.0, 0.0},
    {1.52371034, 0.00001847, 0.09339410, 0.00007882, 1.84969142, -0.00813131, -4.55343205, 19140.30268499, -23.94362959, 0.44441088, 49.55953891, -0.29257343},
    {5.20288700, -0.00011607, 0.04838624, -0.00013253, 1.30439695, -0.00183714, 34.39644051, 3034.74612775, 14.72847983, 0.21252668, 100.47390909, 0.20469106},
    {9.53667594, -0.00125060, 0.05386179, -0.00050991, 2.48599187, 0.00193609, 49.95424423, 1222.49362201, 92.59887831, -0.41897216, 113.66242448, -0.28867794},
    {19.18916464, -0.00196176, 0.04725744, -0.00004397, 0.77263783, -0.00242939, 313.23810451, 428.48202785, 170.95427630, 0.40805281, 74.01692503, 0.04240589},
    {30.06992276, 0.00026291, 0.00859048, 0.00005105, 1.77004347, 0.00035372, -55.12002969, 218.45945325, 44.96476227, -0.32241464, 131.78422574, -0.00508664},
};

double wrapPi(double x) {
    x = std::fmod(x, 2.0 * PI_D);
    if (x > PI_D) x -= 2.0 * PI_D;
    if (x < -PI_D) x += 2.0 * PI_D;
    return x;
}

double solveKepler(double M, double e) {
    double E = e < 0.8 ? M : PI_D;
    for (int k = 0; k < 60; k++) {
        double d = (E - e * std::sin(E) - M) / (1.0 - e * std::cos(E));
        E -= d;
        if (std::fabs(d) < 1e-12) break;
    }
    return E;
}

OrbitState keplerE(double a, double e, double inc, double node, double argPeri, double E, double gm) {
    double cE = std::cos(E), sE = std::sin(E), s1 = std::sqrt(1.0 - e * e);
    double r = a * (1.0 - e * cE);
    double px = a * (cE - e), py = a * s1 * sE;
    double k = std::sqrt(gm * a) / r;
    double vx = -k * sE, vy = k * s1 * cE;
    double cw = std::cos(argPeri), sw = std::sin(argPeri), ci = std::cos(inc), si = std::sin(inc), cn = std::cos(node), sn = std::sin(node);
    auto rot = [&](double x, double y, double* o) {
        double x1 = x * cw - y * sw, y1 = x * sw + y * cw;
        double y2 = y1 * ci, z2 = y1 * si;
        double X = x1 * cn - y2 * sn, Y = x1 * sn + y2 * cn;
        o[0] = X;
        o[1] = -z2;
        o[2] = Y;
    };
    OrbitState s{};
    rot(px, py, s.pos);
    rot(vx, vy, s.vel);
    return s;
}

OrbitState kepler(double a, double e, double inc, double node, double argPeri, double M, double gm) {
    return keplerE(a, e, inc, node, argPeri, solveKepler(wrapPi(M), e), gm);
}

}

void solarSetEpochDay(long long unixDay) {
    g_epochDay = unixDay;
}

double ephemJd(long long unixDay) {
    return (double)unixDay + 2440587.5 + 0.5;
}

double solarEpochJd() {
    return ephemJd(g_epochDay);
}

OrbitState ephemPlanet(int planet, double jd, double aSim, double gm) {
    const Elem& t = TABLE[planet];
    double T = (jd - 2451545.0) / 36525.0;
    double e = t.e0 + t.eR * T;
    double inc = (t.i0 + t.iR * T) * DEG;
    double L = t.l0 + t.lR * T, w = t.w0 + t.wR * T, node = t.n0 + t.nR * T;
    return kepler(aSim, e, inc, node * DEG, (w - node) * DEG, (L - w) * DEG, gm);
}

double ephemHelioLon(int planet, double jd) {
    const Elem& t = TABLE[planet];
    double T = (jd - 2451545.0) / 36525.0;
    double e = t.e0 + t.eR * T;
    double inc = (t.i0 + t.iR * T) * DEG;
    double L = t.l0 + t.lR * T, w = t.w0 + t.wR * T, node = t.n0 + t.nR * T;
    OrbitState s = kepler(1.0, e, inc, node * DEG, (w - node) * DEG, (L - w) * DEG, 1.0);
    return std::atan2(s.pos[2], s.pos[0]);
}

OrbitState ephemHalley(double jd, double gm, bool trueScale, double unitsPerAu) {
    double a = 17.834 * unitsPerAu, e = 0.96714;
    if (!trueScale) {
        a = 24.0;
        e = 0.875;
    }
    double period = 75.32 * 365.25;
    double M = 2.0 * PI_D * (jd - 2446470.5) / period;
    return kepler(a, e, 162.26 * DEG, 58.42 * DEG, 111.33 * DEG, M, gm);
}

double ephemMoonLon(double jd) {
    return wrapPi((218.316 + 13.176396 * (jd - 2451545.0)) * DEG);
}

OrbitState ephemKeplerM(double a, double e, double incDeg, double nodeDeg, double argDeg, double M, double gm) {
    return kepler(a, e, incDeg * DEG, nodeDeg * DEG, argDeg * DEG, M, gm);
}

OrbitState ephemKeplerE(double a, double e, double incDeg, double nodeDeg, double argDeg, double E, double gm) {
    return keplerE(a, e, incDeg * DEG, nodeDeg * DEG, argDeg * DEG, E, gm);
}
