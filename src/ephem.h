#pragma once

struct OrbitState {
    double pos[3];
    double vel[3];
};

void solarSetEpochDay(long long unixDay);
double solarEpochJd();
double ephemJd(long long unixDay);
double ephemHelioLon(int planet, double jd);
OrbitState ephemPlanet(int planet, double jd, double aSim, double gm);
OrbitState ephemHalley(double jd, double gm, bool trueScale, double unitsPerAu);
double ephemMoonLon(double jd);
OrbitState ephemKeplerM(double a, double e, double incDeg, double nodeDeg, double argDeg, double M, double gm);
OrbitState ephemKeplerE(double a, double e, double incDeg, double nodeDeg, double argDeg, double E, double gm);
