#include "plugin_paths.h"

#include <system_error>

#if _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace MonkSynth {
namespace fs = std::filesystem;

// Path of the shared library / bundle executable containing this code.
static fs::path currentModulePath() {
#if _WIN32
    HMODULE mod = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&currentModulePath), &mod))
        return {};
    wchar_t buf[MAX_PATH * 4];
    DWORD len = GetModuleFileNameW(mod, buf, static_cast<DWORD>(sizeof(buf) / sizeof(buf[0])));
    if (len == 0 || len >= sizeof(buf) / sizeof(buf[0]))
        return {};
    return fs::path(buf);
#else
    Dl_info info{};
    if (!dladdr(reinterpret_cast<void *>(&currentModulePath), &info) || !info.dli_fname)
        return {};
    return fs::path(info.dli_fname);
#endif
}

static fs::path locateResourcesDir() {
    // VST3 bundle layout on every platform:
    //   MonkSynth.vst3/Contents/<arch>/MonkSynth[.vst3|.so]   (macOS: Contents/MacOS/MonkSynth)
    //   MonkSynth.vst3/Contents/Resources/
    fs::path mod = currentModulePath();
    if (mod.empty())
        return {};

    std::error_code ec;
    fs::path resources = mod.parent_path().parent_path() / "Resources";
    if (fs::is_directory(resources, ec))
        return resources;
    return {};
}

fs::path getPluginResourcesDir() {
    // The module never moves while loaded, so resolve once.
    static const fs::path cached = locateResourcesDir();
    return cached;
}

} // namespace MonkSynth
