// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include "MdEditor.h"
#include "LineNumberArea.h"
#include "highlight/MdHighlighter.h"
#include "parser/BlockParser.h"
#include "config/Settings.h"
#include "config/Theme.h"
#include "util/CjkUtil.h"

#include <QFile>
#include <QTextStream>
#include <QPainter>
#include <QTextBlock>
#include <QKeyEvent>
#include <QInputMethodEvent>
#include <QFocusEvent>
#include <QScrollBar>
#include <QTextOption>
#include <QPalette>
#include <QRegularExpression>
#include <QHash>
#include <QTimer>
#include <QFontInfo>
#include <QFontDatabase>
#include <QStringConverter>
#include <QTextCharFormat>
#include <QTextFormat>

namespace {
void applyThemePalette(QPlainTextEdit *editor, const Theme &theme)
{
    QPalette pal = editor->palette();
    pal.setColor(QPalette::Base, theme.background);
    pal.setColor(QPalette::Text, theme.foreground);
    pal.setColor(QPalette::WindowText, theme.foreground);
    pal.setColor(QPalette::ButtonText, theme.foreground);
    pal.setColor(QPalette::BrightText, theme.foreground);
    pal.setColor(QPalette::Highlight, theme.selectionBg);
    pal.setColor(QPalette::HighlightedText, theme.selectionFg);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
    pal.setColor(QPalette::PlaceholderText, theme.lineNumberFg);
#endif
    editor->setPalette(pal);
}

QList<QInputMethodEvent::Attribute> normalizedPreeditAttributes(
    const QList<QInputMethodEvent::Attribute> &attributes,
    int preeditLength,
    const QColor &foreground)
{
    QList<QInputMethodEvent::Attribute> normalized;
    normalized.reserve(attributes.size() + 1);

    bool hasTextFormat = false;
    for (const QInputMethodEvent::Attribute &attr : attributes) {
        if (attr.type != QInputMethodEvent::TextFormat) {
            normalized.push_back(attr);
            continue;
        }

        hasTextFormat = true;
        QTextCharFormat fmt;
        if (attr.value.canConvert<QTextFormat>()) {
            fmt = qvariant_cast<QTextFormat>(attr.value).toCharFormat();
        } else if (attr.value.canConvert<QTextCharFormat>()) {
            fmt = qvariant_cast<QTextCharFormat>(attr.value);
        }
        fmt.setForeground(foreground);

        normalized.push_back(QInputMethodEvent::Attribute(
            QInputMethodEvent::TextFormat,
            attr.start,
            attr.length,
            fmt));
    }

    if (!hasTextFormat && preeditLength > 0) {
        QTextCharFormat fmt;
        fmt.setForeground(foreground);
        normalized.push_back(QInputMethodEvent::Attribute(
            QInputMethodEvent::TextFormat,
            0,
            preeditLength,
            fmt));
    }

    return normalized;
}
int leadingSpaceCount(const QString &line)
{
    int count = 0;
    while (count < line.size() && line[count] == QLatin1Char(' ')) {
        ++count;
    }
    return count;
}

bool isBlankLine(const QString &line)
{
    return CjkUtil::isBlankLine(line);
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
    OrderedListLineMatch match;
    if (!BlockParser::parseOrderedListLine(line, &match)) {
        return false;
    }

    if (indent) *indent = match.indent;
    if (number) *number = match.number;
    if (delimiter) *delimiter = match.delimiter;
    if (checkbox) *checkbox = match.checkbox;
    if (content) *content = match.content;
    if (numberStart) *numberStart = match.numberStart;
    if (numberLength) *numberLength = match.numberLength;
    if (contentStart) *contentStart = match.contentStart;
    return true;
}

bool matchUnorderedListLine(const QString &line,
                            int *indent = nullptr,
                            QString *marker = nullptr,
                            QString *checkbox = nullptr,
                            QString *content = nullptr,
                            int *contentStart = nullptr)
{
    UnorderedListLineMatch match;
    if (!BlockParser::parseUnorderedListLine(line, &match)) {
        return false;
    }

    if (indent) *indent = match.indent;
    if (marker) *marker = match.marker;
    if (checkbox) *checkbox = match.checkbox;
    if (content) *content = match.content;
    if (contentStart) *contentStart = match.contentStart;
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
        if (isBlankLine(line)) {
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
    while (prev.isValid() && isBlankLine(prev.text())) {
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

QTextBlock findPreviousOrderedBlockAtOrAboveIndent(const QTextBlock &block,
                                                    int maxIndent,
                                                    int *orderedIndent = nullptr)
{
    for (QTextBlock prev = block.previous(); prev.isValid(); prev = prev.previous()) {
        const QString line = prev.text();
        if (isBlankLine(line)) {
            continue;
        }

        const int lineIndent = leadingSpaceCount(line);
        if (lineIndent > maxIndent) {
            continue;
        }

        int localOrderedIndent = 0;
        if (matchOrderedListLine(line, &localOrderedIndent) && localOrderedIndent <= maxIndent) {
            if (orderedIndent) {
                *orderedIndent = localOrderedIndent;
            }
            return prev;
        }

        break;
    }

    return QTextBlock();
}

QTextBlock findNextOrderedSiblingBlockAtIndent(const QTextBlock &block, int indent)
{
    for (QTextBlock next = block.next(); next.isValid(); next = next.next()) {
        const QString line = next.text();
        if (isBlankLine(line)) {
            continue;
        }

        const int lineIndent = leadingSpaceCount(line);
        if (lineIndent > indent) {
            continue;
        }

        int orderedIndent = 0;
        if (matchOrderedListLine(line, &orderedIndent) && orderedIndent == indent) {
            return next;
        }

        break;
    }

    return QTextBlock();
}

bool hasNonSpaceTextAfterCursorInBlock(const QTextCursor &cursor)
{
    const QString line = cursor.block().text();
    const int start = cursor.positionInBlock();
    for (int i = start; i < line.size(); ++i) {
        if (!line[i].isSpace()) {
            return true;
        }
    }
    return false;
}

bool isEscapedByBackslashBeforeCursor(const QString &line, int cursorPos)
{
    if (cursorPos <= 0 || cursorPos > line.size()) {
        return false;
    }

    int slashCount = 0;
    for (int i = cursorPos - 1; i >= 0 && line[i] == QLatin1Char('\\'); --i) {
        ++slashCount;
    }
    return (slashCount % 2) == 1;
}

bool shouldAutoClosePair(const QTextCursor &cursor, QChar opener)
{
    if (cursor.hasSelection()) {
        return false;
    }
    if (hasNonSpaceTextAfterCursorInBlock(cursor)) {
        return false;
    }

    // Keep ``` input natural: after typing `` and moving over the closer,
    // a third ` should stay literal.
    if (opener == QLatin1Char('`')) {
        const QString line = cursor.block().text();
        const int pos = cursor.positionInBlock();
        if (pos > 0 && line[pos - 1] == QLatin1Char('`')) {
            return false;
        }
    }

    return true;
}

bool matchBlockquoteLine(const QString &line,
                        QString *prefix = nullptr,
                        QString *content = nullptr)
{
    BlockquoteLineMatch match;
    if (!BlockParser::parseBlockquoteLine(line, &match)) {
        return false;
    }

    if (prefix) {
        *prefix = match.prefix;
    }
    if (content) {
        *content = match.content;
    }
    return true;
}

bool parseBlockquotePrefixShape(const QString &prefix, int *indent = nullptr, int *depth = nullptr)
{
    int localIndent = 0;
    while (localIndent < prefix.size() && prefix[localIndent] == QLatin1Char(' ')) {
        ++localIndent;
    }

    int localDepth = 0;
    for (int i = localIndent; i < prefix.size(); ++i) {
        if (prefix[i] == QLatin1Char('>')) {
            ++localDepth;
        }
    }

    if (localDepth <= 0) {
        return false;
    }

    if (indent) {
        *indent = localIndent;
    }
    if (depth) {
        *depth = localDepth;
    }
    return true;
}

bool hasSameBlockquoteContextBefore(const QTextBlock &block, const QString &prefix)
{
    int indent = 0;
    int depth = 0;
    if (!parseBlockquotePrefixShape(prefix, &indent, &depth)) {
        return false;
    }

    QTextBlock prev = block.previous();
    while (prev.isValid() && isBlankLine(prev.text())) {
        prev = prev.previous();
    }
    if (!prev.isValid()) {
        return false;
    }

    QString prevPrefix;
    if (!matchBlockquoteLine(prev.text(), &prevPrefix, nullptr)) {
        return false;
    }

    int prevIndent = 0;
    int prevDepth = 0;
    if (!parseBlockquotePrefixShape(prevPrefix, &prevIndent, &prevDepth)) {
        return false;
    }

    return prevIndent == indent && prevDepth == depth;
}

bool isStandaloneLatexDisplayFenceLine(const QString &line)
{
    return BlockParser::isStandaloneLatexDisplayFence(line);
}

bool parseCodeFenceStartLine(const QString &line,
                             QChar *fenceChar = nullptr,
                             int *fenceLen = nullptr)
{
    QChar localFenceChar;
    int localFenceLen = 0;
    int indent = 0;
    QString lang;
    if (!BlockParser::matchCodeFenceStart(line, localFenceChar, localFenceLen, indent, lang)) {
        return false;
    }

    if (fenceChar) {
        *fenceChar = localFenceChar;
    }
    if (fenceLen) {
        *fenceLen = localFenceLen;
    }
    return true;
}

bool isCodeFenceEndLineForFence(const QString &line, QChar fenceChar, int fenceLen)
{
    const QString trimmed = line.trimmed();
    if (trimmed.size() < fenceLen) {
        return false;
    }

    for (QChar c : trimmed) {
        if (c != fenceChar) {
            return false;
        }
    }
    return true;
}

bool isHorizontalRuleLine(const QString &line)
{
    return BlockParser::isHorizontalRule(line);
}

bool matchLatexBeginEnvLine(const QString &line, QString *linePrefix = nullptr, QString *envName = nullptr)
{
    static const QRegularExpression re(R"(^\s*(?:> ?)*\\begin\{([^}\s]+)\}\s*$)");
    const auto m = re.match(line);
    if (!m.hasMatch()) {
        return false;
    }

    const int beginStart = line.indexOf(QStringLiteral("\\begin{"));
    if (beginStart < 0) {
        return false;
    }

    if (linePrefix) {
        *linePrefix = line.left(beginStart);
    }
    if (envName) {
        *envName = m.captured(1);
    }
    return true;
}

bool hasImmediateAutoClosedBlock(const QTextBlock &openBlock, const QString &closingLine)
{
    QTextBlock next = openBlock.next();
    if (!next.isValid()) {
        return false;
    }

    if (next.text().trimmed() == closingLine.trimmed()) {
        return true;
    }

    if (isBlankLine(next.text())) {
        const QTextBlock nextNext = next.next();
        if (nextNext.isValid() && nextNext.text().trimmed() == closingLine.trimmed()) {
            return true;
        }
    }

    return false;
}

bool isInsideStandaloneLatexDisplayBefore(const QTextDocument *doc, const QTextBlock &untilBlock)
{
    bool inDisplay = false;
    for (QTextBlock block = doc->begin(); block.isValid() && block != untilBlock; block = block.next()) {
        if (isStandaloneLatexDisplayFenceLine(block.text())) {
            inDisplay = !inDisplay;
        }
    }
    return inDisplay;
}

struct CodeFenceScanState {
    bool inFence = false;
    QChar fenceChar;
    int fenceLen = 0;
};

CodeFenceScanState codeFenceStateBefore(const QTextDocument *doc, const QTextBlock &untilBlock)
{
    CodeFenceScanState state;
    for (QTextBlock block = doc->begin(); block.isValid() && block != untilBlock; block = block.next()) {
        const QString line = block.text();

        if (state.inFence) {
            if (isCodeFenceEndLineForFence(line, state.fenceChar, state.fenceLen)) {
                state = CodeFenceScanState{};
            }
            continue;
        }

        QChar fenceChar;
        int fenceLen = 0;
        if (parseCodeFenceStartLine(line, &fenceChar, &fenceLen)) {
            state.inFence = true;
            state.fenceChar = fenceChar;
            state.fenceLen = fenceLen;
        }
    }
    return state;
}

void insertMultilineClosingFence(QPlainTextEdit *editor,
                                 const QString &currentLine,
                                 const QString &closingFence)
{
    QTextCursor cursor = editor->textCursor();
    const QString leadingSpaces(leadingSpaceCount(currentLine), QLatin1Char(' '));

    cursor.beginEditBlock();
    cursor.insertText(QString("\n%1\n%2")
        .arg(leadingSpaces, leadingSpaces + closingFence));
    cursor.movePosition(QTextCursor::Up);
    cursor.movePosition(QTextCursor::EndOfLine);
    cursor.endEditBlock();

    editor->setTextCursor(cursor);
}

void insertMultilineAutoClosedBlock(QPlainTextEdit *editor,
                                    const QString &middleLinePrefix,
                                    const QString &closingLine)
{
    QTextCursor cursor = editor->textCursor();

    cursor.beginEditBlock();
    cursor.insertText(QString("\n%1\n%2")
        .arg(middleLinePrefix, closingLine));
    cursor.movePosition(QTextCursor::Up);
    cursor.movePosition(QTextCursor::EndOfLine);
    cursor.endEditBlock();

    editor->setTextCursor(cursor);
}
}

MdEditor::MdEditor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    const Settings settings = Settings::load();
    themeName_ = settings.theme;
    tabSize_ = Settings::normalizedTabSize(settings.tabSize);

    // Initialize line number area
    lineNumberArea_ = new LineNumberArea(this);

    Theme theme = Theme::resolveByName(themeName_);

    // Initialize highlighter with configured theme.
    highlighter_ = new MdHighlighter(document(), theme);
        statusStatsTimer_ = new QTimer(this);
        statusStatsTimer_->setSingleShot(true);
        statusStatsTimer_->setInterval(150);
        connect(statusStatsTimer_, &QTimer::timeout,
            this, &MdEditor::recomputeWordCountStats);

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

    // Forward modification changed signal
    connect(document(), &QTextDocument::modificationChanged,
            this, &MdEditor::modifiedChanged);

    // Initial setup
    updateLineNumberAreaWidth(0);
    highlightCurrentLine();

    // Apply theme colors.
    applyThemePalette(this, theme);
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
    recomputeWordCountStats();
    updateStatusStats();
}

void MdEditor::loadFile(const QString &path)
{
    if (imeComposing_ || preeditLength_ > 0) {
        imeComposing_ = false;
        preeditBlockNumber_ = -1;
        preeditStart_ = -1;
        preeditLength_ = 0;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    setPlainText(in.readAll());
    file.close();

    currentFile_ = path;
    document()->setModified(false);
    emit fileChanged(path);
    recomputeWordCountStats();
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
    out.setEncoding(QStringConverter::Utf8);
    out << toPlainText();
    file.close();

    currentFile_ = savePath;
    document()->setModified(false);
    emit fileChanged(savePath);
    emit fileSaved(savePath);
    recomputeWordCountStats();
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

int MdEditor::wordCount() const
{
    return cachedWords_;
}

int MdEditor::charCount() const
{
    return cachedChars_;
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
        if (!block.isValid()) {
            break;
        }
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
    if (imeComposing_) {
        if (event->key() == Qt::Key_Return ||
            event->key() == Qt::Key_Enter ||
            event->key() == Qt::Key_Tab ||
            event->key() == Qt::Key_Backtab) {
            event->accept();
            return;
        }

        QPlainTextEdit::keyPressEvent(event);
        return;
    }

    if (handleBackspaceKey(event)) {
        return;
    }
    if (handleAutoCloseKey(event)) {
        return;
    }
    if (handleTabKey(event)) {
        return;
    }
    if (handleEnterKey(event)) {
        return;
    }

    QPlainTextEdit::keyPressEvent(event);
}

bool MdEditor::handleBackspaceKey(QKeyEvent *event)
{
    if (event->key() != Qt::Key_Backspace) {
        return false;
    }

    const Qt::KeyboardModifiers mods = event->modifiers();
    const bool hasCommandModifier = (mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
    if (hasCommandModifier) {
        return false;
    }

    QTextCursor cursor = textCursor();
    if (cursor.hasSelection()) {
        return false;
    }

    const QString line = cursor.block().text();
    const int pos = cursor.positionInBlock();
    if (pos <= 0 || pos >= line.size()) {
        return false;
    }

    const QChar left = line[pos - 1];
    const QChar right = line[pos];

    const bool isRemovablePair =
        (left == QLatin1Char('(') && right == QLatin1Char(')')) ||
        (left == QLatin1Char('[') && right == QLatin1Char(']')) ||
        (left == QLatin1Char('{') && right == QLatin1Char('}')) ||
        (left == QLatin1Char('<') && right == QLatin1Char('>')) ||
        (left == QLatin1Char('$') && right == QLatin1Char('$')) ||
        (left == QLatin1Char('`') && right == QLatin1Char('`'));

    if (!isRemovablePair) {
        return false;
    }

    // Do not collapse the opening $$ when it already has an auto-inserted
    // multiline closing fence below.
    if (left == QLatin1Char('$') && isStandaloneLatexDisplayFenceLine(line)) {
        const QString leadingSpaces(leadingSpaceCount(line), QLatin1Char(' '));
        const QString fenceLine = leadingSpaces + QStringLiteral("$$");
        if (hasImmediateAutoClosedBlock(cursor.block(), fenceLine)) {
            return false;
        }
    }

    // Keep ``` editing natural: when this `` is part of a longer run,
    // Backspace should remove one character instead of collapsing two.
    if (left == QLatin1Char('`')) {
        const bool hasBacktickBeforePair = (pos >= 2 && line[pos - 2] == QLatin1Char('`'));
        const bool hasBacktickAfterPair = (pos + 1 < line.size() && line[pos + 1] == QLatin1Char('`'));
        if (hasBacktickBeforePair || hasBacktickAfterPair) {
            return false;
        }
    }

    const int absolutePos = cursor.position();
    cursor.beginEditBlock();
    cursor.setPosition(absolutePos - 1);
    cursor.setPosition(absolutePos + 1, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
    cursor.endEditBlock();
    setTextCursor(cursor);
    return true;
}

bool MdEditor::handleAutoCloseKey(QKeyEvent *event)
{
    const Qt::KeyboardModifiers mods = event->modifiers();
    const bool hasCommandModifier = (mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier));
    if (hasCommandModifier) {
        return false;
    }

    const QString input = event->text();
    if (input.size() != 1) {
        return false;
    }

    QTextCursor cursor = textCursor();
    const QString line = cursor.block().text();
    const int pos = cursor.positionInBlock();
    const QChar typed = input[0];
    const bool escapedByBackslash = isEscapedByBackslashBeforeCursor(line, pos);

    auto skipExistingCloser = [&](QChar closer) {
        if (pos < line.size() && line[pos] == closer) {
            QTextCursor moved = cursor;
            moved.movePosition(QTextCursor::Right);
            setTextCursor(moved);
            return true;
        }
        return false;
    };

    if (!escapedByBackslash) {
        if (typed == QLatin1Char(')') ||
            typed == QLatin1Char(']') ||
            typed == QLatin1Char('}') ||
            typed == QLatin1Char('>') ||
            typed == QLatin1Char('$') ||
            typed == QLatin1Char('`')) {
            if (skipExistingCloser(typed)) {
                return true;
            }
        }
    }

    QChar closer;
    bool supportsAutoClose = true;
    if (typed == QLatin1Char('(')) {
        closer = QLatin1Char(')');
    } else if (typed == QLatin1Char('[')) {
        closer = QLatin1Char(']');
    } else if (typed == QLatin1Char('{')) {
        closer = QLatin1Char('}');
    } else if (typed == QLatin1Char('<')) {
        closer = QLatin1Char('>');
    } else if (typed == QLatin1Char('$')) {
        closer = QLatin1Char('$');
    } else if (typed == QLatin1Char('`')) {
        closer = QLatin1Char('`');
    } else {
        supportsAutoClose = false;
    }

    if (!escapedByBackslash && supportsAutoClose && shouldAutoClosePair(cursor, typed)) {
        cursor.beginEditBlock();
        cursor.insertText(QString(typed) + QString(closer));
        cursor.movePosition(QTextCursor::Left);
        cursor.endEditBlock();
        setTextCursor(cursor);
        return true;
    }

    return false;
}

bool MdEditor::handleTabKey(QKeyEvent *event)
{
    if (event->key() != Qt::Key_Tab && event->key() != Qt::Key_Backtab) {
        return false;
    }

    const bool indentForward = (event->key() == Qt::Key_Tab);
    if (handleListIndentationKey(indentForward)) {
        return true;
    }

    if (indentForward) {
        insertPlainText(QString(tabSize_, QLatin1Char(' ')));
        return true;
    }

    return false;
}

bool MdEditor::handleEnterKey(QKeyEvent *event)
{
    if (event->key() != Qt::Key_Return && event->key() != Qt::Key_Enter) {
        return false;
    }

    if (imeComposing_) {
        return false;
    }

    if (event->modifiers().testFlag(Qt::ShiftModifier)) {
        QTextCursor plainEnterCursor = textCursor();
        plainEnterCursor.beginEditBlock();
        if (plainEnterCursor.hasSelection()) {
            plainEnterCursor.removeSelectedText();
        }
        plainEnterCursor.insertBlock();
        plainEnterCursor.endEditBlock();
        setTextCursor(plainEnterCursor);
        return true;
    }

    QTextCursor cursor = textCursor();
    QString currentLine = cursor.block().text();

    if (cursor.positionInBlock() == currentLine.size()) {
        QString latexPrefix;
        QString latexEnv;
        if (matchLatexBeginEnvLine(currentLine, &latexPrefix, &latexEnv)) {
            const QString closingLine = latexPrefix + QStringLiteral("\\end{%1}").arg(latexEnv);
            if (!hasImmediateAutoClosedBlock(cursor.block(), closingLine)) {
                insertMultilineAutoClosedBlock(this, latexPrefix, closingLine);
                return true;
            }
        }

        const QString leadingSpaces(leadingSpaceCount(currentLine), QLatin1Char(' '));

        bool latexKnown = false;
        const bool insideLatexFromContext = highlighter_->blockStartsInsideLatexDisplay(cursor.block(), &latexKnown);
        const bool insideLatexDisplay = latexKnown
            ? insideLatexFromContext
            : isInsideStandaloneLatexDisplayBefore(document(), cursor.block());

        if (isStandaloneLatexDisplayFenceLine(currentLine) &&
            !insideLatexDisplay &&
            !hasImmediateAutoClosedBlock(cursor.block(), leadingSpaces + QStringLiteral("$$"))) {
            insertMultilineClosingFence(this, currentLine, QStringLiteral("$$"));
            return true;
        }

        bool fenceKnown = false;
        const bool insideFenceFromContext = highlighter_->blockStartsInsideCodeFence(cursor.block(), &fenceKnown);
        const CodeFenceScanState fallbackFenceState = codeFenceStateBefore(document(), cursor.block());
        const bool insideCodeFence = fenceKnown
            ? insideFenceFromContext
            : fallbackFenceState.inFence;

        QChar openingFenceChar;
        int openingFenceLen = 0;
        if (parseCodeFenceStartLine(currentLine, &openingFenceChar, &openingFenceLen) &&
            !insideCodeFence) {
            const QString closingFence(openingFenceLen, openingFenceChar);
            if (!hasImmediateAutoClosedBlock(cursor.block(), leadingSpaces + closingFence)) {
                insertMultilineClosingFence(this, currentLine, closingFence);
                return true;
            }
        }
    }

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
    const bool unorderedCandidate = !ordered &&
        !isHorizontalRuleLine(currentLine) &&
        matchUnorderedListLine(
            currentLine,
            &unorderedIndent,
            &unorderedMarker,
            &unorderedCheckbox,
            &unorderedContent,
            &unorderedContentStart
        );
    const bool unordered = unorderedCandidate;

    const int paragraphIndent = leadingSpaceCount(currentLine);

    QString blockquotePrefix;
    QString blockquoteContent;
    const bool blockquote = !ordered && !unordered && matchBlockquoteLine(
        currentLine,
        &blockquotePrefix,
        &blockquoteContent
    );

    const bool emptyListItem = (ordered && orderedContent.trimmed().isEmpty())
        || (unordered && unorderedContent.trimmed().isEmpty());
    const bool emptyBlockquote = blockquote && blockquoteContent.trimmed().isEmpty();

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

    auto clearCurrentEmptyBlockquotePrefix = [&]() {
        if (!emptyBlockquote) {
            return false;
        }
        if (!hasSameBlockquoteContextBefore(cursor.block(), blockquotePrefix)) {
            return false;
        }

        const int blockStart = cursor.block().position();
        QTextCursor editCursor = cursor;
        editCursor.setPosition(blockStart);
        editCursor.setPosition(blockStart + blockquotePrefix.length(), QTextCursor::KeepAnchor);
        editCursor.removeSelectedText();
        editCursor.clearSelection();
        editCursor.setPosition(blockStart);
        setTextCursor(editCursor);
        return true;
    };

    const bool shouldExitByEnter = emptyListItem && (
        (ordered && hasSameTypeListContextBefore(cursor.block(), true, orderedIndent))
        || (unordered && hasSameTypeListContextBefore(cursor.block(), false, unorderedIndent))
    );

    if (shouldExitByEnter && clearCurrentEmptyListMarker()) {
        if (ordered) {
            renumberOrderedListsAroundBlock(textCursor().block());
        }
        return true;
    }

    if (clearCurrentEmptyBlockquotePrefix()) {
        return true;
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
        renumberOrderedListsAroundBlock(c.block());
        restoreCursorInBlock(blockNumber, column);
        return true;
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
        return true;
    }

    if (blockquote) {
        if (!blockquotePrefix.endsWith(QLatin1Char(' '))) {
            blockquotePrefix += QLatin1Char(' ');
        }
        insertPlainText(blockquotePrefix);
        return true;
    }

    if (paragraphIndent > 0) {
        insertPlainText(QString(paragraphIndent, QLatin1Char(' ')));
    }

    return true;
}

void MdEditor::inputMethodEvent(QInputMethodEvent *event)
{
    const bool composing = !event->preeditString().isEmpty();

    if (composing) {
        imeComposing_ = true;

        QTextCursor cursor = textCursor();
        // Always normalize composing text color to editor foreground.
        QTextCharFormat cleanFmt;
        cleanFmt.setForeground(palette().color(QPalette::Text));
        setCurrentCharFormat(cleanFmt);

        preeditBlockNumber_ = cursor.blockNumber();
        preeditStart_ = qMax(0, cursor.positionInBlock() + event->replacementStart());
        preeditLength_ = event->preeditString().size();

        const auto attrs = normalizedPreeditAttributes(
            event->attributes(),
            event->preeditString().size(),
            palette().color(QPalette::Text));

        QInputMethodEvent normalizedEvent(event->preeditString(), attrs);
        normalizedEvent.setCommitString(
            event->commitString(),
            event->replacementStart(),
            event->replacementLength());
        QPlainTextEdit::inputMethodEvent(&normalizedEvent);
        return;
    }

    QPlainTextEdit::inputMethodEvent(event);
    imeComposing_ = false;
    preeditBlockNumber_ = -1;
    preeditStart_ = -1;
    preeditLength_ = 0;
}

void MdEditor::focusOutEvent(QFocusEvent *event)
{
    if (imeComposing_ || preeditLength_ > 0) {
        imeComposing_ = false;
        preeditBlockNumber_ = -1;
        preeditStart_ = -1;
        preeditLength_ = 0;
    }

    QPlainTextEdit::focusOutEvent(event);
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
    Theme theme = Theme::resolveByName(themeName_);

    highlighter_->setTheme(theme);
    applyThemePalette(this, theme);

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

    QTextBlock renumberStart = document()->findBlockByNumber(startBlockNumber);
    QTextBlock renumberEnd = document()->findBlockByNumber(endBlockNumber);
    renumberOrderedListsAroundBlock(renumberStart);
    if (renumberEnd.isValid() && renumberEnd.blockNumber() != renumberStart.blockNumber()) {
        renumberOrderedListsAroundBlock(renumberEnd);
    }

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

void MdEditor::renumberOrderedListsAroundBlock(const QTextBlock &anchorBlock)
{
    if (!anchorBlock.isValid()) {
        return;
    }

    QTextBlock seed = anchorBlock;
    if (!matchOrderedListLine(seed.text())) {
        QTextBlock prev = seed.previous();
        while (prev.isValid() && isBlankLine(prev.text())) {
            prev = prev.previous();
        }

        if (prev.isValid() && matchOrderedListLine(prev.text())) {
            seed = prev;
        } else {
            QTextBlock next = seed.next();
            while (next.isValid() && isBlankLine(next.text())) {
                next = next.next();
            }
            if (!next.isValid() || !matchOrderedListLine(next.text())) {
                return;
            }
            seed = next;
        }
    }

    int seedIndent = 0;
    if (!matchOrderedListLine(seed.text(), &seedIndent)) {
        return;
    }

    int rootIndent = seedIndent;
    QTextBlock firstItem = seed;
    const int maxBacktrackSteps = qMax(1, document()->blockCount());
    bool reachedBacktrackEnd = false;
    for (int step = 0; step < maxBacktrackSteps; ++step) {
        int prevIndent = 0;
        const QTextBlock prevItem = findPreviousOrderedBlockAtOrAboveIndent(firstItem, rootIndent, &prevIndent);
        if (!prevItem.isValid()) {
            reachedBacktrackEnd = true;
            break;
        }

        if (prevItem.blockNumber() >= firstItem.blockNumber() || prevIndent > rootIndent) {
            reachedBacktrackEnd = true;
            break;
        }

        firstItem = prevItem;
        rootIndent = prevIndent;
    }
    if (!reachedBacktrackEnd) {
        return;
    }

    QTextBlock lastItem = firstItem;
    const int maxForwardSteps = qMax(1, document()->blockCount());
    bool reachedForwardEnd = false;
    for (int step = 0; step < maxForwardSteps; ++step) {
        const QTextBlock subtreeEnd = findListSubtreeEnd(lastItem);
        const QTextBlock nextSibling = findNextOrderedSiblingBlockAtIndent(subtreeEnd, rootIndent);
        if (!nextSibling.isValid()) {
            reachedForwardEnd = true;
            break;
        }

        if (nextSibling.blockNumber() <= lastItem.blockNumber()) {
            reachedForwardEnd = true;
            break;
        }

        lastItem = nextSibling;
    }
    if (!reachedForwardEnd) {
        return;
    }

    QTextBlock start = firstItem;
    QTextBlock end = findListSubtreeEnd(lastItem);

    while (end.isValid() && isBlankLine(end.text())) {
        if (end == start) {
            return;
        }
        end = end.previous();
    }

    renumberOrderedListRange(start, end);
}

void MdEditor::renumberOrderedListRange(const QTextBlock &startBlock,
                                        const QTextBlock &endBlock)
{
    if (!startBlock.isValid() || !endBlock.isValid()) {
        return;
    }

    QHash<int, int> nextNumberByIndent;
    QTextCursor editCursor(document());
    editCursor.beginEditBlock();

    for (QTextBlock block = startBlock; block.isValid(); block = block.next()) {
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

            for (auto it = nextNumberByIndent.begin(); it != nextNumberByIndent.end(); ) {
                if (it.key() > indent) {
                    it = nextNumberByIndent.erase(it);
                } else {
                    ++it;
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

            if (block == endBlock) {
                break;
            }
            continue;
        }

        if (isBlankLine(line)) {
            if (block == endBlock) {
                break;
            }
            continue;
        }

        const int lineIndent = leadingSpaceCount(line);
        for (auto it = nextNumberByIndent.begin(); it != nextNumberByIndent.end(); ) {
            if (it.key() >= lineIndent) {
                it = nextNumberByIndent.erase(it);
            } else {
                ++it;
            }
        }

        if (block == endBlock) {
            break;
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
            const QRect visibleRect = viewport()->rect();

            QColor dimColor = palette().text().color();
            dimColor.setAlphaF(0.3);

            QTextBlock block = firstVisibleBlock();
            int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
            int bottom = top + qRound(blockBoundingRect(block).height());

            while (block.isValid() && top <= visibleRect.bottom()) {
                if (block.isVisible() && bottom >= visibleRect.top() && block.blockNumber() != currentBlock) {
                    QTextEdit::ExtraSelection dimSel;
                    dimSel.format.setForeground(dimColor);
                    dimSel.cursor = QTextCursor(block);
                    dimSel.cursor.select(QTextCursor::LineUnderCursor);
                    extraSelections.append(dimSel);
                }

                block = block.next();
                top = bottom;
                if (!block.isValid()) {
                    break;
                }
                bottom = top + qRound(blockBoundingRect(block).height());
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

    if (cachedDocRevision_ != document()->revision() && statusStatsTimer_) {
        statusStatsTimer_->start();
    }

    emitWordCountIfChanged();
}

void MdEditor::recomputeWordCountStats()
{
    const int revision = document()->revision();
    if (cachedDocRevision_ == revision) {
        emitWordCountIfChanged();
        return;
    }

    const QString text = toPlainText();
    auto isCountedCjkWordChar = [](QChar c) {
        return CjkUtil::isCjk(c) && !c.isSpace() && !c.isPunct();
    };

    int words = 0;
    int pos = 0;
    while (pos < text.size()) {
        while (pos < text.size() && (text[pos].isSpace() || text[pos].isPunct())) {
            ++pos;
        }
        if (pos >= text.size()) {
            break;
        }

        if (isCountedCjkWordChar(text[pos])) {
            ++words;
            ++pos;
            continue;
        }

        ++words;
        ++pos;
        while (pos < text.size() &&
               !text[pos].isSpace() &&
               !text[pos].isPunct() &&
               !isCountedCjkWordChar(text[pos])) {
            ++pos;
        }
    }

    cachedWords_ = words;
    cachedChars_ = text.size();
    cachedDocRevision_ = revision;
    emitWordCountIfChanged();
}

void MdEditor::emitWordCountIfChanged()
{
    if (cachedWords_ != emittedWords_ || cachedChars_ != emittedChars_) {
        emittedWords_ = cachedWords_;
        emittedChars_ = cachedChars_;
        emit wordCountChanged(cachedWords_, cachedChars_);
    }
}
