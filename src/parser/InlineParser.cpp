// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include "InlineParser.h"
#include "LatexParser.h"
#include "util/CjkUtil.h"

namespace {
int findUnescapedChar(const QString &text, int from, int end, QChar needle)
{
    for (int i = from; i < end; ++i) {
        if (text[i] == QLatin1Char('\\') && i + 1 < end) {
            ++i;
            continue;
        }
        if (text[i] == needle) {
            return i;
        }
    }
    return -1;
}

bool parseImageAt(const QString &text,
                  int start,
                  int end,
                  int &altStart,
                  int &altEnd,
                  int &urlStart,
                  int &urlEnd,
                  int &imageEnd)
{
    if (start + 1 >= end || text[start] != QLatin1Char('!') || text[start + 1] != QLatin1Char('[')) {
        return false;
    }

    altStart = start + 2;
    altEnd = findUnescapedChar(text, altStart, end, QLatin1Char(']'));
    if (altEnd < 0 || altEnd + 1 >= end || text[altEnd + 1] != QLatin1Char('(')) {
        return false;
    }

    urlStart = altEnd + 2;
    urlEnd = findUnescapedChar(text, urlStart, end, QLatin1Char(')'));
    if (urlEnd < 0) {
        return false;
    }

    imageEnd = urlEnd + 1;
    return true;
}

bool isSimpleAutoLinkTarget(const QString &target)
{
    if (target.isEmpty()) {
        return false;
    }

    for (QChar c : target) {
        if (c.isSpace()) {
            return false;
        }
    }

    const Qt::CaseSensitivity cs = Qt::CaseInsensitive;
    if (target.startsWith(QLatin1String("http://"), cs) ||
        target.startsWith(QLatin1String("https://"), cs) ||
        target.startsWith(QLatin1String("ftp://"), cs) ||
        target.startsWith(QLatin1String("mailto:"), cs) ||
        target.startsWith(QLatin1String("www."), cs)) {
        return true;
    }

    // A minimal email-style fallback for angle autolinks.
    const int at = target.indexOf(QLatin1Char('@'));
    return at > 0 && at < target.size() - 1;
}

}

void InlineParser::parse(const QString &text, int offset, const ContextStack &ctx, QVector<InlineToken> &tokens)
{
    Q_UNUSED(ctx);
    tokens.clear();

    const int end = static_cast<int>(text.length());
    const int start = (offset < 0) ? 0 : ((offset > end) ? end : offset);
    State s{text, start, end, tokens};

    while (s.pos < s.end) {
        if (tryEscape(s)) continue;
        if (tryInlineCode(s)) continue;
        if (tryHtmlComment(s)) continue;
        if (tryInlineLatex(s)) continue;
        if (tryAngleAutoLink(s)) continue;
        if (tryLinkedImageLink(s)) continue;
        if (tryImage(s)) continue;
        if (tryLink(s)) continue;
        if (tryUnderline(s)) continue;
        if (tryHighlight(s)) continue;
        if (tryBoldItalic(s)) continue;
        if (trySuperscriptOrSubscript(s)) continue;
        if (tryStrikethrough(s)) continue;
        if (tryHardBreak(s)) continue;

        // Normal character, skip
        s.pos++;
    }
}

bool InlineParser::tryEscape(State &s)
{
    if (s.pos + 1 >= s.end) return false;
    if (s.text[s.pos] != '\\') return false;

    QChar next = s.text[s.pos + 1];
    // Only ASCII punctuation can be escaped
    if (next.unicode() >= 0x21 && next.unicode() <= 0x7E && !next.isLetterOrNumber()) {
        s.tokens.append({s.pos, 2, TokenType::Escape});
        s.pos += 2;
        return true;
    }
    return false;
}

bool InlineParser::tryInlineCode(State &s)
{
    if (s.text[s.pos] != '`') return false;

    // Count opening backticks
    int openStart = s.pos;
    int openLen = 0;
    while (s.pos + openLen < s.end && s.text[s.pos + openLen] == '`')
        openLen++;

    // Find matching closing backticks
    int searchPos = s.pos + openLen;
    while (searchPos <= s.end - openLen) {
        if (s.text[searchPos] == '`') {
            int closeLen = 0;
            while (searchPos + closeLen < s.end && s.text[searchPos + closeLen] == '`')
                closeLen++;
            if (closeLen == openLen) {
                // Found match
                s.tokens.append({openStart, openLen, TokenType::InlineCodeMark});
                s.tokens.append({openStart + openLen, searchPos - openStart - openLen, TokenType::InlineCode});
                s.tokens.append({searchPos, closeLen, TokenType::InlineCodeMark});
                s.pos = searchPos + closeLen;
                return true;
            }
            searchPos += closeLen;
        } else {
            searchPos++;
        }
    }
    return false;
}

