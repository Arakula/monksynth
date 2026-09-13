#include "theme_info_view.h"
#include "i18n.h"
#include "open_url.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cfont.h"

#include <string>
#include <vector>

using namespace VSTGUI;

namespace MonkSynth {

// Length in bytes of the UTF-8 sequence starting at s[i]. A stray
// continuation byte counts as 1 so malformed input still advances.
static size_t utf8SeqLen(const std::string &s, size_t i) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80 || (c >> 6) == 0x2)
        return 1;
    if ((c >> 5) == 0x6)
        return 2;
    if ((c >> 4) == 0xE)
        return 3;
    return 4;
}

// Greedy word wrap using the context's current font. Words wider than
// |maxWidth| on their own are broken at UTF-8 character boundaries so text
// without spaces (CJK, long URLs) still wraps instead of overflowing.
static std::vector<std::string> wrapText(CDrawContext *ctx, const std::string &text,
                                         CCoord maxWidth) {
    std::vector<std::string> lines;
    std::string line;

    auto fits = [&](const std::string &s) { return ctx->getStringWidth(s.c_str()) <= maxWidth; };

    size_t pos = 0;
    while (pos < text.size()) {
        size_t space = text.find(' ', pos);
        std::string word = text.substr(pos, space == std::string::npos ? std::string::npos
                                                                        : space - pos);
        pos = (space == std::string::npos) ? text.size() : space + 1;
        if (word.empty())
            continue;

        std::string candidate = line.empty() ? word : line + " " + word;
        if (fits(candidate)) {
            line = candidate;
            continue;
        }
        if (!line.empty()) {
            lines.push_back(line);
            line.clear();
        }
        if (fits(word)) {
            line = word;
            continue;
        }
        // Word alone is too wide: break it character by character.
        for (size_t i = 0; i < word.size();) {
            size_t n = utf8SeqLen(word, i);
            std::string ch = word.substr(i, n);
            if (!line.empty() && !fits(line + ch)) {
                lines.push_back(line);
                line.clear();
            }
            line += ch;
            i += n;
        }
    }
    if (!line.empty())
        lines.push_back(line);
    return lines;
}

// Trim the scheme and "www." for a friendlier link label.
static std::string displayUrl(const std::string &url) {
    std::string s = url;
    for (const char *prefix : {"https://", "http://"}) {
        if (s.rfind(prefix, 0) == 0) {
            s = s.substr(std::string(prefix).size());
            break;
        }
    }
    if (s.rfind("www.", 0) == 0)
        s = s.substr(4);
    return s;
}

void ThemeInfoView::drawBody(CDrawContext *ctx, const CRect &bounds) {
    const char *font = i18n::uiFont();
    const CCoord textWidth = bounds.getWidth() - 40;

    auto *titleFont = new CFontDesc(font, 20, kBoldFace);
    auto *bodyFont = new CFontDesc(font, 13);
    auto *smallFont = new CFontDesc(font, 11);
    auto *linkFont = new CFontDesc(font, 13, kUnderlineFace);

    // Everything must stay above the Close button. theme.json text is
    // unbounded community input, so long descriptions are cut rather than
    // drawn over the button.
    const double bottomLimit = bounds.top + closeButtonTop() - 10;
    const double linkHeight = 18;
    const double linkGap = 14;

    double y = bounds.top + 55;

    // Theme name
    ctx->setFont(titleFont);
    ctx->setFontColor(CColor(230, 230, 230, 255));
    for (const auto &line : wrapText(ctx, info_.name, textWidth)) {
        if (y + 28 > bottomLimit)
            break;
        CRect r(bounds.left + 20, y, bounds.right - 20, y + 28);
        ctx->drawString(line.c_str(), r, kCenterText);
        y += 28;
    }

    // Author
    if (!info_.author.empty()) {
        ctx->setFont(bodyFont);
        ctx->setFontColor(CColor(190, 190, 190, 255));
        std::string by = std::string(i18n::str(i18n::StringId::ThemeInfoAuthorPrefix)) +
                         info_.author;
        for (const auto &line : wrapText(ctx, by, textWidth)) {
            if (y + 18 > bottomLimit)
                break;
            CRect r(bounds.left + 20, y, bounds.right - 20, y + 18);
            ctx->drawString(line.c_str(), r, kCenterText);
            y += 18;
        }
    }

    // Version
    if (!info_.version.empty() && y + 20 <= bottomLimit) {
        ctx->setFont(smallFont);
        ctx->setFontColor(CColor(140, 140, 140, 255));
        CRect r(bounds.left + 20, y + 2, bounds.right - 20, y + 18);
        ctx->drawString(("v" + info_.version).c_str(), r, kCenterText);
        y += 20;
    }

    // Link lines are laid out first so the description knows how much room
    // to leave for them.
    std::vector<std::string> linkLines;
    if (!info_.url.empty()) {
        ctx->setFont(linkFont);
        linkLines = wrapText(ctx, displayUrl(info_.url), textWidth);
        if (linkLines.size() > 2)
            linkLines.resize(2);
    }
    const double descLimit =
        bottomLimit - (linkLines.empty() ? 0 : linkGap + linkHeight * linkLines.size());

    // Description
    y += 18;
    ctx->setFont(bodyFont);
    if (!info_.description.empty()) {
        ctx->setFontColor(CColor(190, 190, 190, 255));
        auto lines = wrapText(ctx, info_.description, textWidth);
        for (size_t i = 0; i < lines.size(); i++) {
            if (y + 19 > descLimit)
                break;
            std::string text = lines[i];
            if (i + 1 < lines.size() && y + 38 > descLimit)
                text += "\xE2\x80\xA6"; // ellipsis: more text was cut
            CRect r(bounds.left + 20, y, bounds.right - 20, y + 18);
            ctx->drawString(text.c_str(), r, kCenterText);
            y += 19;
        }
    } else if (info_.author.empty() && info_.url.empty()) {
        ctx->setFontColor(CColor(150, 150, 155, 255));
        CRect r(bounds.left + 20, y, bounds.right - 20, y + 18);
        ctx->drawString(i18n::str(i18n::StringId::ThemeInfoNoDetails), r, kCenterText);
        y += 19;
    }

    // Link
    urlLinkRect_ = CRect();
    if (!linkLines.empty()) {
        y += linkGap;
        ctx->setFont(linkFont);
        ctx->setFontColor(CColor(130, 170, 255, 255));
        CRect linkArea(bounds.left + 20, y, bounds.right - 20, y);
        for (const auto &line : linkLines) {
            CRect r(bounds.left + 20, y, bounds.right - 20, y + linkHeight);
            ctx->drawString(line.c_str(), r, kCenterText);
            y += linkHeight;
        }
        linkArea.bottom = y;
        urlLinkRect_ = linkArea;
        urlLinkRect_.offset(-bounds.left, -bounds.top);
    }

    titleFont->forget();
    bodyFont->forget();
    smallFont->forget();
    linkFont->forget();
}

bool ThemeInfoView::hitLink(const CPoint &local, bool click) {
    if (info_.url.empty() || !urlLinkRect_.pointInside(local))
        return false;
    if (click)
        openURL(info_.url);
    return true;
}

} // namespace MonkSynth
