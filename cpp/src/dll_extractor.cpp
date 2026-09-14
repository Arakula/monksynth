#include "dll_extractor.h"
#include "i18n.h"
#include "theme_manager.h"
#include "stb_image_write.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <vector>

#ifdef _WIN32
#include <io.h>
#endif

namespace MonkSynth {
namespace fs = std::filesystem;

// On Windows, fopen(path.string().c_str(), ...) silently mangles paths that
// contain characters outside the active ANSI code page (e.g. Japanese on a
// non-Japanese locale).  Use the wide-character variant instead.
static FILE *platform_fopen(const fs::path &p, const char *mode) {
#ifdef _WIN32
    wchar_t wMode[8] = {};
    for (int i = 0; mode[i] && i < 7; i++)
        wMode[i] = static_cast<wchar_t>(mode[i]);
    return _wfopen(p.wstring().c_str(), wMode);
#else
    return fopen(p.string().c_str(), mode);
#endif
}

// --- CRC32 (standard polynomial 0xEDB88320) ---

static uint32_t crc32_table[256];
static bool crc32_ready = false;

static void init_crc32() {
    if (crc32_ready)
        return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c >> 1) ^ (c & 1 ? 0xEDB88320u : 0);
        crc32_table[i] = c;
    }
    crc32_ready = true;
}

static uint32_t compute_crc32(const uint8_t *data, size_t len) {
    init_crc32();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFF;
}

// --- Known DLL identity ---

static const size_t kExpectedDllSize = 3719168;
static const uint32_t kExpectedDllCrc = 0x9B0E96C2;

// --- Resource table ---
// Each entry is a BITMAPINFOHEADER at a known file offset in the original DLL.
// There is only one known version of the Delay Lama DLL.

enum class TranspMode { Opaque, KeyWhite, KeyTopLeft };

struct ResourceEntry {
    size_t fileOffset;  // offset of BITMAPINFOHEADER in the DLL file
    const char *filename;
    int width;
    int height;
    int bpp;
    TranspMode transparency;
};

static const ResourceEntry kResources[] = {
    {0x00E1B0, "background.png", 360, 510, 8, TranspMode::Opaque},
    {0x03B310, "monk-strip.png", 1570, 1866, 8, TranspMode::Opaque},
    {0x3079A8, "fader-right-sm.png", 10, 10, 24, TranspMode::KeyWhite},
    {0x307B18, "fader-down-sm.png", 10, 10, 24, TranspMode::KeyWhite},
    {0x307C88, "fader-down-large.png", 20, 17, 24, TranspMode::KeyWhite},
    {0x3080B0, "knob-left.png", 50, 3000, 8, TranspMode::KeyTopLeft},
    {0x32E640, "knob-right.png", 50, 3000, 8, TranspMode::KeyTopLeft},
    {0x354BD0, "info.png", 253, 275, 24, TranspMode::Opaque},
};
static const int kNumResources = sizeof(kResources) / sizeof(kResources[0]);

// --- DIB to RGBA conversion ---

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

// |dibSize| is the number of bytes available from |dib| to the end of the
// file. The size and CRC checks in extractClassicTheme make a mismatch
// unlikely, but CRC32 is trivial to forge, so every offset derived from the
// header is checked against the buffer before it is read.
static std::vector<uint8_t> dib_to_rgba(const uint8_t *dib, size_t dibSize, int width, int height,
                                        int bpp) {
    // BITMAPINFOHEADER is 40 bytes
    const size_t headerSize = 40;
    if (width <= 0 || height <= 0 || dibSize < headerSize)
        return {};

    // Palette. biClrUsed comes from the file; the known resources leave it
    // at 0 (meaning 2^bpp entries). Anything larger than that is bogus.
    size_t paletteEntries = 0;
    if (bpp <= 8) {
        uint32_t clrUsed = rd32(dib + 32);
        const uint32_t maxEntries = 1u << bpp;
        if (clrUsed > maxEntries)
            return {};
        paletteEntries = clrUsed != 0 ? clrUsed : maxEntries;
    }
    const uint8_t *palette = dib + headerSize;
    const uint8_t *pixels = palette + paletteEntries * 4;

    // Row stride padded to 4-byte boundary
    const size_t rowBytes = ((static_cast<size_t>(width) * bpp + 31) / 32) * 4;
    if (headerSize + paletteEntries * 4 + rowBytes * static_cast<size_t>(height) > dibSize)
        return {};

    // BMP height field can be negative (top-down), but our known resources are positive (bottom-up)
    int32_t rawH = static_cast<int32_t>(rd32(dib + 8));
    bool bottomUp = (rawH > 0);

    std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4);

    for (int y = 0; y < height; y++) {
        int srcRow = bottomUp ? (height - 1 - y) : y;
        const uint8_t *row = pixels + srcRow * rowBytes;
        uint8_t *dst = rgba.data() + static_cast<size_t>(y) * width * 4;

        for (int x = 0; x < width; x++) {
            uint8_t r, g, b;
            if (bpp == 24) {
                b = row[x * 3 + 0];
                g = row[x * 3 + 1];
                r = row[x * 3 + 2];
            } else if (bpp == 8) {
                size_t idx = row[x];
                if (idx < paletteEntries) {
                    b = palette[idx * 4 + 0];
                    g = palette[idx * 4 + 1];
                    r = palette[idx * 4 + 2];
                } else {
                    r = g = b = 0;
                }
            } else {
                r = g = b = 128;
            }
            dst[x * 4 + 0] = r;
            dst[x * 4 + 1] = g;
            dst[x * 4 + 2] = b;
            dst[x * 4 + 3] = 255;
        }
    }

    return rgba;
}

