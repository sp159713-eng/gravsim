#include "cosmo.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace cosmo {

namespace {

constexpr double MPC_KM = 3.0856776e19;

double eOfA(double a) { return std::sqrt(OR / (a * a * a * a) + OM / (a * a * a) + OL); }

struct TimeTable {
    std::vector<double> lna, lnt;
};

const TimeTable& timeTable() {
    static TimeTable T = [] {
        TimeTable r;
        const int N = 7200;
        const double lo = std::log(1e-33), hi = std::log(1e3);
        const double h0s = H0 / MPC_KM;
        double step = (hi - lo) / N;
        double t = 1.0 / (2.0 * h0s * eOfA(std::exp(lo)));
        for (int i = 0; i <= N; i++) {
            double x = lo + step * i;
            if (i > 0) {
                double xm = x - 0.5 * step;
                t += step / (h0s * eOfA(std::exp(xm)));
            }
            r.lna.push_back(x);
            r.lnt.push_back(std::log(t));
        }
        return r;
    }();
    return T;
}

struct ZTable {
    std::vector<double> z, dc, lb;
};

const ZTable& zTable() {
    static ZTable T = [] {
        ZTable r;
        const double cH0 = 299792.458 / H0 * 3.26156;
        const double tH = 977.8 / H0;
        auto E = [](double q) { return std::sqrt(OR * std::pow(1 + q, 4) + OM * std::pow(1 + q, 3) + OL); };
        double dc = 0.0, lb = 0.0, z = 0.0;
        r.z.push_back(0.0);
        r.dc.push_back(0.0);
        r.lb.push_back(0.0);
        for (int k = 1; z < 1200.0; k++) {
            double dz = 0.002 * (1.0 + z), zm = z + 0.5 * dz;
            dc += cH0 * dz / E(zm);
            lb += tH * dz / ((1 + zm) * E(zm));
            z += dz;
            if (k % 8 == 0) {
                r.z.push_back(z);
                r.dc.push_back(dc);
                r.lb.push_back(lb);
            }
        }
        return r;
    }();
    return T;
}

double lerpAt(const std::vector<double>& xs, const std::vector<double>& ys, double x) {
    size_t i = std::upper_bound(xs.begin(), xs.end(), x) - xs.begin();
    if (i == 0) return ys.front();
    if (i >= xs.size()) return ys.back();
    double f = (x - xs[i - 1]) / (xs[i] - xs[i - 1]);
    return ys[i - 1] + f * (ys[i] - ys[i - 1]);
}

}

double scaleAt(double tSec) {
    const TimeTable& T = timeTable();
    double lt = std::log(std::max(tSec, 1e-60));
    if (lt <= T.lnt.front()) return std::exp(T.lna.front() + 0.5 * (lt - T.lnt.front()));
    return std::exp(lerpAt(T.lnt, T.lna, lt));
}

double timeAt(double a) {
    const TimeTable& T = timeTable();
    return std::exp(lerpAt(T.lna, T.lnt, std::log(a)));
}

double hubble(double a) { return H0 * eOfA(a); }

double tempK(double a) { return 2.7255 / a; }

double decel(double a) {
    double e2 = OR / (a * a * a * a) + OM / (a * a * a) + OL;
    return (OR / (a * a * a * a) + 0.5 * OM / (a * a * a) - OL) / e2;
}

double nowSec() { return timeAt(1.0); }

double turnA() {
    double lo = 0.2, hi = 1.0;
    for (int i = 0; i < 60; i++) {
        double mid = 0.5 * (lo + hi);
        if (decel(mid) > 0.0) lo = mid;
        else hi = mid;
    }
    return 0.5 * (lo + hi);
}

double zOfComovingMly(double mly) {
    const ZTable& T = zTable();
    return lerpAt(T.dc, T.z, mly);
}

double lookbackGyrOfZ(double z) {
    const ZTable& T = zTable();
    return lerpAt(T.z, T.lb, z);
}

double zOfLookbackGyr(double gyr) {
    const ZTable& T = zTable();
    return lerpAt(T.lb, T.z, gyr);
}

}
