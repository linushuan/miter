// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

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
#include <QMap>
#include <QFontInfo>
#include <QFontDatabase>

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

int leadingSpaceCount(const QString &line)
{
    int count = 0;
    while (count < line.size() && line[count] == QLatin1Char(' ')) {
        ++count;
    }
    return count;
}

bool matchOrderedListLine(const QString &line,
                          int *indent = nullptr,
                          int *number = nullptr,
                          QString *delimiter = nullptr,
                          QString *checkbox = nullptr,
                          QString *content = nullptr,
                          int *numberStart = nullptr,
                          int *numberLength = nullptr,
                          int *contentStart = nullptr)
{
    static const QRegularExpression re(
        R"(^(\s*)(\d+)([.)])(\s+)(\[[ xX]\]\s+)?(.*)$)");

    const auto m = re.match(line);
    if (!m.hasMatch()) {
        return false;
    }

    if (indent) *indent = m.captured(1).length();
    if (number) *number = m.captured(2).toInt();
    if (delimiter) *delimiter = m.captured(3);
    if (checkbox) *checkbox = m.captured(5);
    if (content) *content = m.captured(6);
    if (numberStart) *numberStart = m.capturedStart(2);
    if (numberLength) *numberLength = m.capturedLength(2);
    if (contentStart) *contentStart = m.capturedStart(6);
    return true;
}

bool matchUnorderedListLine(const QString &line,
                            int *indent = nullptr,
                            QString *marker = nullptr,
                            QString *checkbox = nullptr,
                            QString *content = nullptr,
                            int *contentStart = nullptr)
{
    static const QRegularExpression re(
        R"(^(\s*)([-*+])(\s+)(\[[ xX]\]\s+)?(.*)$)");

    const auto m = re.match(line);
    if (!m.hasMatch()) {
        return false;
    }

    if (indent) *indent = m.captured(1).length();
    if (marker) *marker = m.captured(2);
    if (checkbox) *checkbox = m.captured(4);
    if (content) *content = m.captured(5);
    if (contentStart) *contentStart = m.capturedStart(5);
    return true;
}

bool isListLine(const QString &line)
{
    return matchOrderedListLine(line) || matchUnorderedListLine(line);
}

bool isEmptyListItemLine(const QString &line)
{
    int indent = 0;
    int number = 0;
    QString delimiter;
    QString checkbox;
    QString content;
    if (matchOrderedListLine(line, &indent, &number, &delimiter, &checkbox, &content)) {
        return content.trimmed().isEmpty();
    }

    QString marker;
    if (matchUnorderedListLine(line, &indent, &marker, &checkbox, &content)) {
        return content.trimmed().isEmpty();
    }

    return false;
}

QTextBlock findListSubtreeEnd(const QTextBlock &start)
{
    QTextBlock end = start;
    const int baseIndent = leadingSpaceCount(start.text());

    for (QTextBlock block = start.next(); block.isValid(); block = block.next()) {
        const QString line = block.text();
        if (line.trimmed().isEmpty()) {
            end = block;
            continue;
        }

        const int indent = leadingSpaceCount(line);
        if (indent <= baseIndent) {
            break;
        }
        end = block;
    }

    return end;
}

bool rangeContainsListLine(const QTextBlock &start, const QTextBlock &end)
{
    for (QTextBlock block = start; block.isValid(); block = block.next()) {
        if (isListLine(block.text())) {
            return true;
        }
        if (block == end) {
            break;
        }
    }
    return false;
}

bool hasSameTypeListContextBefore(const QTextBlock &block, bool ordered, int indent)
{
    QTextBlock prev = block.previous();
    while (prev.isValid() && prev.text().trimmed().isEmpty()) {
        prev = prev.previous();
    }

    if (!prev.isValid()) {
        return false;
    }

    int prevIndent = 0;
    if (ordered) {
        int prevNumber = 0;
        QString prevDelimiter;
        if (!matchOrderedListLine(prev.text(), &prevIndent, &prevNumber, &prevDelimiter)) {
            return false;
        }
    } else {
        QString prevMarker;
        if (!matchUnorderedListLine(prev.text(), &prevIndent, &prevMarker)) {
            return false;
        }
    }

    return prevIndent == indent;
}
}

