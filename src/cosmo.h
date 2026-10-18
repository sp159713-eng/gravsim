#pragma once

namespace cosmo {

constexpr double H0 = 67.7;
constexpr double OM = 0.31;
constexpr double OR = 9.0e-5;
constexpr double OL = 1.0 - OM - OR;
constexpr double GYR_S = 3.15576e16;
constexpr double YR_S = 3.15576e7;

double scaleAt(double tSec);
double timeAt(double a);
double hubble(double a);
double tempK(double a);
double decel(double a);
double nowSec();
double turnA();
double zOfComovingMly(double mly);
double lookbackGyrOfZ(double z);
double zOfLookbackGyr(double gyr);

}
