#include <gpu.h>
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>

extern bool g_sandboxDemo;
extern float g_exAtGyr;
extern float g_bdAtSec;
extern int g_bdScen;
extern bool g_redshift;
int cosmoCheck();
extern bool g_sandboxSolar;
extern float g_tdRadius;
extern bool g_tdMiller;
extern double g_lrStartAt;
extern double g_fsStartAt;
extern int g_imPreset;
extern float g_imAt;
extern int g_ecView;
extern float g_ecPhase;
extern bool g_ecApogee;
extern int g_ecSeason;
extern std::string g_nsFrom;
extern bool g_nsLookSet;
extern float g_nsLook[2];
extern float g_nsLat;
extern float g_nsLst;
extern float g_nsFov;
extern bool g_nsLines;
extern std::string g_nsGo;
extern bool g_lgJupiter;
extern bool g_lgDemo;
extern int g_sandboxPreset;
extern int g_throwDemo;
extern std::string g_closeupName;
extern std::string g_sxDate;
extern bool g_sxNosunDemo;
extern std::string g_sxCamTo;
extern float g_sxCam[3];
extern bool g_sxCamSet;
extern int g_sxRate;
extern float g_capShotAt;
extern int g_quizStartQ;
extern int g_quizStartAnswer;
extern int g_quizFinal;
extern float g_capRecAt;
extern float g_capRecFor;
extern int g_storyBoot;
extern int g_storyAtCh;
extern int g_storyAtObj;
extern int g_storyUi;
extern bool g_storyMute;
extern int g_storyScreen;
extern int g_storySel;
extern bool g_ch1AutoRock;

std::string shaderPath(const char* name) {
    char buffer[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, buffer, MAX_PATH);
    if (len == 0) {
        return "";
    }
    std::string path(buffer, len);
    size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos) {
        return "";
    }
    std::string dir = path.substr(0, pos);
    return dir + "/shaders/" + name + ".spv";
}