bool InlineParser::tryHtmlComment(State &s)
{
    if (s.pos + 3 >= s.end) return false;
    if (s.text[s.pos] != '<' ||
        s.text[s.pos + 1] != '!' ||
        s.text[s.pos + 2] != '-' ||
        s.text[s.pos + 3] != '-') {
        return false;
    }

    const int closePos = s.text.indexOf("-->", s.pos + 4);
    if (closePos < 0) {
        return false;
    }

    s.tokens.append({s.pos, closePos + 3 - s.pos, TokenType::HtmlComment});
    s.pos = closePos + 3;
    return true;
}

bool InlineParser::tryInlineLatex(State &s)
{
    return LatexParser::parseInline(s.text, s.pos, s.tokens);
}

bool InlineParser::tryAngleAutoLink(State &s)
{
    if (s.text[s.pos] != QLatin1Char('<')) return false;
    if (s.pos + 2 >= s.end) return false;

    const int closePos = s.text.indexOf(QLatin1Char('>'), s.pos + 1);
    if (closePos < 0) {
        return false;
    }

    const QString target = s.text.mid(s.pos + 1, closePos - s.pos - 1);
    if (!isSimpleAutoLinkTarget(target)) {
        return false;
    }

    s.tokens.append({s.pos, 1, TokenType::LinkBracket});
    s.tokens.append({s.pos + 1, closePos - s.pos - 1, TokenType::LinkUrl});
    s.tokens.append({closePos, 1, TokenType::LinkBracket});
    s.pos = closePos + 1;
    return true;
}

bool InlineParser::tryLinkedImageLink(State &s)
{
    if (s.text[s.pos] != QLatin1Char('[')) return false;
    if (s.pos + 2 >= s.end || s.text[s.pos + 1] != QLatin1Char('!') || s.text[s.pos + 2] != QLatin1Char('[')) {
        return false;
    }

    int altStart = 0;
    int altEnd = 0;
    int imageUrlStart = 0;
    int imageUrlEnd = 0;
    int imageEnd = 0;
    if (!parseImageAt(s.text, s.pos + 1, s.end, altStart, altEnd, imageUrlStart, imageUrlEnd, imageEnd)) {
        return false;
    }

    if (imageEnd + 1 >= s.end ||
        s.text[imageEnd] != QLatin1Char(']') ||
        s.text[imageEnd + 1] != QLatin1Char('(')) {
        return false;
    }

    const int outerUrlStart = imageEnd + 2;
    const int outerUrlEnd = findUnescapedChar(s.text, outerUrlStart, s.end, QLatin1Char(')'));
    if (outerUrlEnd < 0) {
        return false;
    }

    s.tokens.append({s.pos, 1, TokenType::LinkBracket});                 // [
    s.tokens.append({s.pos + 1, 2, TokenType::ImageBracket});            // ![
    s.tokens.append({altStart, altEnd - altStart, TokenType::ImageAlt});
    s.tokens.append({altEnd, 2, TokenType::ImageBracket});               // ](
    s.tokens.append({imageUrlStart, imageUrlEnd - imageUrlStart, TokenType::ImageUrl});
    s.tokens.append({imageUrlEnd, 1, TokenType::ImageBracket});          // )
    s.tokens.append({imageEnd, 2, TokenType::LinkBracket});              // ](
    s.tokens.append({outerUrlStart, outerUrlEnd - outerUrlStart, TokenType::LinkUrl});
    s.tokens.append({outerUrlEnd, 1, TokenType::LinkBracket});           // )
    s.pos = outerUrlEnd + 1;
    return true;
}

bool InlineParser::tryImage(State &s)
{
    int altStart = 0;
    int altEnd = 0;
    int urlStart = 0;
    int urlEnd = 0;
    int imageEnd = 0;
    if (!parseImageAt(s.text, s.pos, s.end, altStart, altEnd, urlStart, urlEnd, imageEnd)) {
        return false;
    }

    s.tokens.append({s.pos, 2, TokenType::ImageBracket});           // ![
    s.tokens.append({altStart, altEnd - altStart, TokenType::ImageAlt});
    s.tokens.append({altEnd, 2, TokenType::ImageBracket});          // ](
    s.tokens.append({urlStart, urlEnd - urlStart, TokenType::ImageUrl});
    s.tokens.append({urlEnd, 1, TokenType::ImageBracket});          // )
    s.pos = urlEnd + 1;
    return true;
}

