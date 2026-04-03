// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#pragma once

#include <QPlainTextEdit>
#include <QString>
#include <QColor>

class LineNumberArea;
class MdHighlighter;
class QInputMethodEvent;
class QFocusEvent;
class QTextBlock;
class QTimer;

class MdEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit MdEditor(QWidget *parent = nullptr);

    void    loadFile(const QString &path);
    bool    saveFile(const QString &path = QString());
    QString currentFilePath() const;
    bool    isModified() const;
    int     wordCount() const;
    int     charCount() const;

    // Line number area support
    int  lineNumberAreaWidth() const;
    void lineNumberAreaPaintEvent(QPaintEvent *event);

    void setFocusModeEnabled(bool enabled);
    bool isFocusModeEnabled() const;

    void setLineNumbersVisible(bool visible);
    bool lineNumbersVisible() const;

    void setWordWrapEnabled(bool enabled);
    bool isWordWrapEnabled() const;

    void    setThemeName(const QString &themeName);
    QString themeName() const;

    void setGlobalFontPointSize(int pointSize);
    int  globalFontPointSize() const;
    int  defaultFontPointSize() const;

protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void inputMethodEvent(QInputMethodEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;
    void focusOutEvent(QFocusEvent *event) override;

signals:
    void fileChanged(const QString &path);
    void fileSaved(const QString &path);
    void cursorPositionChanged(int line, int col);
    void wordCountChanged(int words, int chars);
    void modifiedChanged(bool modified);

private:
    LineNumberArea *lineNumberArea_;
    MdHighlighter  *highlighter_;
    QString         currentFile_;
    bool            focusModeEnabled_ = false;
    int             lastHighlightedBlock_ = -1;
    bool            lastHighlightFocusMode_ = false;
    bool            lastHighlightReadOnly_ = false;
    bool            lineNumbersVisible_ = true;
    QString         themeName_ = "dark";
    int             baseFontSize_ = 14;
    int             tabSize_ = 4;
    int             cachedDocRevision_ = -1;
    int             cachedWords_ = 0;
    int             cachedChars_ = 0;
    int             emittedWords_ = -1;
    int             emittedChars_ = -1;
    bool            imeComposing_ = false;
    int             preeditBlockNumber_ = -1;
    int             preeditStart_ = -1;
    int             preeditLength_ = 0;
    QColor          currentLineBg_ = QColor("#2a2a3e");
    QColor          lineNumberFg_ = QColor("#585b70");
    QColor          lineNumberBg_ = QColor("#1e1e2e");
    QTimer         *statusStatsTimer_ = nullptr;

    bool handleAutoCloseKey(QKeyEvent *event);
    bool handleBackspaceKey(QKeyEvent *event);
    bool handleTabKey(QKeyEvent *event);
    bool handleEnterKey(QKeyEvent *event);
    bool handleListIndentationKey(bool indentForward);
    void renumberOrderedListsAroundBlock(const QTextBlock &anchorBlock);
    void renumberOrderedListRange(const QTextBlock &startBlock, const QTextBlock &endBlock);
    void restoreCursorInBlock(int blockNumber, int column);
    void applyEditorFont(const QString &family, int pointSize);

    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLine();
    void updateStatusStats();
    void recomputeWordCountStats();
    void emitWordCountIfChanged();
};
