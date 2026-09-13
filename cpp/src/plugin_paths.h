#pragma once

#include <filesystem>

namespace MonkSynth {

// Absolute path to this plugin bundle's Contents/Resources directory, located
// from the running module (works for the VST3 on every platform, and for the
// VST3 nested inside the AU component). Returns an empty path if it can't be
// determined or doesn't exist.
std::filesystem::path getPluginResourcesDir();

} // namespace MonkSynth
