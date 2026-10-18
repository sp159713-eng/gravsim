#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO_WIDE
#include "../third_party/stb_image_write.h"
#include "capture_io.h"
#include <windows.h>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <filesystem>
#include <mutex>
#include <thread>

namespace {

std::mutex g_mu;
std::condition_variable g_cv;
std::deque<CapJob> g_q;
std::thread g_th;
bool g_stop = false;
bool g_busy = false;
std::string g_toast;
double g_toastT = -100.0;
int g_lastIdx = -1;
std::string g_lastPath;

std::string upper(std::string s) {
    for (char& c : s) {
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        if (c == '_') c = '-';
        if (c == '\\') c = '/';
    }
    return s;
}

bool writePng(const CapJob& j, int level) {
    std::vector<uint8_t> rgb((size_t)j.w * j.h * 3);
    const uint8_t* s = j.px.data();
    uint8_t* d = rgb.data();
    size_t n = (size_t)j.w * j.h;
    int r = j.bgr ? 2 : 0, b = j.bgr ? 0 : 2;
    for (size_t i = 0; i < n; i++, s += 4, d += 3) {
        d[0] = s[r];
        d[1] = s[1];
        d[2] = s[b];
    }
    stbi_write_png_compression_level = level;
    return stbi_write_png(j.path.c_str(), j.w, j.h, 3, rgb.data(), j.w * 3) != 0;
}

void finishRecording(const CapJob& j) {
    namespace fs = std::filesystem;
    char found[MAX_PATH];
    bool have = SearchPathA(nullptr, "ffmpeg.exe", nullptr, MAX_PATH, found, nullptr) != 0;
    if (!have) {
        capToastSet("FRAMES SAVED - INSTALL FFMPEG FOR MP4");
        return;
    }
    std::string dirWin = j.dir;
    for (char& c : dirWin)
        if (c == '/') c = '\\';
    std::string mp4Win = j.mp4;
    for (char& c : mp4Win)
        if (c == '/') c = '\\';
    std::string cmd = "cmd.exe /c \"\"" + std::string(found) + "\" -y -loglevel error -framerate 30 -i \"" + dirWin +
                      "\\frame_%05d.png\" -vf \"pad=ceil(iw/2)*2:ceil(ih/2)*2\" -c:v libx264 -pix_fmt yuv420p \"" + mp4Win +
                      "\" && rmdir /s /q \"" + dirWin + "\"\"";
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<char> buf(cmd.begin(), cmd.end());
    buf.push_back('\0');
    if (CreateProcessA(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        capToastSet("SAVED " + upper(j.mp4));
    } else {
        capToastSet("FRAMES SAVED - FFMPEG FAILED TO START");
    }
}

void handle(CapJob& j) {
    namespace fs = std::filesystem;
    if (j.kind == 0) {
        bool ok = writePng(j, 6);
        capToastSet(ok ? "SAVED " + upper(j.path) : "SCREENSHOT WRITE FAILED");
    } else if (j.kind == 1) {
        std::error_code ec;
        if (g_lastIdx >= 0 && j.index > g_lastIdx + 1) {
            char name[64];
            for (int k = g_lastIdx + 1; k < j.index; k++) {
                std::snprintf(name, sizeof(name), "frame_%05d.png", k);
                fs::copy_file(g_lastPath, fs::path(j.dir) / name, fs::copy_options::overwrite_existing, ec);
            }
        }
        writePng(j, 1);
        g_lastIdx = j.index;
        g_lastPath = j.path;
    } else if (j.kind == 2) {
        g_lastIdx = -1;
        g_lastPath.clear();
    } else if (j.kind == 3) {
        finishRecording(j);
        g_lastIdx = -1;
        g_lastPath.clear();
    }
}

void run() {
    for (;;) {
        CapJob j;
        {
            std::unique_lock<std::mutex> lk(g_mu);
            g_busy = false;
            g_cv.wait(lk, [] { return g_stop || !g_q.empty(); });
            if (g_q.empty()) return;
            j = std::move(g_q.front());
            g_q.pop_front();
            g_busy = true;
        }
        handle(j);
    }
}

}

double capClock() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

std::string capTimestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    char b[32];
    std::strftime(b, sizeof(b), "%Y%m%d_%H%M%S", &tm);
    return b;
}

std::string capPicturesDir() {
    const char* up = std::getenv("USERPROFILE");
    std::string base = up ? up : ".";
    for (char& c : base)
        if (c == '\\') c = '/';
    std::string dir = base + "/Pictures/gravsim";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

bool capFfmpegAvailable() {
    char found[MAX_PATH];
    return SearchPathA(nullptr, "ffmpeg.exe", nullptr, MAX_PATH, found, nullptr) != 0;
}

void capToastSet(const std::string& text) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_toast = text;
    g_toastT = capClock();
}

bool capToastGet(std::string& text, double& age) {
    std::lock_guard<std::mutex> lk(g_mu);
    age = capClock() - g_toastT;
    text = g_toast;
    return age < 4.0 && !g_toast.empty();
}

void capIoStart() {
    g_stop = false;
    g_th = std::thread(run);
}

void capIoPush(CapJob&& job) {
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_q.push_back(std::move(job));
    }
    g_cv.notify_one();
}

size_t capIoBacklog() {
    std::lock_guard<std::mutex> lk(g_mu);
    return g_q.size() + (g_busy ? 1 : 0);
}

void capIoStop() {
    {
        std::lock_guard<std::mutex> lk(g_mu);
        g_stop = true;
    }
    g_cv.notify_all();
    if (g_th.joinable()) g_th.join();
}
