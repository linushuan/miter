// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#pragma once

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QHash>

#include "parser/TokenTypes.h"
#include "parser/ContextStack.h"
#include "config/Theme.h"

class MdHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit MdHighlighter(QTextDocument *parent, const Theme &theme);
    void setTheme(const Theme &theme);
    void setEnabled(bool enabled);
    void setBaseFontSize(int pointSize);

protected:
    void highlightBlock(const QString &text) override;

private:
    void highlightHeading(const QString &text, int level);
    void highlightCodeFence(const QString &text, ContextStack &ctx);
    void highlightInline(const QString &text, int offset, const ContextStack &ctx);
    void highlightLatex(const QString &text, int offset, ContextStack &ctx);
    void highlightTable(const QString &text, const ContextStack &ctx);
    void highlightBlockquote(const QString &text, ContextStack &ctx);
    void highlightListItem(const QString &text, const ContextStack &ctx);

    ContextStack restoreContext() const;
    void         saveContext(const ContextStack &ctx);

    void buildFormats();

    Theme theme_;
    QHash<TokenType, QTextCharFormat> formats_;
    bool enabled_ = true;
    bool setextSyncInProgress_ = false;
    bool setextRefreshPending_ = false;
    int baseFontSize_ = 14;
};
