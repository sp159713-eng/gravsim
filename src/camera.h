#pragma once

struct GLFWwindow;

struct Camera {
    float yaw = 0.6f;
    float pitch = 0.4f;
    float dist = 100.0f;
    float target[3] = {0.0f, 0.0f, 0.0f};
    float pos[3] = {31.2f, 23.4f, 45.6f};
    bool free = true;
    float minDist = 1.5f;
    float nearPlane = 0.1f;
    float moveMult = 1.0f;
    float zoomImpulse = 0.0f;
    bool lockLook = false;
    bool wheelHold = false;
    float wheelAcc = 0.0f;
};

void cameraAttach(GLFWwindow* window, Camera* camera);
bool cameraCursor(GLFWwindow* window, double& x, double& y);
void cameraEye(const Camera& camera, float eye[3]);
void cameraViewProj(const Camera& camera, float aspect, float out[16]);
void cameraViewProjEx(const Camera& camera, float aspect, const float* eyeOverride, float nearPlane, float farPlane, float out[16]);
