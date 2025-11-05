#pragma once

#include <string>
#include <fstream>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace hyprbars {

static inline std::string get_log_path() {
    const char* xdg = std::getenv("XDG_CACHE_HOME");
    std::string dir;
    if (xdg && xdg[0])
        dir = std::string(xdg) + "/hyprbars";
    else {
        const char* home = std::getenv("HOME");
        if (!home)
            dir = "/tmp/hyprbars";
        else
            dir = std::string(home) + "/.cache/hyprbars";
    }
    std::filesystem::create_directories(dir);
    return dir + "/hyprbars.log";
}

static inline void log(const std::string& msg) {
    try {
        const auto path = get_log_path();
        std::ofstream f(path, std::ios::app);
        if (!f)
            return;
        const auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        char tbuf[64];
        std::strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        f << "[" << tbuf << "] " << msg << std::endl;
    } catch (...) {
        // best-effort logging only
    }
}

static inline void logException(const std::string& where, const std::exception& e) {
    log(where + " exception: " + e.what());
}

static inline void logUnknown(const std::string& where) {
    log(where + " unknown exception");
}

// Very small POSIX-safe logger that uses low-level file I/O to ensure we
// get writes on-disk even if C++ runtime or TLS destructors misbehave during crashes.
static inline void lowlevel_log(const char* msg) {
    try {
        const auto path = get_log_path();
        // open with O_APPEND so multiple writers are safe
        int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd < 0)
            return;

        // prefix timestamp
        const auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        char tbuf[64];
        std::strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));

        std::string out = std::string("[") + tbuf + "] " + msg + "\n";
        ssize_t w = write(fd, out.c_str(), out.size());
        (void)w;
        fsync(fd);
        close(fd);
    } catch (...) {
        // best-effort only
    }
}

} // namespace hyprbars