MdEditor::MdEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    const Settings settings = Settings::load();
    themeName_ = settings.theme;
    tabSize_ = 2;

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
    applyEditorFont(settings.fontFamily, settings.fontSize);
    baseFontSize_ = qMax(6, settings.fontSize);
    highlighter_->setBaseFontSize(baseFontSize_);
    setWordWrapEnabled(settings.wordWrap);
    setLineNumbersVisible(settings.lineNumbers);

    // Tab -> configurable spaces
    setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char('M')) * tabSize_);
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
    if (event->key() == Qt::Key_Tab || event->key() == Qt::Key_Backtab) {
        const bool indentForward = (event->key() == Qt::Key_Tab);

        if (handleListIndentationKey(indentForward)) {
            return;
        }

        if (indentForward) {
            insertPlainText(QString(tabSize_, QLatin1Char(' ')));
            return;
        }
    }

    // Auto-indent on Enter
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        const bool shiftEnter = event->modifiers().testFlag(Qt::ShiftModifier);
        QTextCursor cursor = textCursor();
        QString currentLine = cursor.block().text();
        int orderedIndent = 0;
        int orderedNumber = 0;
        QString orderedDelimiter;
        QString orderedCheckbox;
        QString orderedContent;
        int orderedContentStart = 0;
        const bool ordered = matchOrderedListLine(
            currentLine,
            &orderedIndent,
            &orderedNumber,
            &orderedDelimiter,
            &orderedCheckbox,
            &orderedContent,
            nullptr,
            nullptr,
            &orderedContentStart
        );

        int unorderedIndent = 0;
        QString unorderedMarker;
        QString unorderedCheckbox;
        QString unorderedContent;
        int unorderedContentStart = 0;
        const bool unordered = !ordered && matchUnorderedListLine(
            currentLine,
            &unorderedIndent,
            &unorderedMarker,
            &unorderedCheckbox,
            &unorderedContent,
            &unorderedContentStart
        );

        const int paragraphIndent = leadingSpaceCount(currentLine);
        const bool emptyListItem = (ordered && orderedContent.trimmed().isEmpty())
            || (unordered && unorderedContent.trimmed().isEmpty());

        auto clearCurrentEmptyListMarker = [&]() {
            if (!emptyListItem) {
                return false;
            }

            int startInBlock = 0;
            int endInBlock = 0;
            if (ordered) {
                startInBlock = orderedIndent;
                endInBlock = orderedContentStart;
            } else if (unordered) {
                startInBlock = unorderedIndent;
                endInBlock = unorderedContentStart;
            } else {
                return false;
            }

            const int blockStart = cursor.block().position();
            QTextCursor editCursor = cursor;
            editCursor.setPosition(blockStart + startInBlock);
            editCursor.setPosition(blockStart + endInBlock, QTextCursor::KeepAnchor);
            editCursor.removeSelectedText();
            editCursor.clearSelection();
            editCursor.setPosition(blockStart + startInBlock);
            setTextCursor(editCursor);
            return true;
        };

        if (shiftEnter) {
            if (clearCurrentEmptyListMarker()) {
                if (ordered) {
                    renumberOrderedLists();
                }
                return;
            }
            QPlainTextEdit::keyPressEvent(event);
            return;
        }

        const bool shouldExitByEnter = emptyListItem && (
            (ordered && hasSameTypeListContextBefore(cursor.block(), true, orderedIndent))
            || (unordered && hasSameTypeListContextBefore(cursor.block(), false, unorderedIndent))
        );

        if (shouldExitByEnter && clearCurrentEmptyListMarker()) {
            if (ordered) {
                renumberOrderedLists();
            }
            return;
        }

        QPlainTextEdit::keyPressEvent(event);

        if (ordered) {
            const QString leading(orderedIndent, QLatin1Char(' '));
            const QString checkbox = orderedCheckbox;
            insertPlainText(QString("%1%2%3 %4")
                .arg(leading)
                .arg(orderedNumber + 1)
                .arg(orderedDelimiter)
                .arg(checkbox));

            const QTextCursor c = textCursor();
            const int blockNumber = c.blockNumber();
            const int column = c.positionInBlock();
            renumberOrderedLists();
            restoreCursorInBlock(blockNumber, column);
            return;
        }

        if (unordered) {
            const QString leading(unorderedIndent, QLatin1Char(' '));
            const QString bullet = unorderedMarker;
            const QString checkbox = unorderedCheckbox;
            Q_UNUSED(unorderedContent);
            insertPlainText(QString("%1%2 %3")
                .arg(leading)
                .arg(bullet)
                .arg(checkbox));
            return;
        }

        if (paragraphIndent > 0) {
            insertPlainText(QString(paragraphIndent, QLatin1Char(' ')));
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
    if (font().pointSize() == clamped)
        return;

    applyEditorFont(font().family(), clamped);
    highlighter_->setBaseFontSize(clamped);
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

bool MdEditor::handleListIndentationKey(bool indentForward)
{
    QTextCursor cursor = textCursor();
    QTextBlock startBlock;
    QTextBlock endBlock;
    const bool hasSelection = cursor.hasSelection();

    if (hasSelection) {
        const int selectionStart = cursor.selectionStart();
        const int selectionEnd = cursor.selectionEnd();
        startBlock = document()->findBlock(selectionStart);
        endBlock = document()->findBlock(qMax(selectionStart, selectionEnd - 1));

        if (!startBlock.isValid() || !endBlock.isValid()) {
            return false;
        }
        if (!rangeContainsListLine(startBlock, endBlock)) {
            return false;
        }
    } else {
        startBlock = cursor.block();
        if (!isListLine(startBlock.text())) {
            return false;
        }
        if (!isEmptyListItemLine(startBlock.text())) {
            return false;
        }
        endBlock = findListSubtreeEnd(startBlock);
    }

    const int startBlockNumber = startBlock.blockNumber();
    const int endBlockNumber = endBlock.blockNumber();
    const int cursorBlockNumber = cursor.blockNumber();
    const int cursorColumn = cursor.positionInBlock();
    int cursorDelta = 0;

    QTextCursor editCursor(document());
    editCursor.beginEditBlock();

    for (int blockNumber = startBlockNumber; blockNumber <= endBlockNumber; ++blockNumber) {
        QTextBlock block = document()->findBlockByNumber(blockNumber);
        if (!block.isValid()) {
            break;
        }

        const QString line = block.text();
        if (indentForward) {
            editCursor.setPosition(block.position());
            editCursor.insertText(QString(tabSize_, QLatin1Char(' ')));
            if (blockNumber == cursorBlockNumber) {
                cursorDelta = tabSize_;
            }
        } else {
            int removeCount = 0;
            while (removeCount < tabSize_ &&
                   removeCount < line.size() &&
                   line[removeCount] == QLatin1Char(' ')) {
                ++removeCount;
            }

            if (removeCount > 0) {
                editCursor.setPosition(block.position());
                editCursor.setPosition(block.position() + removeCount, QTextCursor::KeepAnchor);
                editCursor.removeSelectedText();
            }

            if (blockNumber == cursorBlockNumber) {
                cursorDelta = -removeCount;
            }
        }
    }

    editCursor.endEditBlock();

    renumberOrderedLists();

    if (hasSelection) {
        QTextBlock newStart = document()->findBlockByNumber(startBlockNumber);
        QTextBlock newEnd = document()->findBlockByNumber(endBlockNumber);

        QTextCursor selected(document());
        selected.setPosition(newStart.position());
        selected.setPosition(newEnd.position() + qMax(0, newEnd.length() - 1), QTextCursor::KeepAnchor);
        setTextCursor(selected);
    } else {
        restoreCursorInBlock(cursorBlockNumber, cursorColumn + cursorDelta);
    }

    updateStatusStats();
    return true;
}

void MdEditor::renumberOrderedLists()
{
    QMap<int, int> nextNumberByIndent;
    QTextCursor editCursor(document());
    editCursor.beginEditBlock();

    for (QTextBlock block = document()->begin(); block.isValid(); block = block.next()) {
        const QString line = block.text();
        int indent = 0;
        int currentNumber = 0;
        int numberStart = 0;
        int numberLength = 0;
        QString delimiter;

        if (matchOrderedListLine(
                line,
                &indent,
                &currentNumber,
                &delimiter,
                nullptr,
                nullptr,
                &numberStart,
                &numberLength)) {

            const QList<int> keys = nextNumberByIndent.keys();
            for (int key : keys) {
                if (key > indent) {
                    nextNumberByIndent.remove(key);
                }
            }

            const int expectedNumber = nextNumberByIndent.value(indent, 1);
            if (currentNumber != expectedNumber) {
                const int absStart = block.position() + numberStart;
                editCursor.setPosition(absStart);
                editCursor.setPosition(absStart + numberLength, QTextCursor::KeepAnchor);
                editCursor.insertText(QString::number(expectedNumber));
            }
            nextNumberByIndent[indent] = expectedNumber + 1;
            continue;
        }

        if (line.trimmed().isEmpty()) {
            continue;
        }

        const int lineIndent = leadingSpaceCount(line);
        const QList<int> keys = nextNumberByIndent.keys();
        for (int key : keys) {
            if (key >= lineIndent) {
                nextNumberByIndent.remove(key);
            }
        }
    }

    editCursor.endEditBlock();
}

void MdEditor::restoreCursorInBlock(int blockNumber, int column)
{
    QTextBlock block = document()->findBlockByNumber(blockNumber);
    if (!block.isValid()) {
        return;
    }

    const int clampedColumn = qBound(0, column, qMax(0, block.length() - 1));
    QTextCursor cursor(document());
    cursor.setPosition(block.position() + clampedColumn);
    setTextCursor(cursor);
}

void MdEditor::applyEditorFont(const QString &family, int pointSize)
{
    QFont f(family, qMax(6, pointSize));
    f.setStyleHint(QFont::TypeWriter);
    f.setFixedPitch(true);

    if (!QFontInfo(f).fixedPitch()) {
        f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        f.setPointSize(qMax(6, pointSize));
        f.setStyleHint(QFont::TypeWriter);
        f.setFixedPitch(true);
    }

    setFont(f);

    const int cellWidth = qMax(
        fontMetrics().horizontalAdvance(QLatin1Char('M')),
        fontMetrics().horizontalAdvance(QLatin1Char(' '))
    );
    setTabStopDistance(cellWidth * tabSize_);
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

    const int revision = document()->revision();
    if (cachedDocRevision_ != revision) {
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

        cachedWords_ = words;
        cachedChars_ = text.size();
        cachedDocRevision_ = revision;
    }

    if (cachedWords_ != emittedWords_ || cachedChars_ != emittedChars_) {
        emittedWords_ = cachedWords_;
        emittedChars_ = cachedChars_;
        emit wordCountChanged(cachedWords_, cachedChars_);
    }
}
