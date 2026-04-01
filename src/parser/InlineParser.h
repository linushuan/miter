// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#pragma once

#include "ContextStack.h"
#include "TokenTypes.h"
#include <QString>
#include <QVector>

struct InlineToken {
    int       start;
    int       length;
    TokenType type;
};

class InlineParser {
public:
    static void parse(
        const QString        &text,
        int                   offset,
        const ContextStack   &ctx,
        QVector<InlineToken> &tokens
    );

private:
    struct State {
        const QString &text;
        int            pos;
        int            end;
        QVector<InlineToken> &tokens;
    };

    static bool tryEscape(State &s);
    static bool tryInlineCode(State &s);
    static bool tryHtmlComment(State &s);
    static bool tryInlineLatex(State &s);
    static bool tryAngleAutoLink(State &s);
    static bool tryLinkedImageLink(State &s);
    static bool tryImage(State &s);
    static bool tryLink(State &s);
    static bool tryUnderline(State &s);
    static bool tryHighlight(State &s);
    static bool tryBoldItalic(State &s);
    static bool trySuperscriptOrSubscript(State &s);
    static bool tryStrikethrough(State &s);
    static bool tryHardBreak(State &s);

    static bool isLeftFlanking(const QString &text, int markerStart, int markerLen);
    static bool isRightFlanking(const QString &text, int markerStart, int markerLen);
};
