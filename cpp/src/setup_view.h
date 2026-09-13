#pragma once

#include "theme_manager.h"

#include "vstgui/lib/cviewcontainer.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace MonkSynth {

// First-run setup overlay. Drawn with VSTGUI primitives only (no bitmap assets).
// Prompts the user to import the classic theme from the original Delay Lama DLL.
class SetupView : public VSTGUI::CViewContainer {
  public:
    SetupView(const VSTGUI::CRect &size);

    using ImportCallback = std::function<void()>;
    using ThemeCallback = std::function<void(const std::filesystem::path &)>;

    void setImportCallback(ImportCallback cb) { importCb_ = std::move(cb); }

    // Themes shipped inside the plugin bundle. If non-empty, the first one is
    // offered as a one-click alternative to importing the classic theme.
    void setBuiltInThemes(std::vector<ThemeManager::InstalledTheme> themes);
    void setBuiltInThemeCallback(ThemeCallback cb) { builtInCb_ = std::move(cb); }

    void setStatusText(const std::string &text);

    void drawBackgroundRect(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &rect) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint &where,
                                          const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint &where,
                                           const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint &where,
                                            const VSTGUI::CButtonState &buttons) override;

  private:
    VSTGUI::CRect importBtnRect_;
    VSTGUI::CRect urlLinkRect_;
    VSTGUI::CRect openFolderRect_;
    VSTGUI::CRect builtInLinkRect_;
    std::string statusText_;
    std::vector<ThemeManager::InstalledTheme> builtInThemes_;
    ImportCallback importCb_;
    ThemeCallback builtInCb_;
};

} // namespace MonkSynth
