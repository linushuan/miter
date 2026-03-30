// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#pragma once

#include <QColor>
#include <QString>

struct Theme {
    QString name;

    // Editor base
    QColor background;
    QColor foreground;
    QColor currentLineBg;
    QColor lineNumberFg;
    QColor lineNumberBg;
    QColor selectionBg;
    QColor selectionFg;
    QColor cursorColor;
    QColor searchHighlightBg;

    // Markdown token colors
    QColor heading[6];
    QColor codeInlineFg;
    QColor codeInlineBg;
    QColor codeFenceFg;
    QColor codeFenceBg;
    QColor codeFenceLangFg;
    QColor blockquoteFg;
    QColor blockquoteBorderFg;
    QColor listBulletFg;
    QColor linkTextFg;
    QColor linkUrlFg;
    QColor imageFg;
    QColor boldFg;
    QColor italicFg;
    QColor strikeFg;
    QColor tablePipeFg;
    QColor hrFg;
    QColor markerFg;
    QColor hardBreakFg;

    // LaTeX token colors
    QColor latexDelimiterFg;
    QColor latexCommandFg;
    QColor latexBraceFg;
    QColor latexMathBodyFg;
    QColor latexEnvNameFg;

    static Theme fromToml(const QString &path);
    static Theme darkDefault();
    static Theme lightDefault();
};