bool InlineParser::tryLink(State &s)
{
    if (s.text[s.pos] != '[') return false;

    // Find ] (non-greedy, no nested [])
    int textStart = s.pos + 1;
    int textEnd = -1;
    for (int i = textStart; i < s.end; ++i) {
        if (s.text[i] == '[') return false;  // Nested [, bail
        if (s.text[i] == ']') { textEnd = i; break; }
    }
    if (textEnd < 0 || textEnd + 1 >= s.end || s.text[textEnd + 1] != '(') return false;

    // Find )
    int urlStart = textEnd + 2;
    int urlEnd = -1;
    for (int i = urlStart; i < s.end; ++i) {
        if (s.text[i] == ')') { urlEnd = i; break; }
    }
    if (urlEnd < 0) return false;

    s.tokens.append({s.pos, 1, TokenType::LinkBracket});           // [
    s.tokens.append({textStart, textEnd - textStart, TokenType::LinkText});
    s.tokens.append({textEnd, 2, TokenType::LinkBracket});         // ](
    s.tokens.append({urlStart, urlEnd - urlStart, TokenType::LinkUrl});
    s.tokens.append({urlEnd, 1, TokenType::LinkBracket});          // )
    s.pos = urlEnd + 1;
    return true;
}

bool InlineParser::tryUnderline(State &s)
{
    if (s.pos + 4 > s.end) {
        return false;
    }
    if (s.text[s.pos] != QLatin1Char('+') || s.text[s.pos + 1] != QLatin1Char('+')) {
        return false;
    }

    const int contentStart = s.pos + 2;
    if (s.text[contentStart].isSpace()) {
        return false;
    }

    for (int i = contentStart; i <= s.end - 2; ++i) {
        if (s.text[i] == QLatin1Char('+') && s.text[i + 1] == QLatin1Char('+')) {
            if (i <= contentStart || s.text[i - 1].isSpace()) {
                continue;
            }
            s.tokens.append({s.pos, 2, TokenType::UnderlineMarker});
            s.tokens.append({contentStart, i - contentStart, TokenType::Underline});
            s.tokens.append({i, 2, TokenType::UnderlineMarker});
            s.pos = i + 2;
            return true;
        }
    }

    return false;
}

bool InlineParser::tryHighlight(State &s)
{
    if (s.pos + 4 > s.end) {
        return false;
    }
    if (s.text[s.pos] != QLatin1Char('=') || s.text[s.pos + 1] != QLatin1Char('=')) {
        return false;
    }

    const int contentStart = s.pos + 2;
    if (s.text[contentStart].isSpace()) {
        return false;
    }

    for (int i = contentStart; i <= s.end - 2; ++i) {
        if (s.text[i] == QLatin1Char('=') && s.text[i + 1] == QLatin1Char('=')) {
            if (i <= contentStart || s.text[i - 1].isSpace()) {
                continue;
            }
            s.tokens.append({s.pos, 2, TokenType::HighlightMarker});
            s.tokens.append({contentStart, i - contentStart, TokenType::Highlight});
            s.tokens.append({i, 2, TokenType::HighlightMarker});
            s.pos = i + 2;
            return true;
        }
    }

    return false;
}

bool InlineParser::tryBoldItalic(State &s)
{
    QChar marker = s.text[s.pos];
    if (marker != '*' && marker != '_') return false;

    // Count consecutive markers
    int markerStart = s.pos;
    int markerLen = 0;
    while (markerStart + markerLen < s.end && s.text[markerStart + markerLen] == marker)
        markerLen++;

    if (!isLeftFlanking(s.text, markerStart, markerLen))
        return false;

    // Determine type: 3=bold+italic, 2=bold, 1=italic
    int matchLen = qMin(markerLen, 3);
    if (matchLen < 1) return false;

    // Scan for matching close markers
    int contentStart = markerStart + matchLen;
    for (int i = contentStart; i <= s.end - matchLen; ++i) {
        if (s.text[i] == marker) {
            int closeLen = 0;
            while (i + closeLen < s.end && s.text[i + closeLen] == marker)
                closeLen++;

            if (closeLen >= matchLen && isRightFlanking(s.text, i, closeLen)) {
                TokenType markerType, contentType;
                if (matchLen == 3) {
                    markerType = TokenType::BoldItalicMarker;
                    contentType = TokenType::BoldItalic;
                } else if (matchLen == 2) {
                    markerType = TokenType::BoldMarker;
                    contentType = TokenType::Bold;
                } else {
                    markerType = TokenType::ItalicMarker;
                    contentType = TokenType::Italic;
                }

                s.tokens.append({markerStart, matchLen, markerType});
                s.tokens.append({contentStart, i - contentStart, contentType});
                s.tokens.append({i, matchLen, markerType});
                s.pos = i + matchLen;
                return true;
            }
            i += closeLen - 1; // Skip past this run
        }
    }
    return false;
}

