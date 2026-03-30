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
    SetextUnderline,
    HR,
    BlankLine,
};

class BlockParser {
public:
    static BlockType classify(
        const QString       &text,
        ContextStack        &ctx,
        QVector<BlockToken> &tokens
    );

    static bool isSetextH1Underline(const QString &nextLine);
    static bool isSetextH2Underline(const QString &nextLine);

private:
    static bool matchCodeFenceStart(const QString &text, QChar &fenceChar, int &fenceLen, int &indent, QString &lang);
    static bool matchCodeFenceEnd(const QString &text, QChar fenceChar, int fenceLen);
    static bool matchATXHeading(const QString &text, int &level, int &contentStart, int &contentEnd);
    static bool matchTable(const QString &text);
    static bool matchBlockquote(const QString &text, int &contentOffset);
    static bool matchOrderedList(const QString &text, int &indent, int &contentOffset);
    static bool matchUnorderedList(const QString &text, int &indent, int &contentOffset);
    static bool matchHR(const QString &text);
};
