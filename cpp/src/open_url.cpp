#include "open_url.h"

#include <system_error>

#if _WIN32
#include <windows.h>
#include <shellapi.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace MonkSynth {

#if !_WIN32
// Launch |argv| detached from the host process without going through a
// shell, so arguments are never interpreted (theme.json URLs are untrusted
// input). Double-forks so the launcher is reaped immediately and the opened
// program is never left as a zombie of the DAW.
static void spawnDetached(const char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0)
        return;
    if (pid == 0) {
        pid_t inner = fork();
        if (inner == 0) {
            execvp(argv[0], const_cast<char *const *>(argv));
            _exit(127);
        }
        _exit(inner < 0 ? 1 : 0);
    }
    int status = 0;
    waitpid(pid, &status, 0);
}
#endif

static bool isWebUrl(const std::string &url) {
    return url.rfind("https://", 0) == 0 || url.rfind("http://", 0) == 0;
}

void openURL(const std::string &url) {
    // Only ever hand http(s) links to the OS. Anything else (file paths,
    // custom schemes, shell metacharacters) is dropped.
    if (!isWebUrl(url))
        return;

#if _WIN32
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif __APPLE__
    const char *argv[] = {"open", url.c_str(), nullptr};
    spawnDetached(argv);
#else
    const char *argv[] = {"xdg-open", url.c_str(), nullptr};
    spawnDetached(argv);
#endif
}

void openFolder(const std::filesystem::path &path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    // Proceed even if create_directories failed — the folder may already exist
    // and succeeded in a prior run, or a parent permission may block creation.

#if _WIN32
    // ShellExecuteW correctly handles non-ASCII paths (e.g. Japanese user
    // folder names) which the ANSI variant would mangle.
    ShellExecuteW(nullptr, L"open", path.wstring().c_str(), nullptr, nullptr,
                  SW_SHOWNORMAL);
#elif __APPLE__
    std::string p = path.string();
    const char *argv[] = {"open", p.c_str(), nullptr};
    spawnDetached(argv);
#else
    std::string p = path.string();
    const char *argv[] = {"xdg-open", p.c_str(), nullptr};
    spawnDetached(argv);
#endif
}

} // namespace MonkSynth