bool InlineParser::tryStrikethrough(State &s)
{
    if (s.pos + 1 >= s.end) return false;
    if (s.text[s.pos] != '~' || s.text[s.pos + 1] != '~') return false;

    // Find closing ~~
    int contentStart = s.pos + 2;
    for (int i = contentStart; i < s.end - 1; ++i) {
        if (s.text[i] == '~' && s.text[i + 1] == '~') {
            s.tokens.append({s.pos, 2, TokenType::StrikeMarker});
            s.tokens.append({contentStart, i - contentStart, TokenType::Strikethrough});
            s.tokens.append({i, 2, TokenType::StrikeMarker});
            s.pos = i + 2;
            return true;
        }
    }
    return false;
}

bool InlineParser::trySuperscriptOrSubscript(State &s)
{
    const QChar marker = s.text[s.pos];
    if (marker != QLatin1Char('^') && marker != QLatin1Char('~')) {
        return false;
    }
    if (s.pos + 2 >= s.end) {
        return false;
    }
    if (s.text[s.pos + 1].isSpace() || s.text[s.pos + 1] == marker) {
        return false;
    }

    const int contentStart = s.pos + 1;
    for (int i = contentStart; i < s.end; ++i) {
        if (s.text[i] == QLatin1Char('\\') && i + 1 < s.end) {
            ++i;
            continue;
        }

        if (s.text[i] == marker) {
            if (i <= contentStart || s.text[i - 1].isSpace()) {
                return false;
            }

            TokenType markerType = (marker == QLatin1Char('^'))
                ? TokenType::SuperscriptMarker
                : TokenType::SubscriptMarker;
            TokenType contentType = (marker == QLatin1Char('^'))
                ? TokenType::Superscript
                : TokenType::Subscript;

            s.tokens.append({s.pos, 1, markerType});
            s.tokens.append({contentStart, i - contentStart, contentType});
            s.tokens.append({i, 1, markerType});
            s.pos = i + 1;
            return true;
        }

        if (s.text[i].isSpace()) {
            return false;
        }
    }

    return false;
}

bool InlineParser::tryHardBreak(State &s)
{
    // Only at end of line
    // Trailing 2+ spaces
    if (s.text[s.pos] == ' ' && s.pos >= 1) {
        int spaceStart = s.pos;
        int spaceEnd = s.pos;
        while (spaceEnd < s.end && s.text[spaceEnd] == ' ')
            spaceEnd++;
        if (spaceEnd == s.end && (spaceEnd - spaceStart) >= 2) {
            s.tokens.append({spaceStart, spaceEnd - spaceStart, TokenType::HardBreakSpace});
            s.pos = spaceEnd;
            return true;
        }
    }

    // Trailing backslash
    if (s.text[s.pos] == '\\' && s.pos + 1 == s.end) {
        s.tokens.append({s.pos, 1, TokenType::HardBreakBackslash});
        s.pos++;
        return true;
    }

    return false;
}

bool InlineParser::isLeftFlanking(const QString &text, int markerStart, int markerLen)
{
    // After the marker run: must not be whitespace
    int afterPos = markerStart + markerLen;
    if (afterPos >= text.length()) return false;
    QChar after = text[afterPos];
    if (after.isSpace()) return false;

    // Before the marker run: must be boundary (start of line, space, punct, CJK)
    if (markerStart == 0) return true;
    QChar before = text[markerStart - 1];
    return CjkUtil::isBoundary(before);
}

bool InlineParser::isRightFlanking(const QString &text, int markerStart, int markerLen)
{
    Q_UNUSED(markerLen);
    // Before the marker: must not be whitespace
    if (markerStart == 0) return false;
    QChar before = text[markerStart - 1];
    if (before.isSpace()) return false;

    // After the marker: must be boundary (end of line, space, punct, CJK)
    int afterPos = markerStart + markerLen;
    if (afterPos >= text.length()) return true;
    QChar after = text[afterPos];
    return CjkUtil::isBoundary(after);
}