static void apply_transparency(std::vector<uint8_t> &rgba, int w, int h, TranspMode mode) {
    if (mode == TranspMode::Opaque)
        return;

    uint8_t kr, kg, kb;
    if (mode == TranspMode::KeyWhite) {
        kr = kg = kb = 255;
    } else {
        kr = rgba[0];
        kg = rgba[1];
        kb = rgba[2];
    }

    for (int i = 0; i < w * h; i++) {
        if (rgba[i * 4] == kr && rgba[i * 4 + 1] == kg && rgba[i * 4 + 2] == kb) {
            rgba[i * 4 + 0] = 0;
            rgba[i * 4 + 1] = 0;
            rgba[i * 4 + 2] = 0;
            rgba[i * 4 + 3] = 0;
        }
    }
}

// --- Main extraction ---

ExtractionResult extractClassicTheme(const fs::path &dllPath, const fs::path &configDir) {
    // Read entire DLL
    FILE *f = platform_fopen(dllPath, "rb");
    if (!f) {
        // Include the OS reason: "Operation not permitted" points at macOS
        // folder privacy (TCC) blocking the AU host process, which is the
        // usual cause in Logic/GarageBand for files in Downloads (#27).
        int err = errno;
        std::string msg = std::string(i18n::str(i18n::StringId::ErrCannotOpen)) + " (" +
                          std::strerror(err) + ").";
        if (err == EPERM || err == EACCES)
            msg += std::string("\n") + i18n::str(i18n::StringId::ErrCannotOpenHint);
        return {false, msg, {}};
    }

    fseek(f, 0, SEEK_END);
    size_t fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fileSize != kExpectedDllSize) {
        fclose(f);
        return {false, i18n::str(i18n::StringId::ErrUnexpectedSize), {}};
    }

    std::vector<uint8_t> dll(fileSize);
    if (fread(dll.data(), 1, fileSize, f) != fileSize) {
        fclose(f);
        return {false, i18n::str(i18n::StringId::ErrReadFailed), {}};
    }
    fclose(f);

    // Validate DLL checksum
    uint32_t crc = compute_crc32(dll.data(), dll.size());
    if (crc != kExpectedDllCrc)
        return {false, i18n::str(i18n::StringId::ErrChecksumMismatch), {}};

    // Create output directory
    fs::path themeDir = configDir / "themes" / "classic";
    std::error_code ec;
    fs::create_directories(themeDir, ec);
    if (ec)
        return {false, std::string(i18n::str(i18n::StringId::ErrCreateDir)) + ec.message(), {}};

    int extracted = 0;

    for (int i = 0; i < kNumResources; i++) {
        const auto &res = kResources[i];

        if (res.fileOffset + 40 > fileSize)
            continue;

        const uint8_t *dib = dll.data() + res.fileOffset;

        // Sanity check: BITMAPINFOHEADER.biSize should be 40
        if (rd32(dib) != 40)
            continue;

        auto rgba = dib_to_rgba(dib, fileSize - res.fileOffset, res.width, res.height, res.bpp);
        if (rgba.empty())
            continue;

        apply_transparency(rgba, res.width, res.height, res.transparency);

        fs::path outPath = themeDir / res.filename;
        if (!stbi_write_png(outPath.u8string().c_str(), res.width, res.height, 4, rgba.data(),
                            res.width * 4))
            continue;

        extracted++;
    }

    // Write theme manifest
    {
        fs::path manifest = themeDir / "theme.json";
        FILE *mf = platform_fopen(manifest, "w");
        if (mf) {
            fprintf(mf,
                    "{\n"
                    "  \"name\": \"%s\",\n"
                    "  \"author\": \"%s\",\n"
                    "  \"version\": \"1.0\",\n"
                    "  \"description\": \"%s\",\n"
                    "  \"url\": \"%s\"\n"
                    "}\n",
                    ThemeManager::kClassicThemeName, ThemeManager::kClassicThemeAuthor,
                    ThemeManager::kClassicThemeDescription, ThemeManager::kClassicThemeUrl);
            fclose(mf);
        }
    }

    if (extracted == kNumResources)
        return {true, {}, themeDir};
    if (extracted > 0) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), i18n::str(i18n::StringId::MsgImportedCount), extracted,
                      kNumResources);
        return {true, buf, themeDir};
    }

    fs::remove_all(themeDir, ec);
    return {false, i18n::str(i18n::StringId::ErrExtractFailed), {}};
}

} // namespace MonkSynth
