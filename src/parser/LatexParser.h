// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#pragma once

#include <QString>
#include <QVector>

struct InlineToken;

class LatexParser {
public:
    // Parse inline $...$ at position pos in text.
    // Returns true if a match was found and tokens were added.
    // Advances pos past the match.
    static bool parseInline(const QString &text, int &pos, QVector<InlineToken> &tokens);

    // Parse LaTeX commands (\cmd) and brace groups within a LaTeX body line
    static void parseLatexBody(const QString &text, int start, int length, QVector<InlineToken> &tokens);

private:
    static bool isValidInlineMathStart(const QString &text, int pos);
    static int  findMatchingBrace(const QString &text, int openPos);
};
