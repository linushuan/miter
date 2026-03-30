#pragma once

#include <QPlainTextEdit>
#include <QString>
#include <QColor>

class LineNumberArea;
class MdHighlighter;
class QInputMethodEvent;

class MdEditor : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit MdEditor(QWidget *parent = nullptr);

    void    loadFile(const QString &path);
    bool    saveFile(const QString &path = QString());
    QString currentFilePath() const;
    bool    isModified() const;

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
    bool            lineNumbersVisible_ = true;
    QString         themeName_ = "dark";
    int             baseFontSize_ = 14;
    int             tabSize_ = 4;
    QColor          currentLineBg_ = QColor("#2a2a3e");
    QColor          lineNumberFg_ = QColor("#585b70");
    QColor          lineNumberBg_ = QColor("#1e1e2e");

    void updateLineNumberAreaWidth(int newBlockCount);
    void updateLineNumberArea(const QRect &rect, int dy);
    void highlightCurrentLine();
    void updateStatusStats();
};
