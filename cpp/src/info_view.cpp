#include "info_view.h"
#include "i18n.h"
#include "open_url.h"
#include "theme_manager.h"
#include "version.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"

using namespace VSTGUI;

namespace MonkSynth {

void InfoView::drawBody(CDrawContext *ctx, const CRect &bounds) {
    const char *font = i18n::uiFont();

    auto *titleFont = new CFontDesc(font, 24, kBoldFace);
    auto *bodyFont = new CFontDesc(font, 13);
    auto *smallFont = new CFontDesc(font, 11);
    auto *linkFont = new CFontDesc(font, 13, kUnderlineFace);
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

    // Creator
    ctx->setFont(bodyFont);
    ctx->setFontColor(CColor(190, 190, 190, 255));
    double y = bounds.top + 135;
    CRect creatorRect(bounds.left + 20, y, bounds.right - 20, y + 18);
    ctx->drawString(i18n::str(i18n::StringId::InfoCreatedBy), creatorRect, kCenterText);

    // License
    y += 36;
    ctx->setFontColor(CColor(200, 150, 50, 255));
    CRect licenseHeaderRect(bounds.left + 20, y, bounds.right - 20, y + 18);
    ctx->drawString(i18n::str(i18n::StringId::InfoLicenseHeader), licenseHeaderRect, kCenterText);

    y += 22;
    ctx->setFont(bodyFont);
    ctx->setFontColor(CColor(190, 190, 190, 255));
    CRect licenseRect(bounds.left + 20, y, bounds.right - 20, y + 18);
    ctx->drawString("MIT License \xC2\xA9 2026 Jonathan Taylor", licenseRect, kCenterText);

    // Source code section
    y += 36;
    ctx->setFontColor(CColor(200, 150, 50, 255));
    CRect srcHeaderRect(bounds.left + 20, y, bounds.right - 20, y + 18);
    ctx->drawString(i18n::str(i18n::StringId::InfoSourceCodeHeader), srcHeaderRect, kCenterText);

    y += 22;
    ctx->setFont(linkFont);
    ctx->setFontColor(CColor(130, 170, 255, 255));
    CRect linkRect(bounds.left + 20, y, bounds.right - 20, y + 18);
    ctx->drawString("github.com/JonET/monksynth", linkRect, kCenterText);
    githubLinkRect_ = linkRect;
    githubLinkRect_.offset(-bounds.left, -bounds.top);

    // Description
    y += 32;
    ctx->setFont(bodyFont);
    ctx->setFontColor(CColor(150, 150, 155, 255));
    const char *descLines[] = {
        i18n::str(i18n::StringId::InfoTagline1),
        i18n::str(i18n::StringId::InfoTagline2),
    };
    for (const char *line : descLines) {
        CRect lr(bounds.left + 20, y, bounds.right - 20, y + 18);
        ctx->drawString(line, lr, kCenterText);
        y += 19;
    }

    // ---- Contribute section ----
    double cy = bounds.top + 355;
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
    smallFont->forget();
    linkFont->forget();
    sectionFont->forget();
}

bool InfoView::hitLink(const CPoint &local, bool click) {
    if (githubLinkRect_.pointInside(local)) {
        if (click)
            openURL("https://github.com/JonET/monksynth");
        return true;
    }
    if (openFolderRect_.pointInside(local)) {
        if (click)
            openFolder(ThemeManager::getThemesDir());
        return true;
    }
    return false;
}

} // namespace MonkSynth
