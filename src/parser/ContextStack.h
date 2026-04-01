// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#pragma once

#include <QStack>
#include <QByteArray>
#include <QString>
#include <QChar>

enum class BlockState : int {
    Normal       = 0,
    CodeFence    = 1,
    LatexDisplay = 2,
    LatexEnv     = 3,
    Blockquote   = 4,
    ListItem     = 5,
    Table        = 6,
    HtmlComment  = 7,
};

struct ContextFrame {
    BlockState state     = BlockState::Normal;
    int        depth     = 0;
    QChar      fenceChar = '`';
    int        fenceLen  = 3;
    QString    envName;
    int        listIndent = 0;
};

class ContextStack {
public:
    void               push(ContextFrame frame);
    void               pop();
    ContextFrame       top() const;
    BlockState         topState() const;
    int                listDepth() const;
    bool               inCode() const;
    bool               inLatex() const;
    bool               inTable() const;

    int  size() const;
    void clear();

    // Serialization helper kept for tests and potential persistence features.
    QByteArray          serialize() const;
    static ContextStack deserialize(const QByteArray &data);

private:
    QStack<ContextFrame> stack_;
};
