#include "MdEditor.h"
#include "LineNumberArea.h"
#include "highlight/MdHighlighter.h"
#include "config/Settings.h"
#include "config/Theme.h"

#include <QFile>
#include <QTextStream>
#include <QPainter>
#include <QTextBlock>
#include <QKeyEvent>
#include <QInputMethodEvent>
#include <QScrollBar>
#include <QTextOption>
#include <QCoreApplication>
#include <QDir>
#include <QPalette>
#include <QRegularExpression>

namespace {
Theme resolveThemeByName(const QString &themeName)
{
    Theme theme = Theme::darkDefault();
    const QStringList themeCandidates = {
        QDir::current().filePath("themes/" + themeName + ".toml"),
        QCoreApplication::applicationDirPath() + "/../share/mded/themes/" + themeName + ".toml",
        QCoreApplication::applicationDirPath() + "/themes/" + themeName + ".toml"
    };
    for (const QString &path : themeCandidates) {
        if (QFile::exists(path)) {
            theme = Theme::fromToml(path);
            break;
        }
    }
    return theme;
}
}

MdEditor::MdEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    const Settings settings = Settings::load();
    themeName_ = settings.theme;

    // Initialize line number area
    lineNumberArea_ = new LineNumberArea(this);

    Theme theme = resolveThemeByName(themeName_);

    // Initialize highlighter with configured theme.
    highlighter_ = new MdHighlighter(document(), theme);

    // Connect signals for line number area
    connect(this, &QPlainTextEdit::blockCountChanged,
            this, &MdEditor::updateLineNumberAreaWidth);
    connect(this, &QPlainTextEdit::updateRequest,
            this, &MdEditor::updateLineNumberArea);

    // Current line highlight
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &MdEditor::highlightCurrentLine);
    connect(this, &QPlainTextEdit::cursorPositionChanged,
            this, &MdEditor::updateStatusStats);
    connect(this, &QPlainTextEdit::textChanged,
            this, &MdEditor::updateStatusStats);

    // Forward modification changed signal
    connect(document(), &QTextDocument::modificationChanged,
            this, &MdEditor::modifiedChanged);

    // Initial setup
    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    // Apply theme colors.
    QPalette pal = palette();
    pal.setColor(QPalette::Base, theme.background);
    pal.setColor(QPalette::Text, theme.foreground);
    pal.setColor(QPalette::Highlight, theme.selectionBg);
    pal.setColor(QPalette::HighlightedText, theme.selectionFg);
    setPalette(pal);
    currentLineBg_ = theme.currentLineBg;
    lineNumberFg_ = theme.lineNumberFg;
    lineNumberBg_ = theme.lineNumberBg;

    // Apply configured font and editor behavior.
    QFont f(settings.fontFamily, settings.fontSize);
    setFont(f);
    baseFontSize_ = qMax(6, settings.fontSize);
    highlighter_->setBaseFontSize(baseFontSize_);
    setWordWrapEnabled(settings.wordWrap);
    setLineNumbersVisible(settings.lineNumbers);

    // Tab -> configurable spaces
    tabSize_ = qMax(1, settings.tabSize);
    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * tabSize_);
    updateStatusStats();
}

void MdEditor::loadFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    setPlainText(in.readAll());
    file.close();

    currentFile_ = path;
    document()->setModified(false);
    emit fileChanged(path);
    updateStatusStats();
}

bool MdEditor::saveFile(const QString &path)
{
    QString savePath = path.isEmpty() ? currentFile_ : path;
    if (savePath.isEmpty())
        return false;

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << toPlainText();
    file.close();

    currentFile_ = savePath;
    document()->setModified(false);
    emit fileChanged(savePath);
    emit fileSaved(savePath);
    updateStatusStats();
    return true;
}

QString MdEditor::currentFilePath() const
{
    return currentFile_;
}

bool MdEditor::isModified() const
{
    return document()->isModified();
}

