// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include "CjkUtil.h"
#include <QString>

namespace CjkUtil {

bool isCjk(QChar c)
{
    ushort u = c.unicode();

    // CJK Unified Ideographs
    if (u >= 0x4E00 && u <= 0x9FFF) return true;
    // CJK Unified Ideographs Extension A
    if (u >= 0x3400 && u <= 0x4DBF) return true;
    // CJK Compatibility Ideographs
    if (u >= 0xF900 && u <= 0xFAFF) return true;
    // Hiragana
    if (u >= 0x3040 && u <= 0x309F) return true;
    // Katakana
    if (u >= 0x30A0 && u <= 0x30FF) return true;
    // Hangul Syllables
    if (u >= 0xAC00 && u <= 0xD7AF) return true;
    // CJK Symbols and Punctuation
    if (u >= 0x3000 && u <= 0x303F) return true;
    // Fullwidth Forms
    if (u >= 0xFF00 && u <= 0xFFEF) return true;
    // Bopomofo
    if (u >= 0x3100 && u <= 0x312F) return true;

    return false;
}

bool isBoundary(QChar c)
{
    if (c.isSpace()) return true;
    if (c.isPunct()) return true;
    if (isCjk(c)) return true;
    return false;
}

bool isBlankLine(const QString &text)
{
    for (QChar c : text) {
        if (!c.isSpace() && c != QChar(0x3000))
            return false;
    }
    return true;
}

} // namespace CjkUtil
