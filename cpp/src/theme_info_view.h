#pragma once

#include "overlay_view.h"
#include "theme_manager.h"

namespace MonkSynth {

// Overlay shown from the right-click menu's "About Theme..." item. Displays
// the active theme's name, author, description and link, as read from its
// theme.json manifest.
class ThemeInfoView : public OverlayView {
  public:
    ThemeInfoView(const VSTGUI::CRect &size, ThemeManager::ThemeInfo info)
        : OverlayView(size), info_(std::move(info)) {}

  protected:
    void drawBody(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &bounds) override;
    bool hitLink(const VSTGUI::CPoint &local, bool click) override;

  private:
    ThemeManager::ThemeInfo info_;
    VSTGUI::CRect urlLinkRect_;
};

} // namespace MonkSynth
