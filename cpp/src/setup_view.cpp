#include "setup_view.h"
#include "i18n.h"
#include "draw_utils.h"
#include "open_url.h"
#include "theme_manager.h"
#include "version.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/cstring.h"

using namespace VSTGUI;

namespace MonkSynth {

SetupView::SetupView(const CRect &size) : CViewContainer(size) {
    setTransparency(false);

    // Import button position (relative to view)
    double bw = 220, bh = 36;
    double bx = (size.getWidth() - bw) / 2;
    double by = 308;
    importBtnRect_ = CRect(bx, by, bx + bw, by + bh);
}

void SetupView::setBuiltInThemes(std::vector<ThemeManager::InstalledTheme> themes) {
    builtInThemes_ = std::move(themes);
    setDirty(true);
}

void SetupView::setStatusText(const std::string &text) {
    statusText_ = text;
    setDirty(true);
}

// Shorten |text| with a trailing ellipsis until it fits |maxWidth| in the
// context's current font. Cuts at UTF-8 character boundaries.
static std::string ellipsize(CDrawContext *ctx, std::string text, CCoord maxWidth) {
    static const char *kEllipsis = "\xE2\x80\xA6";
    if (ctx->getStringWidth(text.c_str()) <= maxWidth)
        return text;
    while (!text.empty()) {
        // Drop one UTF-8 character from the end.
        size_t i = text.size() - 1;
        while (i > 0 && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80)
            i--;
        text.erase(i);
        std::string candidate = text + kEllipsis;
        if (ctx->getStringWidth(candidate.c_str()) <= maxWidth)
            return candidate;
    }
    return kEllipsis;
}

void SetupView::drawBackgroundRect(CDrawContext *ctx, const CRect & /*rect*/) {
    CRect bounds = getViewSize();
    const char *font = i18n::uiFont();

    // Enable anti-aliasing for sharp rendering on HiDPI
    ctx->setDrawMode(kAntiAliasing | kNonIntegralMode);

    // Dark background
    ctx->setFillColor(CColor(30, 30, 35, 255));
    ctx->drawRect(bounds, kDrawFilled);

    // Accent line
    CRect accent(bounds.left + 30, bounds.top + 40, bounds.right - 30, bounds.top + 42);
    ctx->setFillColor(CColor(200, 150, 50, 255));
    ctx->drawRect(accent, kDrawFilled);

    auto *titleFont = new CFontDesc(font, 24, kBoldFace);
    auto *bodyFont = new CFontDesc(font, 13);
    auto *linkFont = new CFontDesc(font, 13, kUnderlineFace);
    auto *smallFont = new CFontDesc(font, 11);
    auto *btnFont = new CFontDesc(font, 14, kBoldFace);
    auto *sectionFont = new CFontDesc(font, 14, kBoldFace);

    // Title
    ctx->setFont(titleFont);
    ctx->setFontColor(CColor(230, 230, 230, 255));
    CRect titleRect(bounds.left, bounds.top + 55, bounds.right, bounds.top + 85);
    ctx->drawString("MonkSynth", titleRect, kCenterText);

    // Version
    ctx->setFont(smallFont);
    ctx->setFontColor(CColor(140, 140, 140, 255));
    CRect verRect(bounds.left, bounds.top + 90, bounds.right, bounds.top + 108);
    ctx->drawString(MONKSYNTH_VERSION " " MONKSYNTH_VERSION_LABEL, verRect, kCenterText);

    // Body text
    ctx->setFont(bodyFont);
    ctx->setFontColor(CColor(190, 190, 190, 255));

    const char *lines[] = {
        i18n::str(i18n::StringId::SetupNeedsTheme),
        "",
        i18n::str(i18n::StringId::SetupImportFromClassic1),
        i18n::str(i18n::StringId::SetupImportFromClassic2),
        "",
        i18n::str(i18n::StringId::SetupDownloadFrom),
    };
    double lineY = bounds.top + 130;
    for (const char *line : lines) {
        CRect lr(bounds.left + 20, lineY, bounds.right - 20, lineY + 18);
        ctx->drawString(line, lr, kCenterText);
        lineY += 19;
    }

    // Clickable URL link
    ctx->setFont(linkFont);
    ctx->setFontColor(CColor(130, 170, 255, 255));
    CRect linkRect(bounds.left + 20, lineY, bounds.right - 20, lineY + 18);
    ctx->drawString("www.audionerdz.nl/download.htm", linkRect, kCenterText);
    urlLinkRect_ = linkRect;
    urlLinkRect_.offset(-bounds.left, -bounds.top);
    lineY += 19;

    // Remaining body text
    ctx->setFont(bodyFont);
    ctx->setFontColor(CColor(190, 190, 190, 255));
    const char *lines2[] = {
        i18n::str(i18n::StringId::SetupThenClick1),
        i18n::str(i18n::StringId::SetupThenClick2),
    };
    for (const char *line : lines2) {
        CRect lr(bounds.left + 20, lineY, bounds.right - 20, lineY + 18);
        ctx->drawString(line, lr, kCenterText);
        lineY += 19;
    }

    // Import button (rounded rect)
    CRect btn = importBtnRect_;
    btn.offset(bounds.left, bounds.top);
    ctx->setFillColor(CColor(200, 150, 50, 255));
    drawRoundRect(ctx, btn, 6);

    ctx->setFont(btnFont);
    ctx->setFontColor(CColor(30, 30, 35, 255));
    ctx->drawString(i18n::str(i18n::StringId::SetupImportButton), btn, kCenterText);

    // Status text (single line; squeezed into the gap above the contribute
    // section so the two don't overlap).
    if (!statusText_.empty()) {
        ctx->setFont(smallFont);
        ctx->setFontColor(CColor(220, 180, 100, 255));
        CRect statusRect(bounds.left + 20, btn.bottom + 5, bounds.right - 20, btn.bottom + 20);
        ctx->drawString(statusText_.c_str(), statusRect, kCenterText);
    }

    // Built-in theme shortcut: "Or use the built-in theme: <name>" on one
    // line, with only the name drawn as a link. Deliberately low-key so the
    // classic import above stays the obvious path.
    builtInLinkRect_ = CRect();
    if (!builtInThemes_.empty()) {
        const std::string prefix = i18n::str(i18n::StringId::SetupOrBuiltIn);
        const CCoord usable = bounds.getWidth() - 40;

        ctx->setFont(bodyFont);
        CCoord wPrefix = ctx->getStringWidth(prefix.c_str());
        // Theme names are unbounded community input: keep the whole line
        // inside the view.
        ctx->setFont(linkFont);
        std::string name = ellipsize(ctx, builtInThemes_.front().name, usable - wPrefix);
        CCoord wName = ctx->getStringWidth(name.c_str());

        double ly = btn.bottom + 30;
        double x0 = bounds.left + (bounds.getWidth() - (wPrefix + wName)) / 2;

        ctx->setFont(bodyFont);
        ctx->setFontColor(CColor(150, 150, 155, 255));
        CRect prefixRect(x0, ly, x0 + wPrefix, ly + 18);
        ctx->drawString(prefix.c_str(), prefixRect, kLeftText);

        ctx->setFont(linkFont);
        ctx->setFontColor(CColor(130, 170, 255, 255));
        CRect nameRect(x0 + wPrefix, ly, x0 + wPrefix + wName, ly + 18);
        ctx->drawString(name.c_str(), nameRect, kLeftText);
        builtInLinkRect_ = nameRect;
        builtInLinkRect_.offset(-bounds.left, -bounds.top);
    }

    // ---- Contribute section ----
    double cy = bounds.top + 400;
    CRect sepRect(bounds.left + 30, cy, bounds.right - 30, cy + 1);
    ctx->setFillColor(CColor(200, 150, 50, 120));
    ctx->drawRect(sepRect, kDrawFilled);
    cy += 8;

    ctx->setFont(sectionFont);
    ctx->setFontColor(CColor(200, 150, 50, 255));
    CRect headerRect(bounds.left + 20, cy, bounds.right - 20, cy + 20);
    ctx->drawString(i18n::str(i18n::StringId::ContributeHeader), headerRect, kCenterText);
    cy += 22;

    ctx->setFont(bodyFont);
    ctx->setFontColor(CColor(190, 190, 190, 255));
    CRect shareRect(bounds.left + 20, cy, bounds.right - 20, cy + 18);
    ctx->drawString(i18n::str(i18n::StringId::ContributeShare), shareRect, kCenterText);
    cy += 18;

    ctx->setFontColor(CColor(150, 150, 155, 255));
    CRect lookingRect(bounds.left + 20, cy, bounds.right - 20, cy + 18);
    ctx->drawString(i18n::str(i18n::StringId::ContributeLookingFor), lookingRect, kCenterText);
    cy += 20;

    ctx->setFont(linkFont);
    ctx->setFontColor(CColor(130, 170, 255, 255));
    CRect folderRect(bounds.left + 20, cy, bounds.right - 20, cy + 18);
    ctx->drawString(i18n::str(i18n::StringId::ContributeOpenFolder), folderRect, kCenterText);
    openFolderRect_ = folderRect;
    openFolderRect_.offset(-bounds.left, -bounds.top);

    titleFont->forget();
    bodyFont->forget();
    linkFont->forget();
    smallFont->forget();
    btnFont->forget();
    sectionFont->forget();
}

CMouseEventResult SetupView::onMouseDown(CPoint &where, const CButtonState &buttons) {
    if (!(buttons & kLButton))
        return kMouseEventNotHandled;

    CRect bounds = getViewSize();
    CPoint local = where;
    local.offset(-bounds.left, -bounds.top);

    if (importBtnRect_.pointInside(local)) {
        if (importCb_)
            importCb_();
        return kMouseEventHandled;
    }

    if (urlLinkRect_.pointInside(local)) {
        openURL("http://www.audionerdz.nl/download.htm");
        return kMouseEventHandled;
    }

    if (openFolderRect_.pointInside(local)) {
        openFolder(ThemeManager::getThemesDir());
        return kMouseEventHandled;
    }

    if (!builtInThemes_.empty() && builtInLinkRect_.pointInside(local)) {
        if (builtInCb_)
            builtInCb_(builtInThemes_.front().path);
        return kMouseEventHandled;
    }

    return kMouseEventNotHandled;
}

CMouseEventResult SetupView::onMouseMoved(CPoint &where, const CButtonState & /*buttons*/) {
    CRect bounds = getViewSize();
    CPoint local = where;
    local.offset(-bounds.left, -bounds.top);

    auto *frame = getFrame();
    if (frame) {
        if (importBtnRect_.pointInside(local) || urlLinkRect_.pointInside(local) ||
            openFolderRect_.pointInside(local) ||
            (!builtInThemes_.empty() && builtInLinkRect_.pointInside(local)))
            frame->setCursor(kCursorHand);
        else
            frame->setCursor(kCursorDefault);
    }
    return kMouseEventHandled;
}

CMouseEventResult SetupView::onMouseExited(CPoint & /*where*/, const CButtonState & /*buttons*/) {
    auto *frame = getFrame();
    if (frame)
        frame->setCursor(kCursorDefault);
    return kMouseEventHandled;
}

} // namespace MonkSynth
