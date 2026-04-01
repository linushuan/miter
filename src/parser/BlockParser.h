// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#pragma once

#include "ContextStack.h"
#include "TokenTypes.h"
#include <QString>
#include <QVector>

struct BlockToken {
    int       start;
    int       length;
    TokenType type;
};

enum class BlockType {
    Normal,
    Heading,
    CodeFenceStart,
    CodeFenceEnd,
    CodeFenceBody,
    LatexDisplayStart,
    LatexDisplayEnd,
    LatexDisplayBody,
    LatexEnvStart,
    LatexEnvEnd,
    LatexEnvBody,
    Blockquote,
    ListItem,
    Table,
    HtmlComment,
    SetextUnderline,
    HR,
    BlankLine,
};

struct OrderedListLineMatch {
    int     indent       = 0;
    int     number       = 0;
    QString delimiter;
    QString checkbox;
    QString content;
    int     numberStart  = 0;
    int     numberLength = 0;
    int     contentStart = 0;
    int     markerEnd    = 0;
};

struct UnorderedListLineMatch {
    int     indent       = 0;
    QString marker;
    QString checkbox;
    QString content;
    int     contentStart = 0;
    int     markerEnd    = 0;
};

struct BlockquoteLineMatch {
    QString prefix;
    QString content;
    int     indent = 0;
    int     depth  = 0;
};

class BlockParser {
public:
    static BlockType classify(
        const QString       &text,
        ContextStack        &ctx,
        QVector<BlockToken> &tokens
    );

    static bool parseOrderedListLine(const QString &text, OrderedListLineMatch *match = nullptr);
    static bool parseUnorderedListLine(const QString &text, UnorderedListLineMatch *match = nullptr);
    static bool parseBlockquoteLine(const QString &text, BlockquoteLineMatch *match = nullptr);

    static bool isHorizontalRule(const QString &text);
    static bool isStandaloneLatexDisplayFence(const QString &text);
    static bool isCodeFenceStartLine(const QString &text);

    static bool isSetextH1Underline(const QString &nextLine);
    static bool isSetextH2Underline(const QString &nextLine);

private:
    static bool classifyInOpenContext(const QString &text, ContextStack &ctx, QVector<BlockToken> &tokens, BlockType &type);
    static BlockType classifyInNormalContext(const QString &text, ContextStack &ctx, QVector<BlockToken> &tokens);

    static bool matchCodeFenceStart(const QString &text, QChar &fenceChar, int &fenceLen, int &indent, QString &lang);
    static bool matchCodeFenceEnd(const QString &text, QChar fenceChar, int fenceLen);
    static bool matchATXHeading(const QString &text, int &level, int &contentStart, int &contentEnd);
    static bool matchTable(const QString &text);
    static bool matchBlockquote(const QString &text, int &contentOffset);
    static bool matchOrderedList(const QString &text, int &indent, int &contentOffset);
    static bool matchUnorderedList(const QString &text, int &indent, int &contentOffset);
    static bool matchHR(const QString &text);
};
