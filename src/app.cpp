#include "vk_app.h"
#include "gpu.h"
#include "capture.h"
#include "help_page.h"
#include "solarx.h"
#include "belts.h"
#include "quiz.h"
#include "story.h"
#include "expansion.h"
#include "bhdisc.h"
#include "redshift.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace {

int g_pausePresses = 0;
int g_scalePresses = 0;

int pollKey(GLFWwindow* w, int key) {
    return timeTyping() ? GLFW_RELEASE : glfwGetKey(w, key);
}

void keyCallback(GLFWwindow*, int key, int, int action, int mods) {
    if (timeKeyHook(key, action)) return;
    if (action != GLFW_PRESS || (mods & (GLFW_MOD_ALT | GLFW_MOD_CONTROL | GLFW_MOD_SUPER))) return;
    if (key == GLFW_KEY_SPACE && !storyEatSpace()) g_pausePresses++;
    if (key == GLFW_KEY_T) g_scalePresses++;
}

void focusCallback(GLFWwindow*, int focused) {
    if (!focused) {
        g_pausePresses = 0;
        g_scalePresses = 0;
    }
}

}

int runApp(uint32_t n, Scene scene, float tourAt) {
    if (!glfwInit()) return -1;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "gravsim", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    AppContext app{};
    app.n = n;
    app.scene = scene;
    app.showSky = scene != Scene::SOLAR;
    app.window = window;
    applyScale(app);

    initVulkanBase(app);
    createSwapchain(app);
    createPipelines(app);
    createGridPipeline(app);
    createBloom(app);
    createOverlay(app);
    createSkyPipeline(app);
    createMicro(app);
    createGalaxy(app);
    createNebulae(app);
    createUniverse(app);
    createDark(app);
    createPlanets(app);
    createBlackHoles(app);
    createLifecycle(app);
    createStarSizes(app);
    createEarthMoon(app);
    createTitleBg(app);
    createFutureSky(app);
    createNightSky(app);
    createImpact(app);
    createEclipse(app);
    createExpansion(app);
    createBelts(app);
    createTimeDilation(app);
    createSgrA(app);
    createCommands(app);
    timeInit(app);
    solarxApplyFlags(app);
    createBuffers(app);
    visitSync(app);
    timeAttach(&app);

    cameraAttach(window, &app.camera);
    andromedaFrameReset(app);
    if (scene == Scene::BHSIZES) bhResetCamera(app);
    if (scene == Scene::STARSIZES) {
        app.showSky = false;
        starsRestart(app);
        if (tourAt >= 0.0f) {
            starsSetStop(app, (int)tourAt, 2.0f);
            app.stars.mode = 2;
        }
    }
    if (scene == Scene::SANDBOX) sbEnter(app);
    if (scene == Scene::EARTHMOON) emEnter(app);
    if (scene == Scene::TIMEDIL) tdEnter(app);
    if (scene == Scene::LAGRANGE) lgEnter(app);
    if (scene == Scene::LIGHTRIDE) lrEnter(app);
    if (scene == Scene::FUTURESKY) fsEnter(app);
    if (scene == Scene::NIGHTSKY) nsEnter(app);
    if (scene == Scene::IMPACT) imEnter(app);
    if (scene == Scene::ECLIPSE) ecEnter(app);
    if (scene == Scene::EXPANSION) exEnter(app);
    if (scene == Scene::BHDISC) bdEnter(app);
    if (g_throwDemo && (scene == Scene::SOLAR || scene == Scene::GALAXY)) throwDemoInit(app);
    if (scene == Scene::LIFECYCLE) {
        app.showSky = false;
        if (tourAt >= 100000.0f) {
            int pi = (int)(tourAt / 100000.0f);
            app.life.mass = 3;
            app.life.preset = std::min(pi - 1, 8);
            tourAt -= 100000.0f * (float)pi;
        } else if (tourAt >= 1000.0f) {
            app.life.mass = (int)(tourAt / 1000.0f);
            tourAt -= 1000.0f * (float)app.life.mass;
        }
        lifeRestart(app);
        app.life.t = std::max(0.0f, tourAt);
    }
    glfwSetKeyCallback(window, keyCallback);
    storyInit(app);
    captureInit(app);
    glfwSetWindowFocusCallback(window, focusCallback);

    uint32_t activeBufferIndex = 0;
    double lastTime = glfwGetTime();
    int frameCount = 0;
    bool gridKeyDown = false;
    const float speeds[] = {0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f};
    int speedIndex = g_throwDemo == 2 ? 5 : 2;
    int massStepPrev = 0;
    bool paused = scene == Scene::FUTURESKY;
    if (scene == Scene::EARTHMOON && tourAt >= 0.0f) {
        app.em.days = tourAt;
        paused = true;
    }
    bool upDown = false, downDown = false, labelDown = false, orbitDown = false, bloomDown = false, skyDown = false, galaxyDown = false, universeDown = false, darkDown = false;
    bool mouseDown = false, pressUi = false, helpDown = false, escDown = false, escUsed = false, backDown = false;
    bool startTour = tourAt >= 0.0f;
    float startTourAt = std::max(0.0f, tourAt);
    bool replayDown = false, replayDown0 = false, tourDown = false;
    bool zeroDown = false, lagDown = false, ridDown = false, fsDown = false, fsPrev = scene == Scene::FUTURESKY, nsDown = false, nsPrev = scene == Scene::NIGHTSKY, imDown = false, imPrev = scene == Scene::IMPACT, ecDown = false, ecPrev = scene == Scene::ECLIPSE, exDown = false, exPrev = scene == Scene::EXPANSION, bdDown = false, bdPrev = scene == Scene::BHDISC;
    bool sceneDown[9] = {false, false, false, false, false, false, false, false, false};
    int fps = 0;
    auto setTitle = [&]() {
        std::string title = std::string("gravsim - ") + sceneName(app.scene) + " - " + std::to_string(fps) + " FPS - "
            + (app.scene == Scene::SOLAR ? (paused ? std::string("paused") : timeRateLabel(app)) : paused ? std::string("paused") : "speed " + std::to_string(speeds[speedIndex]).substr(0, 4) + "x")
            + (app.scene == Scene::SOLAR ? (app.trueScale ? " - true scale" : " - enlarged") : "") + (app.camera.free ? " - free-fly" : " - follow")
            + " - press H for help";
        glfwSetWindowTitle(window, title.c_str());
    };
    bool pendingPick = false;
    bool storyHoldPrev = false, storySavedPause = false;
    double pressX = 0.0, pressY = 0.0, pickX = 0.0, pickY = 0.0;
    double frameClock = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        bool gridKey = pollKey(window, GLFW_KEY_G) == GLFW_PRESS;
        if (gridKey && !gridKeyDown) app.showGrid = !app.showGrid;
        gridKeyDown = gridKey;
        bool upKey = pollKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS || pollKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS;
        bool downKey = pollKey(window, GLFW_KEY_MINUS) == GLFW_PRESS || pollKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS;
        bool labelKey = pollKey(window, GLFW_KEY_L) == GLFW_PRESS;
        if (upKey && !upDown) { if (app.scene == Scene::SOLAR) timeRate(app, 1); else if (speedIndex < 6) speedIndex++; }
        if (downKey && !downDown) { if (app.scene == Scene::SOLAR) timeRate(app, -1); else if (speedIndex > 0) speedIndex--; }
        if (app.tm.pauseNow) {
            app.tm.pauseNow = false;
            paused = true;
        }
        if (app.tm.needReset) {
            app.tm.needReset = false;
            resetSystem(app);
            activeBufferIndex = 0;
        }
        if (g_pausePresses & 1) {
            if (app.scene == Scene::BHSIZES) bhTourToggle(app);
            else if (app.scene == Scene::LIFECYCLE) lifeToggle(app);
            else if (app.scene == Scene::STARSIZES) starsToggle(app);
            else if (app.tour.mode != 0) tourToggle(app);
            else if (app.scene != Scene::IMPACT && app.scene != Scene::ECLIPSE && app.scene != Scene::EXPANSION && app.scene != Scene::BHDISC) paused = !paused;
        }
        g_pausePresses = 0;
        if ((g_scalePresses & 1) && app.scene != Scene::NIGHTSKY && app.scene != Scene::IMPACT && app.scene != Scene::ECLIPSE && app.scene != Scene::EXPANSION && app.scene != Scene::BHDISC) {
            app.trueScale = !app.trueScale;
            resetSystem(app);
            activeBufferIndex = 0;
        }
        g_scalePresses = 0;
        bool replayKey = pollKey(window, GLFW_KEY_R) == GLFW_PRESS;
        if (replayKey && !replayDown && app.scene == Scene::BHSIZES) bhTourStart(app);
        if (replayKey && !replayDown && app.scene == Scene::LIFECYCLE) lifeRestart(app);
        if (replayKey && !replayDown && app.scene == Scene::STARSIZES) starsRestart(app);
        if (replayKey && !replayDown && app.scene == Scene::SANDBOX) sbRestart(app);
        if (replayKey && !replayDown && app.scene == Scene::EARTHMOON) emRestart(app);
        if (replayKey && !replayDown && app.scene == Scene::TIMEDIL) tdRestart(app);
        if (replayKey && !replayDown && app.scene == Scene::LAGRANGE) lgRestart(app);
        replayDown = replayKey;
        bool tourKey = pollKey(window, GLFW_KEY_P) == GLFW_PRESS;
        bool tourAsk = (tourKey && !tourDown) || (replayKey && !replayDown0 && app.tour.ran) || startTour;
        if (tourKey && !tourDown && app.scene == Scene::SANDBOX) sbPresetStep(app, pollKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || pollKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS ? -1 : 1);
        tourDown = tourKey;
        replayDown0 = replayKey;
        if (tourAsk && app.scene == Scene::SOLAR && app.visit.mode == 0 && !app.visit.active && !app.gal.active && (!startTour || app.displayValid)) {
            startTour = false;
            if (!app.trueScale) {
                app.trueScale = true;
                resetSystem(app);
                activeBufferIndex = 0;
            }
            tourStart(app, startTourAt);
            startTourAt = 0.0f;
        }
        if (labelKey && !labelDown && app.scene != Scene::NIGHTSKY && app.scene != Scene::IMPACT && app.scene != Scene::ECLIPSE && app.scene != Scene::EXPANSION && app.scene != Scene::BHDISC) app.showLabels = !app.showLabels;
        bool zeroNow = pollKey(window, GLFW_KEY_0) == GLFW_PRESS || pollKey(window, GLFW_KEY_KP_0) == GLFW_PRESS;
        if (zeroNow && !zeroDown) {
            switchScene(app, Scene::TIMEDIL);
            activeBufferIndex = 0;
            pendingPick = false;
            setTitle();
        }
        zeroDown = zeroNow;
        bool shiftHeld = pollKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || pollKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
        bool ridNow = shiftHeld && pollKey(window, GLFW_KEY_2) == GLFW_PRESS && !(app.scene == Scene::SANDBOX || throwActive(app));
        if (ridNow && !ridDown) {
            switchScene(app, Scene::LIGHTRIDE);
            activeBufferIndex = 0;
            pendingPick = false;
            setTitle();
        }
        ridDown = ridNow;
        bool lagNow = shiftHeld && pollKey(window, GLFW_KEY_1) == GLFW_PRESS && !(app.scene == Scene::SANDBOX || throwActive(app));
        if (lagNow && !lagDown) {
            switchScene(app, Scene::LAGRANGE);
            activeBufferIndex = 0;
            pendingPick = false;
            setTitle();
        }
        lagDown = lagNow;
        bool fsNow = shiftHeld && pollKey(window, GLFW_KEY_3) == GLFW_PRESS && !(app.scene == Scene::SANDBOX || throwActive(app));
        if (fsNow && !fsDown) {
            switchScene(app, Scene::FUTURESKY);
            paused = true;
            activeBufferIndex = 0;
            pendingPick = false;
            setTitle();
        }
        fsDown = fsNow;
        bool nsNow = shiftHeld && pollKey(window, GLFW_KEY_4) == GLFW_PRESS && !(app.scene == Scene::SANDBOX || throwActive(app));
        if (nsNow && !nsDown) {
            switchScene(app, Scene::NIGHTSKY);
            activeBufferIndex = 0;
            pendingPick = false;
            setTitle();
        }
        nsDown = nsNow;
        bool imNow = shiftHeld && pollKey(window, GLFW_KEY_5) == GLFW_PRESS && !(app.scene == Scene::SANDBOX || throwActive(app));
        if (imNow && !imDown) {
            switchScene(app, Scene::IMPACT);
            activeBufferIndex = 0;
            pendingPick = false;
            setTitle();
        }
        imDown = imNow;
        bool ecNow = shiftHeld && pollKey(window, GLFW_KEY_6) == GLFW_PRESS && !(app.scene == Scene::SANDBOX || throwActive(app));
        if (ecNow && !ecDown) {
            switchScene(app, Scene::ECLIPSE);
            activeBufferIndex = 0;
            pendingPick = false;
            setTitle();
        }
        ecDown = ecNow;
        bool exNow = shiftHeld && pollKey(window, GLFW_KEY_7) == GLFW_PRESS && !(app.scene == Scene::SANDBOX || throwActive(app));
        if (exNow && !exDown) {
            switchScene(app, Scene::EXPANSION);
            activeBufferIndex = 0;
            pendingPick = false;
            setTitle();
        }
        exDown = exNow;
        bool bdNow = shiftHeld && pollKey(window, GLFW_KEY_8) == GLFW_PRESS && !(app.scene == Scene::SANDBOX || throwActive(app));
        if (bdNow && !bdDown) {
            switchScene(app, Scene::BHDISC);
            activeBufferIndex = 0;
            pendingPick = false;
            setTitle();
        }
        bdDown = bdNow;
        const Scene sceneKeys[9] = {Scene::SOLAR, Scene::GALAXY, Scene::COLLISION, Scene::BHSIZES, Scene::LIFECYCLE, Scene::ANDROMEDA, Scene::STARSIZES, Scene::SANDBOX, Scene::EARTHMOON};
        for (int k = 0; k < 9; k++) {
            bool now = pollKey(window, GLFW_KEY_1 + k) == GLFW_PRESS || pollKey(window, GLFW_KEY_KP_1 + k) == GLFW_PRESS;
            if (k <= 7 && shiftHeld) now = false;
            if (app.scene == Scene::BHDISC && k < 3) now = false;
            if (app.scene == Scene::IMPACT && k < 5) now = false;
            if (app.scene == Scene::ECLIPSE && k < 5) now = false;
            if ((app.scene == Scene::SANDBOX || throwActive(app)) && k < 4) {
                sceneDown[k] = now;
                continue;
            }
            if (now && !sceneDown[k]) {
                switchScene(app, sceneKeys[k]);
                activeBufferIndex = 0;
                pendingPick = false;
                setTitle();
            }
            sceneDown[k] = now;
        }
        bool skyKey = pollKey(window, GLFW_KEY_B) == GLFW_PRESS;
        if (skyKey && !skyDown) app.showSky = !app.showSky;
        skyDown = skyKey;
        bool galaxyKey = pollKey(window, GLFW_KEY_M) == GLFW_PRESS;
        if (galaxyKey && !galaxyDown) {
            if (app.scene == Scene::LIFECYCLE) lifeCycleMass(app);
            else app.showGalaxy = !app.showGalaxy;
        }
        galaxyDown = galaxyKey;
        int massStep = (pollKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS || pollKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS ? 1 : 0)
            - (pollKey(window, GLFW_KEY_LEFT) == GLFW_PRESS || pollKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS ? 1 : 0);
        if (massStep != 0 && massStep != massStepPrev && app.scene == Scene::LIFECYCLE) lifeStepMass(app, massStep);
        if (massStep != 0 && massStep != massStepPrev && app.scene == Scene::STARSIZES) starsStep(app, massStep);
        massStepPrev = massStep;
        bool orbitKey = pollKey(window, GLFW_KEY_O) == GLFW_PRESS;
        if (orbitKey && !orbitDown) app.showOrbits = !app.showOrbits;
        orbitDown = orbitKey;
        redshiftKey(app);
        bool bloomKey = pollKey(window, GLFW_KEY_K) == GLFW_PRESS;
        if (bloomKey && !bloomDown) app.showBloom = !app.showBloom;
        bloomDown = bloomKey;
        bool darkKey = pollKey(window, GLFW_KEY_X) == GLFW_PRESS;
        if (darkKey && !darkDown) {
            if (app.scene == Scene::SANDBOX) sbToggleSun(app);
            else darkToggleKey(app);
        }
        darkDown = darkKey;
        bool universeKey = pollKey(window, GLFW_KEY_U) == GLFW_PRESS;
        if (universeKey && !universeDown) app.showUniverse = !app.showUniverse;
        universeDown = universeKey;
        bool mouseNow = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        double mx, my;
        bool cursorInside = cameraCursor(window, mx, my);
        {
            int fbw, fbh, ww, wh;
            glfwGetFramebufferSize(window, &fbw, &fbh);
            glfwGetWindowSize(window, &ww, &wh);
            app.cursorFb[0] = ww > 0 ? mx * fbw / ww : mx;
            app.cursorFb[1] = wh > 0 ? my * fbh / wh : my;
            app.cursorOk = cursorInside;
            app.mouseHeld = mouseNow;
        }
        bool backKey = pollKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
        if (backKey && !backDown) visitHome(app);
        backDown = backKey;
        app.helpHover = helpButtonHit(app, mx, my);
        if (mouseNow && !mouseDown) {
            pressX = mx;
            pressY = my;
            pressUi = helpButtonHit(app, mx, my) || helpPanelHit(app, mx, my) || quizPanelHit(app) || storyPanelHit(app);
        }
        if (!mouseNow && mouseDown && pressUi && helpButtonHit(app, mx, my) && helpButtonHit(app, pressX, pressY))
            app.showHelp = !app.showHelp;
        if (!mouseNow && mouseDown && !pressUi && std::fabs(mx - pressX) + std::fabs(my - pressY) < 5.0) {
            int fbw, fbh, ww, wh;
            glfwGetFramebufferSize(window, &fbw, &fbh);
            glfwGetWindowSize(window, &ww, &wh);
            double sx = ww > 0 ? (double)fbw / ww : 1.0, sy = wh > 0 ? (double)fbh / wh : 1.0;
            pendingPick = true;
            pickX = mx * sx;
            pickY = my * sy;
        }
        mouseDown = mouseNow;
        bool helpKey = pollKey(window, GLFW_KEY_H) == GLFW_PRESS;
        if (helpKey && !helpDown) app.showHelp = !app.showHelp;
        helpDown = helpKey;
        captureKeys(app);
        helpKeys(app);
        int quizFlags = storyUpdate(app);
        if (!storyBlocksQuiz()) quizFlags |= quizUpdate(app);
        bool storyHold = storyFrozen();
        if (storyHold && !storyHoldPrev) storySavedPause = paused;
        if (storyHold) paused = true;
        if (!storyHold && storyHoldPrev) paused = storySavedPause;
        storyHoldPrev = storyHold;
        if (quizFlags & 1) {
            activeBufferIndex = 0;
            pendingPick = false;
            setTitle();
        }
        if (quizFlags & 2) paused = true;
        if (quizFlags & 4) {
            startTour = true;
            startTourAt = quizTourAt();
        }
        bool escKey = pollKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        if (escKey && !escDown && !app.showHelp && (app.scene == Scene::FUTURESKY || app.scene == Scene::NIGHTSKY || app.scene == Scene::IMPACT || app.scene == Scene::ECLIPSE || app.scene == Scene::EXPANSION || app.scene == Scene::BHDISC)) {
            switchScene(app, Scene::SOLAR);
            activeBufferIndex = 0;
            pendingPick = false;
            setTitle();
        }
        if (escKey && !escDown && app.showHelp) {
            app.showHelp = false;
            escUsed = true;
        }
        if (!escKey) escUsed = false;
        if (escKey && !escDown && !app.showHelp && app.tour.mode != 0) {
            tourStop(app);
            resetSystem(app);
            activeBufferIndex = 0;
        }
        if (escKey && !escUsed && app.follow >= 0) enterFree(app);
        escDown = escKey;
        upDown = upKey; downDown = downKey; labelDown = labelKey;

        int width = 0, height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        if (width == 0 || height == 0) {
            continue;
        }

        vkWaitForFences(app.device, 1, &app.inFlightFences[app.currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(app.device, app.swapchain, UINT64_MAX,
            app.imageAvailableSemaphores[app.currentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            vkDeviceWaitIdle(app.device);
            cleanupSwapchain(app);
            createSwapchain(app);
            continue;
        }

        vkResetFences(app.device, 1, &app.inFlightFences[app.currentFrame]);
        captureBeginFrame(app);
        double clockNow = glfwGetTime();
        float frameDt = (float)(clockNow - frameClock);
        frameClock = clockNow;
        float speed = speeds[speedIndex];
        uint32_t steps = paused || app.scene == Scene::BHSIZES || app.scene == Scene::LIFECYCLE || app.scene == Scene::STARSIZES || app.scene == Scene::SANDBOX || app.scene == Scene::EARTHMOON || app.scene == Scene::TIMEDIL || app.scene == Scene::LAGRANGE || app.scene == Scene::LIGHTRIDE || app.scene == Scene::FUTURESKY || app.scene == Scene::NIGHTSKY || app.scene == Scene::IMPACT || app.scene == Scene::ECLIPSE || app.scene == Scene::EXPANSION || app.scene == Scene::BHDISC ? 0 : (speed >= 1.0f ? (uint32_t)speed : 1);
        float stepDt = (speed >= 1.0f ? 0.01f : 0.01f * speed) * (app.scene == Scene::ANDROMEDA ? 1.25f : 1.0f);
        if (app.scene == Scene::SOLAR) timePlan(app, frameDt, paused, steps, stepDt);
        if (app.scene == Scene::SOLAR && app.tour.mode != 0) steps = 0;
        if (app.scene == Scene::SOLAR) {
            steps *= SOLAR_SUB;
            stepDt /= (float)SOLAR_SUB;
        }
        SimParams params{stepDt, app.simG, app.simSoft, n};
        if (app.scene == Scene::SOLAR) params.bodySoft = app.simSoft * BODY_SOFT_RATIO;
        if (app.scene == Scene::ANDROMEDA) params.n = std::min(n, ANDROMEDA_N);
        params.bh = blackHoleCount(app);
        if (app.scene == Scene::SOLAR && app.tm.simN) params.n = std::min(n, std::max(app.tm.simN, throwReadEnd(app)));
        app.batchSteps[app.currentFrame] = steps;
        app.batchDt[app.currentFrame] = params.dt;
        params.prevDt = app.leapReset ? 0.0f : app.lastStepDt;
        app.batchPrev[app.currentFrame] = params.prevDt;
        if (steps > 0) {
            app.leapReset = false;
            app.lastStepDt = params.dt;
        }
        buildDisplayBodies(app);
        visitUpdate(app);
        galVisitUpdate(app);
        tourStep(app, frameDt);
        andromedaFrameStep(app, frameDt);
        if (app.scene == Scene::SANDBOX || app.scene == Scene::EARTHMOON || app.scene == Scene::TIMEDIL || app.scene == Scene::LAGRANGE || app.scene == Scene::LIGHTRIDE || app.scene == Scene::FUTURESKY || app.scene == Scene::NIGHTSKY || app.scene == Scene::IMPACT || app.scene == Scene::ECLIPSE || app.scene == Scene::EXPANSION || app.scene == Scene::BHDISC || throwActive(app)) pendingPick = false;
        if (pendingPick) {
            pendingPick = false;
            bool visited = closeupClick(app, pickX, pickY) || visitClick(app, pickX, pickY) || galVisitClick(app, pickX, pickY);
            int picked = visited ? -1 : pickBody(app, pickX, pickY);
            if (!visited && picked < 0) solarxClick(app, pickX, pickY);
            const Particle* bodies;
            uint32_t bodyCount;
            if (picked >= 0 && picked != app.follow && currentBodies(app, bodies, bodyCount)) {
                float target[3];
                bodyPosition(app, bodies, (uint32_t)picked, target);
                enterFollow(app, picked, target);
            }
        }
        if (app.scene == Scene::SANDBOX) sbUpdate(app, frameDt, paused ? 0.0f : speeds[speedIndex], mouseNow, pressUi);
        app.lg.paused = paused;
        app.lg.speed = speeds[speedIndex];
        if (app.scene == Scene::LAGRANGE) lgUpdate(app, frameDt, mouseNow, pressUi);
        if (fsPrev && app.scene != Scene::FUTURESKY) paused = false;
        fsPrev = app.scene == Scene::FUTURESKY;
        if (nsPrev && app.scene != Scene::NIGHTSKY) nsLeave(app);
        nsPrev = app.scene == Scene::NIGHTSKY;
        if (app.scene == Scene::NIGHTSKY) nsUpdate(app, frameDt, mouseNow && !pressUi);
        if (imPrev && app.scene != Scene::IMPACT) imLeave(app);
        imPrev = app.scene == Scene::IMPACT;
        if (app.scene == Scene::IMPACT) imUpdate(app, frameDt, mouseNow && !pressUi);
        if (ecPrev && app.scene != Scene::ECLIPSE) ecLeave(app);
        ecPrev = app.scene == Scene::ECLIPSE;
        if (app.scene == Scene::ECLIPSE) ecUpdate(app, frameDt, mouseNow && !pressUi);
        if (exPrev && app.scene != Scene::EXPANSION) exLeave(app);
        exPrev = app.scene == Scene::EXPANSION;
        if (app.scene == Scene::EXPANSION) exUpdate(app, frameDt);
        if (bdPrev && app.scene != Scene::BHDISC) bdLeave(app);
        bdPrev = app.scene == Scene::BHDISC;
        if (app.scene == Scene::BHDISC) bdUpdate(app, frameDt);
        if (app.scene == Scene::FUTURESKY) {
            if (fsUpdate(app, frameDt, !paused, massStep, shiftHeld, mouseNow && !pressUi)) paused = true;
        }
        if (app.scene == Scene::LIGHTRIDE) {
            app.lr.paused = paused;
            lrUpdate(app, frameDt, mouseNow && !pressUi, escKey && !escUsed);
            if (app.lr.exitReq) {
                app.lr.exitReq = false;
                switchScene(app, Scene::SOLAR);
                activeBufferIndex = 0;
                pendingPick = false;
                setTitle();
            }
        }
        throwUpdate(app, frameDt, mouseNow, pressUi);
        solarxStep(app, frameDt);
        app.em.paused = paused;
        app.em.speed = speeds[speedIndex];
        app.td.paused = paused;
        app.td.speed = speeds[speedIndex];
        updateCamera(app, frameDt);
        gwUpdate(app);
        updateOverlay(app);
        storyOverlay(app);

        VkCommandBuffer cmd = app.commandBuffers[app.currentFrame];
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cmd, &beginInfo);
        throwRecord(app, cmd);
        solarxRecord(app, cmd);
        bdRecord(app, cmd);

        VkMemoryBarrier full{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr,
            VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT};
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0, 1, &full, 0, nullptr, 0, nullptr);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, app.computePipeline);
        vkCmdPushConstants(cmd, app.computeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SimParams), &params);
        app.simTime += steps * params.dt;
        for (uint32_t step = 0; step < steps; step++) {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, app.computeLayout, 0, 1,
                &app.descriptorSets[activeBufferIndex], 0, nullptr);
            vkCmdDispatch(cmd, (n + 255) / 256, 1, 1);
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0, 1, &full, 0, nullptr, 0, nullptr);
            if (step == 0 && steps > 1) {
                params.prevDt = params.dt;
                vkCmdPushConstants(cmd, app.computeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(SimParams), &params);
            }
            activeBufferIndex ^= 1;
        }
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 1, &full, 0, nullptr, 0, nullptr);
        recordReadback(app, cmd, app.particleBuffers[activeBufferIndex]);

        // Begin render pass
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = app.renderPass;
        renderPassInfo.framebuffer = app.framebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = app.swapchainExtent;
        VkClearValue clearColor = {{{0.02f, 0.02f, 0.04f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
        auto beginMain = [&]() {
            vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            // Set dynamic viewport/scissor
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float)app.swapchainExtent.width;
            viewport.height = (float)app.swapchainExtent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmd, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = app.swapchainExtent;
            vkCmdSetScissor(cmd, 0, 1, &scissor);

        };
        float aspect = (float)app.swapchainExtent.width / (float)app.swapchainExtent.height;
        float viewProj[16];
        if (app.scene == Scene::BHSIZES) bhViewProj(app, aspect, viewProj);
        else cameraViewProj(app.camera, aspect, viewProj);
        auto drawScene = [&]() {
            if (storyTitleActive()) {
                drawTitleBg(app, cmd, aspect);
            } else if (closeupCovers(app)) {
                closeupDraw(app, cmd, aspect);
            } else if (app.scene == Scene::LIFECYCLE) {
                drawLifecycle(app, cmd, aspect);
            } else if (app.scene == Scene::STARSIZES) {
                drawStarSizes(app, cmd, aspect);
            } else if (app.scene == Scene::TIMEDIL) {
                drawTimeDilation(app, cmd, aspect);
            } else if (app.scene == Scene::NIGHTSKY) {
                drawNightSky(app, cmd, aspect);
            } else if (app.scene == Scene::IMPACT) {
                drawImpact(app, cmd, aspect);
            } else if (app.scene == Scene::ECLIPSE) {
                drawEclipse(app, cmd, aspect);
            } else if (app.scene == Scene::EXPANSION) {
                drawExpansion(app, cmd, aspect);
            } else if (app.scene == Scene::BHDISC) {
                drawSky(app, cmd, viewProj);
                drawBhDisc(app, cmd, aspect, viewProj);
            } else if (app.scene == Scene::FUTURESKY) {
                drawFutureSky(app, cmd, aspect);
            } else if (app.scene == Scene::LIGHTRIDE) {
                drawLightRide(app, cmd, aspect);
            } else if (app.scene == Scene::LAGRANGE) {
                drawLagrange(app, cmd, aspect);
            } else if (app.scene == Scene::EARTHMOON) {
                drawEarthMoon(app, cmd, aspect);
            } else if (app.scene == Scene::SANDBOX) {
                (void)0;
            } else {
                drawSky(app, cmd, viewProj);
                drawUniverse(app, cmd, aspect, (float)app.swapchainExtent.height);
                drawDark(app, cmd, aspect);
                drawGalaxy(app, cmd, aspect);
                drawNebulae(app, cmd, aspect);
                drawSgrA(app, cmd, aspect);
                drawTourHole(app, cmd, aspect);
                bool solarFar = app.scene == Scene::SOLAR && app.refDist > 2000.0f;
                if (app.scene != Scene::BHSIZES && !solarFar && !(app.scene == Scene::SOLAR && app.gal.active && !app.visit.active)) drawGrid(app, cmd, 1 - activeBufferIndex, viewProj);
                drawBelts(app, cmd, viewProj);

                // Bind graphics pipeline
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, app.graphicsPipeline);
                vkCmdPushConstants(cmd, app.graphicsLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, viewProj);
                uint32_t starCol = 0;
                if (app.visit.active) {
                    auto byte = [](float v) { return (uint32_t)(std::fmin(std::fmax(v, 0.0f), 1.0f) * 255.0f); };
                    starCol = byte(app.visit.sys.color[0]) | (byte(app.visit.sys.color[1]) << 8) | (byte(app.visit.sys.color[2]) << 16) | (255u << 24);
                }
                struct SolarFlags { uint32_t solar, starCol; float trueMode, pxPerUnit; } solarFlag = {app.scene != Scene::SOLAR ? 0u : 1u, starCol, app.trueScale ? 1.0f : 0.0f, 0.0f};
                if (app.scene == Scene::SOLAR && !app.visit.active && std::fabs(viewProj[5]) > 0.0f)
                    solarFlag.pxPerUnit = 0.5f * (float)app.swapchainExtent.height / std::tan(30.0f * 3.14159265f / 180.0f);
                vkCmdPushConstants(cmd, app.graphicsLayout, VK_SHADER_STAGE_VERTEX_BIT, 64, 16, &solarFlag);

                // Bind output particle storage buffer as vertex buffer and draw
                VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(cmd, 0, 1, &app.particleBuffers[activeBufferIndex], &offset);
                bool homeHidden = app.scene == Scene::SOLAR && app.gal.active && !app.visit.active;
                if (app.scene != Scene::BHSIZES && !solarFar && !homeHidden) vkCmdDraw(cmd, n, 1, 0, 0);
                if (!solarFar && !homeHidden) {
                    drawBlackHoles(app, cmd, viewProj, app.particleBuffers[activeBufferIndex]);
                    drawPlanets(app, cmd, viewProj, app.particleBuffers[activeBufferIndex]);
                }
                drawMicro(app, cmd, aspect);
                closeupDraw(app, cmd, aspect);
            }
        };
        if (bloomActive(app)) {
            bloomBeginScene(app, cmd);
            drawScene();
            bloomEndScene(app, cmd);
            beginMain();
            bloomComposite(app, cmd);
        } else {
            beginMain();
            drawScene();
        }
        drawOverlay(app, cmd);
        captureDraw(app, cmd);

        vkCmdEndRenderPass(cmd);
        captureRecordCopy(app, cmd, imageIndex);
        vkEndCommandBuffer(cmd);

        // Submit command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = {app.imageAvailableSemaphores[app.currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        VkSemaphore signalSemaphores[] = {app.renderFinishedSemaphores[app.currentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkQueueSubmit(app.graphicsQueue, 1, &submitInfo, app.inFlightFences[app.currentFrame]);

        // Present swapchain image
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapchains[] = {app.swapchain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(app.presentQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            vkDeviceWaitIdle(app.device);
            cleanupSwapchain(app);
            createSwapchain(app);
        }

        app.currentFrame = (app.currentFrame + 1) % 2;

        // Measure FPS
        frameCount++;
        double currentTime = glfwGetTime();
        if (currentTime - lastTime >= 1.0) {
            fps = (int)std::round(frameCount / (currentTime - lastTime));
            setTitle();
            frameCount = 0;
            lastTime = currentTime;
        }
    }

    vkDeviceWaitIdle(app.device);
    captureShutdown(app);

    cleanupSwapchain(app);

    for (size_t i = 0; i < 2; i++) {
        vkDestroyBuffer(app.device, app.particleBuffers[i], nullptr);
        vkFreeMemory(app.device, app.particleMemories[i], nullptr);
        vkDestroySemaphore(app.device, app.imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(app.device, app.renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(app.device, app.inFlightFences[i], nullptr);
    }

    destroyBloom(app);
    destroyOverlay(app);
    destroySky(app);
    destroyMicro(app);
    destroyBelts(app);
    destroyNebulae(app);
    destroyGalaxy(app);
    destroyUniverse(app);
    destroyDark(app);
    destroyPlanets(app);
    destroyBlackHoles(app);
    destroyLifecycle(app);
    destroyStarSizes(app);
    destroyEarthMoon(app);
    destroyTitleBg(app);
    destroyFutureSky(app);
    destroyNightSky(app);
    destroyImpact(app);
    destroyEclipse(app);
    destroyExpansion(app);
    destroyBhDisc(app);
    destroyTimeDilation(app);
    vkDestroyDescriptorPool(app.device, app.descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(app.device, app.computeDSL, nullptr);
    vkDestroyPipeline(app.device, app.computePipeline, nullptr);
    vkDestroyPipelineLayout(app.device, app.computeLayout, nullptr);
    vkDestroyPipeline(app.device, app.graphicsPipeline, nullptr);
    vkDestroyPipeline(app.device, app.gridPipeline, nullptr);
    vkDestroyPipelineLayout(app.device, app.gridLayout, nullptr);
    vkDestroyPipelineLayout(app.device, app.graphicsLayout, nullptr);
    vkDestroyCommandPool(app.device, app.commandPool, nullptr);

    vkDestroyDevice(app.device, nullptr);
    vkDestroySurfaceKHR(app.instance, app.surface, nullptr);
    vkDestroyInstance(app.instance, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
