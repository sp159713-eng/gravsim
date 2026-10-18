#pragma once

struct BodyInfo {
    const char* name;
    const char* type;
    double massKg;
    double radiusKm;
    double aAu;
    double periodDays;
    double dayHours;
    int moons;
    double tiltDeg;
    double gravity;
    double tempK;
    const char* fact;
};

constexpr int BODY_INFO_COUNT = 15;
extern const BodyInfo BODY_INFO[BODY_INFO_COUNT];
extern const float HOME_ENLARGED_RADIUS[8];
extern const float HOME_REAL_AU[8];
constexpr double EARTH_MASS_KG = 5.972e24;
constexpr double EARTH_RADIUS_KM = 6371.0;
constexpr double EARTH_G = 9.81;
constexpr double KM_PER_AU = 1.496e8;
constexpr double EARTH_ORBIT_KMS = 29.78;
constexpr double DAYS_PER_YEAR = 365.25;
