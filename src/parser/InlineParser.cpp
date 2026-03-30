// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include "InlineParser.h"
#include "LatexParser.h"
#include "util/CjkUtil.h"

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
        if (tryInlineLatex(s)) continue;
        if (tryImage(s)) continue;
        if (tryLink(s)) continue;
        if (tryBoldItalic(s)) continue;
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

bool InlineParser::tryInlineLatex(State &s)
{
    return LatexParser::parseInline(s.text, s.pos, s.tokens);
}

bool InlineParser::tryImage(State &s)
{
    if (s.pos + 1 >= s.end) return false;
    if (s.text[s.pos] != '!' || s.text[s.pos + 1] != '[') return false;

    // Find ]
    int altStart = s.pos + 2;
    int altEnd = -1;
    for (int i = altStart; i < s.end; ++i) {
        if (s.text[i] == ']') { altEnd = i; break; }
    }
    if (altEnd < 0 || altEnd + 1 >= s.end || s.text[altEnd + 1] != '(') return false;

    // Find )
    int urlStart = altEnd + 2;
    int urlEnd = -1;
    for (int i = urlStart; i < s.end; ++i) {
        if (s.text[i] == ')') { urlEnd = i; break; }
    }
    if (urlEnd < 0) return false;

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
