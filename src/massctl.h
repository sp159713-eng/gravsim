#pragma once
#include <string>

struct GLFWwindow;
struct Camera;

constexpr int MASS_KMIN = -7;
constexpr int MASS_KMAX = 10;
constexpr double MSUN_PER_GALAXY_UNIT = 4000.0;

double massMult(int k);
float massSize(int k);
int massSteps(GLFWwindow* w, Camera& cam, bool prev[2]);
std::string fmtMass(double msun, bool planetary);
void rayBasis(const Camera& c, double back[3], double right[3], double up[3]);
bool rayPlane(const double e[3], const double d[3], const double p0[3], const double n[3], double out[3]);
void cursorRay(const Camera& c, double W, double H, double fx, double fy, double e[3], double d[3]);
