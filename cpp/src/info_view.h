#pragma once

#include "overlay_view.h"

namespace MonkSynth {

// Info overlay shown when the user clicks the "?" button.
// Displays project info, license, creator credit, and links.
class InfoView : public OverlayView {
  public:
    explicit InfoView(const VSTGUI::CRect &size) : OverlayView(size) {}

  protected:
    void drawBody(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &bounds) override;
    bool hitLink(const VSTGUI::CPoint &local, bool click) override;

  private:
    VSTGUI::CRect githubLinkRect_;
    VSTGUI::CRect openFolderRect_;
};

} // namespace MonkSynth