int MdEditor::lineNumberAreaWidth() const
{
    if (!lineNumbersVisible_)
        return 0;

    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }
    int space = 3 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * digits;
    return space;
}

void MdEditor::lineNumberAreaPaintEvent(QPaintEvent *event)
{
    if (!lineNumbersVisible_)
        return;

    QPainter painter(lineNumberArea_);
    painter.fillRect(event->rect(), lineNumberBg_);

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    int bottom = top + qRound(blockBoundingRect(block).height());

    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && bottom >= event->rect().top()) {
            QString number = QString::number(blockNumber + 1);
            painter.setPen(lineNumberFg_);
            painter.drawText(0, top, lineNumberArea_->width() - 2,
                           fontMetrics().height(), Qt::AlignRight, number);
        }
        block = block.next();
        top = bottom;
        bottom = top + qRound(blockBoundingRect(block).height());
        ++blockNumber;
    }
}

void MdEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    QRect cr = contentsRect();
    lineNumberArea_->setGeometry(QRect(cr.left(), cr.top(),
                                       lineNumberAreaWidth(), cr.height()));
}

void MdEditor::keyPressEvent(QKeyEvent *event)
{
    // Tab -> 4 spaces
    if (event->key() == Qt::Key_Tab) {
        insertPlainText("    ");
        return;
    }

    // Auto-indent on Enter
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        QString currentLine = textCursor().block().text();
        static QRegularExpression unorderedRe(R"(^(\s*)([-*+])\s+(\[[ xX]\]\s+)?(.*)$)");
        static QRegularExpression orderedRe(R"(^(\s*)(\d+)([.)])\s+(\[[ xX]\]\s+)?(.*)$)");

        auto unorderedMatch = unorderedRe.match(currentLine);
        auto orderedMatch = orderedRe.match(currentLine);
        int indent = 0;
        for (QChar c : currentLine) {
            if (c == ' ') indent++;
            else break;
        }

        QPlainTextEdit::keyPressEvent(event);

        if (orderedMatch.hasMatch()) {
            const QString leading = orderedMatch.captured(1);
            const int currentNum = orderedMatch.captured(2).toInt();
            const QString delim = orderedMatch.captured(3);
            const QString checkbox = orderedMatch.captured(4);
            const QString content = orderedMatch.captured(5).trimmed();

            if (content.isEmpty() && checkbox.trimmed().isEmpty()) {
                insertPlainText(leading);
            } else {
                insertPlainText(QString("%1%2%3 %4")
                    .arg(leading)
                    .arg(currentNum + 1)
                    .arg(delim)
                    .arg(checkbox));
            }
            return;
        }

        if (unorderedMatch.hasMatch()) {
            const QString leading = unorderedMatch.captured(1);
            const QString bullet = unorderedMatch.captured(2);
            const QString checkbox = unorderedMatch.captured(3);
            const QString content = unorderedMatch.captured(4).trimmed();

            if (content.isEmpty() && checkbox.trimmed().isEmpty()) {
                insertPlainText(leading);
            } else {
                insertPlainText(QString("%1%2 %3")
                    .arg(leading)
                    .arg(bullet)
                    .arg(checkbox));
            }
            return;
        }

        if (indent > 0) {
            insertPlainText(QString(indent, ' '));
        }
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

void MdEditor::inputMethodEvent(QInputMethodEvent *event)
{
    // Pause highlighter during IME composition to avoid excessive rehighlights
    bool composing = !event->preeditString().isEmpty();
    highlighter_->setEnabled(!composing);
    QPlainTextEdit::inputMethodEvent(event);
    if (!composing) {
        highlighter_->rehighlight();
    }
}

void MdEditor::setGlobalFontPointSize(int pointSize)
{
    const int clamped = qMax(6, pointSize);
    QFont f = font();
    if (f.pointSize() == clamped)
        return;

    f.setPointSize(clamped);
    setFont(f);
    highlighter_->setBaseFontSize(clamped);
    setTabStopDistance(fontMetrics().horizontalAdvance(' ') * tabSize_);
}

int MdEditor::globalFontPointSize() const
{
    return qMax(6, font().pointSize());
}

int MdEditor::defaultFontPointSize() const
{
    return baseFontSize_;
}

void MdEditor::setFocusModeEnabled(bool enabled)
{
    if (focusModeEnabled_ == enabled)
        return;

    focusModeEnabled_ = enabled;
    highlightCurrentLine();
}

bool MdEditor::isFocusModeEnabled() const
{
    return focusModeEnabled_;
}

void MdEditor::setLineNumbersVisible(bool visible)
{
    if (lineNumbersVisible_ == visible)
        return;

    lineNumbersVisible_ = visible;
    lineNumberArea_->setVisible(visible);
    updateLineNumberAreaWidth(0);
    viewport()->update();
}

bool MdEditor::lineNumbersVisible() const
{
    return lineNumbersVisible_;
}

void MdEditor::setWordWrapEnabled(bool enabled)
{
    setWordWrapMode(enabled
        ? QTextOption::WrapAtWordBoundaryOrAnywhere
        : QTextOption::NoWrap);
}

bool MdEditor::isWordWrapEnabled() const
{
    return wordWrapMode() != QTextOption::NoWrap;
}

void MdEditor::setThemeName(const QString &themeName)
{
    themeName_ = themeName;
    Theme theme = resolveThemeByName(themeName_);

    highlighter_->setTheme(theme);
    QPalette pal = palette();
    pal.setColor(QPalette::Base, theme.background);
    pal.setColor(QPalette::Text, theme.foreground);
    pal.setColor(QPalette::Highlight, theme.selectionBg);
    pal.setColor(QPalette::HighlightedText, theme.selectionFg);
    setPalette(pal);

    currentLineBg_ = theme.currentLineBg;
    lineNumberFg_ = theme.lineNumberFg;
    lineNumberBg_ = theme.lineNumberBg;
    lineNumberArea_->update();
    highlightCurrentLine();
    viewport()->update();
}

QString MdEditor::themeName() const
{
    return themeName_;
}

void MdEditor::updateLineNumberAreaWidth(int /*newBlockCount*/)
{
    setViewportMargins(lineNumberAreaWidth(), 0, 0, 0);
}

void MdEditor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        lineNumberArea_->scroll(0, dy);
    else
        lineNumberArea_->update(0, rect.y(), lineNumberArea_->width(), rect.height());

    if (rect.contains(viewport()->rect()))
        updateLineNumberAreaWidth(0);
}

void MdEditor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    if (!isReadOnly()) {
        if (focusModeEnabled_) {
            QTextCursor current = textCursor();
            const int currentBlock = current.blockNumber();

            QColor dimColor = palette().text().color();
            dimColor.setAlphaF(0.3);

            for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
                if (block.blockNumber() == currentBlock)
                    continue;

                QTextEdit::ExtraSelection dimSel;
                dimSel.format.setForeground(dimColor);
                dimSel.cursor = QTextCursor(block);
                dimSel.cursor.select(QTextCursor::LineUnderCursor);
                extraSelections.append(dimSel);
            }
        }

        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(currentLineBg_);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = textCursor();
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }
    setExtraSelections(extraSelections);
}

void MdEditor::updateStatusStats()
{
    const QTextCursor c = textCursor();
    emit cursorPositionChanged(c.blockNumber() + 1, c.columnNumber() + 1);

    const QString text = toPlainText();
    int words = 0;
    int pos = 0;
    while (pos < text.size()) {
        while (pos < text.size() && text[pos].isSpace()) {
            ++pos;
        }
        if (pos >= text.size()) break;
        ++words;
        while (pos < text.size() && !text[pos].isSpace()) {
            ++pos;
        }
    }
    emit wordCountChanged(words, text.size());
}
