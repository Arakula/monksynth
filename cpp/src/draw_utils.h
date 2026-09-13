#pragma once

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"

namespace MonkSynth {

// Fill a rounded rectangle. Shared by the overlay views.
inline void drawRoundRect(VSTGUI::CDrawContext *ctx, const VSTGUI::CRect &r,
                          VSTGUI::CCoord radius) {
    auto *path = ctx->createRoundRectGraphicsPath(r, radius);
    if (!path)
        return;
    ctx->drawGraphicsPath(path, VSTGUI::CDrawContext::kPathFilled);
    path->forget();
}

} // namespace MonkSynth