int main(int argc, char* argv[]) {
    bool selfTest = false;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--selftest") == 0) {
            selfTest = true;
            break;
        }
    }
    if (selfTest) {
        return runSelfTest();
    }

    uint32_t n = 4096;
    Scene scene = Scene::SOLAR;
    float tour = -1.0f;
    bool plainLaunch = true;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--n") == 0 && i + 1 < argc) {
            ++i;
            continue;
        }
        plainLaunch = false;
    }
    if (plainLaunch) g_storyBoot = 1;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--title") == 0)
            g_storyBoot = 1;
        if (strcmp(argv[i], "--story") == 0) {
            g_storyBoot = 2;
            g_storyAtCh = 1;
            g_storyAtObj = 1;
        }
        if (strcmp(argv[i], "--story-at") == 0 && i + 1 < argc) {
            g_storyBoot = 2;
            g_storyAtCh = 1;
            g_storyAtObj = 1;
            std::sscanf(argv[i + 1], "%d.%d", &g_storyAtCh, &g_storyAtObj);
        }
        if (strcmp(argv[i], "--story-ui") == 0 && i + 1 < argc)
            g_storyUi = atoi(argv[i + 1]);
        if (strcmp(argv[i], "--title-screen") == 0 && i + 1 < argc)
            g_storyScreen = atoi(argv[i + 1]);
        if (strcmp(argv[i], "--title-sel") == 0 && i + 1 < argc)
            g_storySel = atoi(argv[i + 1]);
        if (strcmp(argv[i], "--story-autorock") == 0)
            g_ch1AutoRock = true;
        if (strcmp(argv[i], "--mute") == 0)
            g_storyMute = true;
    }
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--n") == 0 && i + 1 < argc)
            n = static_cast<uint32_t>(std::stoi(argv[i + 1]));
        if (strcmp(argv[i], "--tour") == 0)
            tour = 0.0f;
        if (strcmp(argv[i], "--tour-at") == 0 && i + 1 < argc)
            tour = std::stof(argv[i + 1]);
        if (strcmp(argv[i], "--closeup") == 0 && i + 1 < argc)
            g_closeupName = argv[i + 1];
        if (strcmp(argv[i], "--shot-at") == 0 && i + 1 < argc)
            g_capShotAt = (float)atof(argv[i + 1]);
        if (strcmp(argv[i], "--rec-at") == 0 && i + 1 < argc)
            g_capRecAt = (float)atof(argv[i + 1]);
        if (strcmp(argv[i], "--rec-for") == 0 && i + 1 < argc)
            g_capRecFor = (float)atof(argv[i + 1]);
        if (strcmp(argv[i], "--quiz-q") == 0 && i + 1 < argc)
            g_quizStartQ = atoi(argv[i + 1]);
        if (strcmp(argv[i], "--quiz-answer") == 0 && i + 1 < argc)
            g_quizStartAnswer = atoi(argv[i + 1]);
        if (strcmp(argv[i], "--quiz-final") == 0 && i + 1 < argc)
            g_quizFinal = atoi(argv[i + 1]);
        if (strcmp(argv[i], "--galaxy") == 0)
            scene = Scene::GALAXY;
        if (strcmp(argv[i], "--collision") == 0)
            scene = Scene::COLLISION;
        if (strcmp(argv[i], "--andromeda") == 0)
            scene = Scene::ANDROMEDA;
        if (strcmp(argv[i], "--lifecycle") == 0)
            scene = Scene::LIFECYCLE;
        if (strcmp(argv[i], "--bhsizes") == 0)
            scene = Scene::BHSIZES;
        if (strcmp(argv[i], "--starsizes") == 0)
            scene = Scene::STARSIZES;
        if (strcmp(argv[i], "--throw-demo") == 0 && i + 1 < argc) {
            bool gal = strcmp(argv[i + 1], "galaxy") == 0;
            scene = gal ? Scene::GALAXY : Scene::SOLAR;
            g_throwDemo = gal ? 2 : 1;
        }
        if (strcmp(argv[i], "--date") == 0 && i + 1 < argc)
            g_sxDate = argv[i + 1];
        if (strcmp(argv[i], "--nosun-demo") == 0)
            g_sxNosunDemo = true;
        if (strcmp(argv[i], "--camto") == 0 && i + 1 < argc)
            g_sxCamTo = argv[i + 1];
        if (strcmp(argv[i], "--cam") == 0 && i + 3 < argc) {
            g_sxCam[0] = (float)atof(argv[i + 1]);
            g_sxCam[1] = (float)atof(argv[i + 2]);
            g_sxCam[2] = (float)atof(argv[i + 3]);
            g_sxCamSet = true;
        }
        if (strcmp(argv[i], "--rate") == 0 && i + 1 < argc)
            g_sxRate = atoi(argv[i + 1]);
        if (strcmp(argv[i], "--earthmoon") == 0)
            scene = Scene::EARTHMOON;
        if (strcmp(argv[i], "--timedilation") == 0)
            scene = Scene::TIMEDIL;
        if (strcmp(argv[i], "--td-r") == 0 && i + 1 < argc)
            g_tdRadius = (float)atof(argv[i + 1]);
        if (strcmp(argv[i], "--lightride") == 0)
            scene = Scene::LIGHTRIDE;
        if (strcmp(argv[i], "--lightride-at") == 0 && i + 1 < argc) {
            scene = Scene::LIGHTRIDE;
            g_lrStartAt = atof(argv[i + 1]);
        }
        if (strcmp(argv[i], "--futuresky") == 0)
            scene = Scene::FUTURESKY;
        if (strcmp(argv[i], "--futuresky-at") == 0 && i + 1 < argc) {
            scene = Scene::FUTURESKY;
            g_fsStartAt = atof(argv[i + 1]);
        }
        if (strcmp(argv[i], "--impact") == 0)
            scene = Scene::IMPACT;
        if (strcmp(argv[i], "--impact-preset") == 0 && i + 1 < argc) {
            scene = Scene::IMPACT;
            g_imPreset = atoi(argv[i + 1]);
        }
        if (strcmp(argv[i], "--impact-at") == 0 && i + 1 < argc) {
            scene = Scene::IMPACT;
            g_imAt = (float)atof(argv[i + 1]);
        }
        if (strcmp(argv[i], "--expansion") == 0)
            scene = Scene::EXPANSION;
        if (strcmp(argv[i], "--expansion-at") == 0 && i + 1 < argc) {
            scene = Scene::EXPANSION;
            g_exAtGyr = (float)atof(argv[i + 1]);
        }
        if (strcmp(argv[i], "--bhdisc") == 0)
            scene = Scene::BHDISC;
        if (strcmp(argv[i], "--bhdisc-at") == 0 && i + 1 < argc) {
            scene = Scene::BHDISC;
            g_bdAtSec = (float)atof(argv[i + 1]);
        }
        if (strcmp(argv[i], "--bhdisc-feed") == 0 && i + 1 < argc)
            g_bdScen = atoi(argv[i + 1]);
        if (strcmp(argv[i], "--redshift") == 0)
            g_redshift = true;
        if (strcmp(argv[i], "--cosmo-check") == 0)
            return cosmoCheck();
        if (strcmp(argv[i], "--eclipse") == 0)
            scene = Scene::ECLIPSE;
        if (strcmp(argv[i], "--eclipse-view") == 0 && i + 1 < argc) {
            scene = Scene::ECLIPSE;
            g_ecView = atoi(argv[i + 1]);
        }
        if (strcmp(argv[i], "--eclipse-apogee") == 0) {
            scene = Scene::ECLIPSE;
            g_ecApogee = true;
        }
        if (strcmp(argv[i], "--eclipse-season") == 0 && i + 1 < argc) {
            scene = Scene::ECLIPSE;
            g_ecSeason = atoi(argv[i + 1]);
        }
        if (strcmp(argv[i], "--eclipse-phase") == 0 && i + 1 < argc) {
            scene = Scene::ECLIPSE;
            g_ecPhase = (float)atof(argv[i + 1]);
        }
        if (strcmp(argv[i], "--nightsky") == 0)
            scene = Scene::NIGHTSKY;
        if (strcmp(argv[i], "--nightsky-from") == 0 && i + 1 < argc) {
            scene = Scene::NIGHTSKY;
            g_nsFrom = argv[i + 1];
        }
        if (strcmp(argv[i], "--nightsky-look") == 0 && i + 2 < argc) {
            scene = Scene::NIGHTSKY;
            g_nsLookSet = true;
            g_nsLook[0] = (float)atof(argv[i + 1]);
            g_nsLook[1] = (float)atof(argv[i + 2]);
        }
        if (strcmp(argv[i], "--nightsky-lat") == 0 && i + 1 < argc)
            g_nsLat = (float)atof(argv[i + 1]);
        if (strcmp(argv[i], "--nightsky-lst") == 0 && i + 1 < argc)
            g_nsLst = (float)atof(argv[i + 1]);
        if (strcmp(argv[i], "--nightsky-fov") == 0 && i + 1 < argc)
            g_nsFov = (float)atof(argv[i + 1]);
        if (strcmp(argv[i], "--nightsky-go") == 0 && i + 1 < argc)
            g_nsGo = argv[i + 1];
        if (strcmp(argv[i], "--nightsky-lines") == 0)
            g_nsLines = true;
        if (strcmp(argv[i], "--lagrange") == 0)
            scene = Scene::LAGRANGE;
        if (strcmp(argv[i], "--lg-jupiter") == 0) {
            scene = Scene::LAGRANGE;
            g_lgJupiter = true;
        }
        if (strcmp(argv[i], "--lg-demo") == 0) {
            scene = Scene::LAGRANGE;
            g_lgDemo = true;
        }
        if (strcmp(argv[i], "--td-miller") == 0)
            g_tdMiller = true;
        if (strcmp(argv[i], "--sandbox") == 0)
            scene = Scene::SANDBOX;
        if (strcmp(argv[i], "--sandbox-solar") == 0) {
            scene = Scene::SANDBOX;
            g_sandboxSolar = true;
        }
        if (strcmp(argv[i], "--sandbox-preset") == 0 && i + 1 < argc) {
            scene = Scene::SANDBOX;
            g_sandboxPreset = atoi(argv[i + 1]);
        }
        if (strcmp(argv[i], "--sandbox-demo") == 0) {
            scene = Scene::SANDBOX;
            g_sandboxDemo = true;
        }
    }
    return runApp(n, scene, tour);
}
