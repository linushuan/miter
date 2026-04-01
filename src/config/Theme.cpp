// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include "Theme.h"
#include "util/TomlParser.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>

Theme Theme::darkDefault()
{
    Theme t;
    t.name = "dark";

    t.background      = QColor("#12171f");
    t.foreground      = QColor("#d9e1ee");
    t.currentLineBg   = QColor("#1b2330");
    t.lineNumberFg    = QColor("#5f7391");
    t.lineNumberBg    = QColor("#10151c");
    t.selectionBg     = QColor("#2b3a50");
    t.selectionFg     = QColor("#f2f6ff");
    t.cursorColor     = QColor("#ffb84d");
    t.searchHighlightBg = QColor("#ffe08a66");

    t.heading[0] = QColor("#ff6b6b");
    t.heading[1] = QColor("#4dabf7");
    t.heading[2] = QColor("#20c997");
    t.heading[3] = QColor("#ffd43b");
    t.heading[4] = QColor("#f783ac");
    t.heading[5] = QColor("#74c0fc");

    t.codeInlineFg    = QColor("#ffa94d");
    t.codeInlineBg    = QColor("#2a1f14");
    t.codeFenceFg     = QColor("#ffd59e");
    t.codeFenceBg     = QColor("#1a2330");
    t.codeFenceLangFg = QColor("#51cf66");

    t.blockquoteFg       = QColor("#9bb0cb");
    t.blockquoteBorderFg = QColor("#4c6687");
    t.listBulletFg       = QColor("#74c0fc");
    t.linkTextFg         = QColor("#4dd4ac");
    t.linkUrlFg          = QColor("#74a7ff");
    t.imageFg            = QColor("#ff9f6e");
    t.boldFg             = QColor("#ffffff");
    t.italicFg           = QColor("#b2f2bb");
    t.strikeFg           = QColor("#7489a8");
    t.tablePipeFg        = QColor("#7fb0ff");
    t.hrFg               = QColor("#45607e");
    t.markerFg           = QColor("#7a93b1");
    t.hardBreakFg        = QColor("#e599f7");

    t.latexDelimiterFg = QColor("#66d9e8");
    t.latexCommandFg   = QColor("#f06595");
    t.latexBraceFg     = QColor("#ffd43b");
    t.latexMathBodyFg  = QColor("#ffec99");
    t.latexEnvNameFg   = QColor("#69db7c");

    return t;
}

Theme Theme::resolveByName(const QString &themeName)
{
    Theme theme = Theme::darkDefault();
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString themeFile = themeName + ".toml";
    const QStringList themeCandidates = {
        QDir::current().filePath("themes/" + themeFile),
        appDir + "/themes/" + themeFile,
        appDir + "/../themes/" + themeFile,
        appDir + "/../share/miter/themes/" + themeFile,
        appDir + "/../Resources/themes/" + themeFile,
        QString(":/themes/%1").arg(themeFile)
    };

    for (const QString &path : themeCandidates) {
        if (QFile::exists(path)) {
            theme = Theme::fromToml(path);
            break;
        }
    }

    return theme;
}

Theme Theme::fromToml(const QString &path)
{
    Theme fallback = darkDefault();

    TomlParser toml(path);
    if (!toml.isValid()) return fallback;

    Theme t;
    t.name = toml.getString("", "name", fallback.name);

    auto color = [&](const QString &section, const QString &key, const QColor &def) -> QColor {
        QString val = toml.getString(section, key);
        if (val.isEmpty()) return def;
        QColor c(val);
        return c.isValid() ? c : def;
    };

    t.background      = color("", "background", fallback.background);
    t.foreground      = color("", "foreground", fallback.foreground);
    t.currentLineBg   = color("", "currentLineBg", fallback.currentLineBg);
    t.lineNumberFg    = color("", "lineNumberFg", fallback.lineNumberFg);
    t.lineNumberBg    = color("", "lineNumberBg", fallback.lineNumberBg);
    t.selectionBg     = color("", "selectionBg", fallback.selectionBg);
    t.selectionFg     = color("", "selectionFg", fallback.selectionFg);
    t.cursorColor     = color("", "cursorColor", fallback.cursorColor);
    t.searchHighlightBg = color("", "searchHighlightBg", fallback.searchHighlightBg);

    t.heading[0] = color("heading", "h1", fallback.heading[0]);
    t.heading[1] = color("heading", "h2", fallback.heading[1]);
    t.heading[2] = color("heading", "h3", fallback.heading[2]);
    t.heading[3] = color("heading", "h4", fallback.heading[3]);
    t.heading[4] = color("heading", "h5", fallback.heading[4]);
    t.heading[5] = color("heading", "h6", fallback.heading[5]);

    t.codeInlineFg    = color("code", "inlineFg", fallback.codeInlineFg);
    t.codeInlineBg    = color("code", "inlineBg", fallback.codeInlineBg);
    t.codeFenceFg     = color("code", "fenceFg", fallback.codeFenceFg);
    t.codeFenceBg     = color("code", "fenceBg", fallback.codeFenceBg);
    t.codeFenceLangFg = color("code", "langFg", fallback.codeFenceLangFg);

    t.blockquoteFg       = color("block", "blockquoteFg", fallback.blockquoteFg);
    t.blockquoteBorderFg = color("block", "blockquoteBorder", fallback.blockquoteBorderFg);
    t.listBulletFg       = color("block", "listBullet", fallback.listBulletFg);
    t.linkTextFg         = color("block", "linkText", fallback.linkTextFg);
    t.linkUrlFg          = color("block", "linkUrl", fallback.linkUrlFg);
    t.imageFg            = color("block", "image", fallback.imageFg);
    t.boldFg             = color("block", "bold", fallback.boldFg);
    t.italicFg           = color("block", "italic", fallback.italicFg);
    t.strikeFg           = color("block", "strike", fallback.strikeFg);
    t.tablePipeFg        = color("block", "tablePipe", fallback.tablePipeFg);
    t.hrFg               = color("block", "hr", fallback.hrFg);
    t.markerFg           = color("block", "marker", fallback.markerFg);
    t.hardBreakFg        = color("block", "hardBreak", fallback.hardBreakFg);

    t.latexDelimiterFg = color("latex", "delimiter", fallback.latexDelimiterFg);
    t.latexCommandFg   = color("latex", "command", fallback.latexCommandFg);
    t.latexBraceFg     = color("latex", "brace", fallback.latexBraceFg);
    t.latexMathBodyFg  = color("latex", "mathBody", fallback.latexMathBodyFg);
    t.latexEnvNameFg   = color("latex", "envName", fallback.latexEnvNameFg);

    return t;
}
