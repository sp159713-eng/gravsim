#include "camera.h"
#include <GLFW/glfw3.h>
#include <cmath>

struct InputState {
    bool mousePressed = false;
    double lastX = 0.0;
    double lastY = 0.0;
    double curX = 0.0;
    double curY = 0.0;
    bool curValid = false;
    bool inside = true;
};

static InputState g_inputState;

bool cameraCursor(GLFWwindow* window, double& x, double& y) {
    if (g_inputState.curValid) {
        x = g_inputState.curX;
        y = g_inputState.curY;
    } else {
        glfwGetCursorPos(window, &x, &y);
    }
    return g_inputState.inside;
}

extern float g_mouseSens;

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    (void)mods;
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            g_inputState.mousePressed = true;
            cameraCursor(window, g_inputState.lastX, g_inputState.lastY);
        } else if (action == GLFW_RELEASE) {
            g_inputState.mousePressed = false;
        }
    }
}

static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    g_inputState.curX = xpos;
    g_inputState.curY = ypos;
    g_inputState.curValid = true;
    g_inputState.inside = true;
    if (g_inputState.mousePressed) {
        Camera* camera = static_cast<Camera*>(glfwGetWindowUserPointer(window));
        if (camera && camera->lockLook) {
            g_inputState.lastX = xpos;
            g_inputState.lastY = ypos;
        } else if (camera) {
            double dx = xpos - g_inputState.lastX;
            double dy = ypos - g_inputState.lastY;
            const float sensitivity = 0.005f * g_mouseSens;
            float sign = camera->free ? -1.0f : 1.0f;
            camera->yaw += sign * static_cast<float>(dx) * sensitivity;
            camera->pitch -= sign * static_cast<float>(dy) * sensitivity;
            if (camera->pitch > 1.5f) camera->pitch = 1.5f;
            if (camera->pitch < -1.5f) camera->pitch = -1.5f;
        }
        g_inputState.lastX = xpos;
        g_inputState.lastY = ypos;
    }
}

static void cursorEnterCallback(GLFWwindow*, int entered) {
    g_inputState.inside = entered != 0;
}

extern double g_storyWheel;
extern float g_mouseSens;

static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)xoffset;
    g_storyWheel += yoffset;
    Camera* camera = static_cast<Camera*>(glfwGetWindowUserPointer(window));
    if (camera && camera->wheelHold) {
        camera->wheelAcc += static_cast<float>(yoffset);
        return;
    }
    if (camera) {
        if (camera->free && glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) != GLFW_PRESS) {
            camera->zoomImpulse += static_cast<float>(yoffset);
            return;
        }
        if (camera->free) {
            camera->moveMult *= std::pow(1.25f, static_cast<float>(yoffset));
            if (camera->moveMult < 0.02f) camera->moveMult = 0.02f;
            if (camera->moveMult > 50.0f) camera->moveMult = 50.0f;
            return;
        }
        if (yoffset > 0) camera->dist *= 0.9f;
        else if (yoffset < 0) camera->dist *= 1.1f;
        if (camera->dist < camera->minDist) camera->dist = camera->minDist;
        if (camera->dist > 500.0f) camera->dist = 500.0f;
    }
}

void cameraAttach(GLFWwindow* window, Camera* camera) {
    glfwSetWindowUserPointer(window, camera);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetCursorEnterCallback(window, cursorEnterCallback);
}

static void matMul(const float* a, const float* b, float* c) {
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a[row + 4 * k] * b[k + 4 * col];
            }
            c[row + 4 * col] = sum;
        }
    }
}

void cameraEye(const Camera& camera, float eye[3]) {
    if (camera.free) {
        eye[0] = camera.pos[0];
        eye[1] = camera.pos[1];
        eye[2] = camera.pos[2];
        return;
    }
    eye[0] = camera.target[0] + camera.dist * std::cos(camera.pitch) * std::sin(camera.yaw);
    eye[1] = camera.target[1] + camera.dist * std::sin(camera.pitch);
    eye[2] = camera.target[2] + camera.dist * std::cos(camera.pitch) * std::cos(camera.yaw);
}

void cameraViewProjEx(const Camera& camera, float aspect, const float* eyeOverride, float nearPlane, float farPlane, float out[16]) {
    float fx = -std::cos(camera.pitch) * std::sin(camera.yaw);
    float fy = -std::sin(camera.pitch);
    float fz = -std::cos(camera.pitch) * std::cos(camera.yaw);

    // Up vector U = (0, 1, 0)
    // Right vector r = normalize(cross(f, U))
    // f x U = (-fz, 0, fx)
    float rlen = std::sqrt(fz * fz + fx * fx);
    float rx = 0.0f, ry = 0.0f, rz = 0.0f;
    if (rlen > 1e-6f) {
        rx = -fz / rlen;
        ry = 0.0f;
        rz = fx / rlen;
    }

    // True up vector u = cross(r, f)
    float ux = ry * fz - rz * fy;
    float uy = rz * fx - rx * fz;
    float uz = rx * fy - ry * fx;

    // LookAt matrix V (column-major)
    float V[16] = {0};
    float eye[3];
    if (eyeOverride) { eye[0] = eyeOverride[0]; eye[1] = eyeOverride[1]; eye[2] = eyeOverride[2]; }
    else cameraEye(camera, eye);
    float ex = eye[0], ey = eye[1], ez = eye[2];
    V[0] = rx;  V[4] = ry;  V[8]  = rz;  V[12] = -(rx * ex + ry * ey + rz * ez);
    V[1] = ux;  V[5] = uy;  V[9]  = uz;  V[13] = -(ux * ex + uy * ey + uz * ez);
    V[2] = -fx; V[6] = -fy; V[10] = -fz; V[14] = fx * ex + fy * ey + fz * ez;
    V[3] = 0.0f;V[7] = 0.0f;V[11] = 0.0f;V[15] = 1.0f;

    // Perspective matrix P (Vulkan clip space: y flipped, depth 0..1)
    const float fovRad = 60.0f * 3.14159265358979323846f / 180.0f;
    float h = 1.0f / std::tan(fovRad * 0.5f);
    float w = h / aspect;

    float P[16] = {0};
    P[0] = w;
    P[5] = -h;
    P[10] = farPlane / (nearPlane - farPlane);
    P[11] = -1.0f;
    P[14] = (nearPlane * farPlane) / (nearPlane - farPlane);

    matMul(P, V, out);
}

void cameraViewProj(const Camera& camera, float aspect, float out[16]) {
    cameraViewProjEx(camera, aspect, nullptr, camera.nearPlane, 2000.0f, out);
}
