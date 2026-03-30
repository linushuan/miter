// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include "LatexParser.h"
#include "InlineParser.h"  // for InlineToken struct

bool LatexParser::parseInline(const QString &text, int &pos, QVector<InlineToken> &tokens)
{
    const int textLen = static_cast<int>(text.length());
    if (pos < 0 || pos >= textLen) return false;
    if (text[pos] != '$') return false;

    // $$ is display math, not inline — don't handle here
    if (pos + 1 < textLen && text[pos + 1] == '$') return false;

    if (!isValidInlineMathStart(text, pos)) return false;

    // Scan for closing $
    int start = pos;
    int contentStart = pos + 1;
    for (int i = contentStart; i < textLen; ++i) {
        // Skip escaped $
        if (text[i] == '\\' && i + 1 < textLen && text[i + 1] == '$') {
            i++; // skip past \$
            continue;
        }
        if (text[i] == '$') {
            // Closing $ must not be preceded by space
            if (i > contentStart && text[i - 1].isSpace()) return false;

            // Valid inline math
            tokens.append({start, 1, TokenType::LatexDelimiter});
            tokens.append({contentStart, i - contentStart, TokenType::LatexMathBody});
            tokens.append({i, 1, TokenType::LatexDelimiter});
            pos = i + 1;
            return true;
        }
        // Inline math cannot span lines
        if (text[i] == '\n') return false;
    }
    return false;
}

void LatexParser::parseLatexBody(const QString &text, int start, int length, QVector<InlineToken> &tokens)
{
    const int textLen = static_cast<int>(text.length());
    const int clampedStart = (start < 0) ? 0 : ((start > textLen) ? textLen : start);
    const int clampedLength = (length < 0) ? 0 : length;
    const int end = qMin(textLen, clampedStart + clampedLength);
    int pos = clampedStart;

    while (pos < end) {
        // \command
        if (text[pos] == '\\' && pos + 1 < end && text[pos + 1].isLetter()) {
            int cmdStart = pos;
            pos += 1;
            while (pos < end && text[pos].isLetter())
                pos++;
            tokens.append({cmdStart, pos - cmdStart, TokenType::LatexCommand});

            // Check for brace group(s) after command
            while (pos < end && text[pos] == '{') {
                tokens.append({pos, 1, TokenType::LatexBrace}); // {
                int braceEnd = findMatchingBrace(text, pos);
                if (braceEnd < 0) break;
                // Content between braces
                if (braceEnd > pos + 1) {
                    tokens.append({pos + 1, braceEnd - pos - 1, TokenType::LatexMathBody});
                }
                tokens.append({braceEnd, 1, TokenType::LatexBrace}); // }
                pos = braceEnd + 1;
            }
            continue;
        }

        // { } standalone
        if (text[pos] == '{' || text[pos] == '}') {
            tokens.append({pos, 1, TokenType::LatexBrace});
            pos++;
            continue;
        }

        pos++;
    }
}

bool LatexParser::isValidInlineMathStart(const QString &text, int pos)
{
    const int textLen = static_cast<int>(text.length());
    // $ must not be followed by whitespace
    if (pos + 1 >= textLen) return false;
    if (text[pos + 1].isSpace()) return false;

    return true;
}

int LatexParser::findMatchingBrace(const QString &text, int openPos)
{
    const int textLen = static_cast<int>(text.length());
    int depth = 0;
    for (int i = openPos; i < textLen; ++i) {
        if (text[i] == '{') depth++;
        else if (text[i] == '}') {
            depth--;
            if (depth == 0) return i;
        }
    }
    return -1; // No match found
}
