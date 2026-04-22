// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include "MdHighlighter.h"
#include "parser/BlockParser.h"
#include "parser/InlineParser.h"
#include "parser/LatexParser.h"

#include <QTextBlock>
#include <QTextBlockUserData>
#include <QTextDocument>
#include <QRegularExpression>
#include <QMetaObject>
#include <QTimer>
#include <QDateTime>

// Store ContextStack in QTextBlockUserData
class ContextBlockData : public QTextBlockUserData {
public:
    ContextBlockData(const ContextStack &ctx) : ctx_(ctx) {}
    ContextStack ctx_;
};

static bool looksLikeSetextMutationCandidate(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    const QChar marker = trimmed.front();
    if (marker != QLatin1Char('-') &&
        marker != QLatin1Char('*') &&
        marker != QLatin1Char('=')) {
        return false;
    }

    int markerCount = 0;
    for (const QChar ch : trimmed) {
        if (ch == marker) {
            ++markerCount;
            continue;
        }
        if (ch.isSpace()) {
            continue;
        }
        return markerCount >= 2;
    }

    return markerCount >= 2;
}

static bool isListLine(const QString &text)
{
    return BlockParser::parseOrderedListLine(text) ||
           BlockParser::parseUnorderedListLine(text);
}

static bool couldBeSetextHeadingTextLine(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    if (BlockParser::isSetextH1Underline(text) ||
        BlockParser::isSetextH2Underline(text)) {
        return false;
    }

    if (BlockParser::isHorizontalRule(text) || isListLine(text)) {
        return false;
    }

    return true;
}

bool MdHighlighter::blockStartsInsideLatexDisplay(const QTextBlock &block, bool *known) const
{
    return blockStartsInsideState(block, BlockState::LatexDisplay, known);
}

bool MdHighlighter::blockStartsInsideCodeFence(const QTextBlock &block, bool *known) const
{
    return blockStartsInsideState(block, BlockState::CodeFence, known);
}

bool MdHighlighter::blockStartsInsideState(const QTextBlock &block,
                                           BlockState state,
                                           bool *known) const
{
    const QTextBlock prevBlock = block.previous();
    if (!prevBlock.isValid()) {
        if (known) {
            *known = true;
        }
        return false;
    }

    auto *data = dynamic_cast<ContextBlockData *>(prevBlock.userData());
    if (!data) {
        if (known) {
            *known = false;
        }
        return false;
    }

    if (known) {
        *known = true;
    }
    return data->ctx_.topState() == state;
}

MdHighlighter::MdHighlighter(QTextDocument *parent, const Theme &theme)
    : QSyntaxHighlighter(parent)
    , theme_(theme)
{
    tableRefreshTimer_ = new QTimer(this);
    tableRefreshTimer_->setSingleShot(true);
    connect(tableRefreshTimer_, &QTimer::timeout, this, [this]() {
        if (tableSyncInProgress_ || rehighlightInProgress_) {
            tableRefreshTimer_->start(tableRefreshDebounceMs_);
            return;
        }

        rehighlightInProgress_ = true;
        tableSyncInProgress_ = true;
        tableRefreshPending_ = false;
        rehighlight();
        tableSyncInProgress_ = false;
        rehighlightInProgress_ = false;
        lastTableRefreshMs_ = QDateTime::currentMSecsSinceEpoch();
    });

    buildFormats();
}

void MdHighlighter::setTheme(const Theme &theme)
{
    theme_ = theme;
    buildFormats();
    rehighlight();
}

void MdHighlighter::setEnabled(bool enabled)
{
    if (enabled_ == enabled)
        return;

    enabled_ = enabled;
    if (enabled_) {
        // Ensure blocks changed during IME composition get refreshed.
        rehighlight();
    }
}

void MdHighlighter::setBaseFontSize(int pointSize)
{
    const int clamped = qMax(6, pointSize);
    if (baseFontSize_ == clamped)
        return;

    baseFontSize_ = clamped;
    buildFormats();
    rehighlight();
}

void MdHighlighter::setPreeditRange(int blockNumber, int startInBlock, int length)
{
    const int clampedStart = qMax(0, startInBlock);
    const int clampedLength = qMax(0, length);

    const int previousBlockNumber = preeditBlockNumber_;
    const int previousStart = preeditStartInBlock_;
    const int previousLength = preeditLength_;
    if (preeditBlockNumber_ == blockNumber &&
        preeditStartInBlock_ == clampedStart &&
        preeditLength_ == clampedLength) {
        return;
    }

    preeditBlockNumber_ = blockNumber;
    preeditStartInBlock_ = clampedStart;
    preeditLength_ = clampedLength;

    QTextDocument *doc = document();
    if (!doc) {
        return;
    }

    if (previousBlockNumber >= 0) {
        const QTextBlock previousBlock = doc->findBlockByNumber(previousBlockNumber);
        if (previousBlock.isValid()) {
            rehighlightBlock(previousBlock);
        }
    }

    if (blockNumber >= 0 &&
        (blockNumber != previousBlockNumber ||
         clampedStart != previousStart ||
         clampedLength != previousLength)) {
        const QTextBlock composingBlock = doc->findBlockByNumber(blockNumber);
        if (composingBlock.isValid()) {
            rehighlightBlock(composingBlock);
        }
    }
}

void MdHighlighter::clearPreeditRange()
{
    if (preeditBlockNumber_ < 0 && preeditStartInBlock_ < 0 && preeditLength_ == 0) {
        return;
    }

    const int previousBlockNumber = preeditBlockNumber_;
    preeditBlockNumber_ = -1;
    preeditStartInBlock_ = -1;
    preeditLength_ = 0;

    QTextDocument *doc = document();
    if (!doc || previousBlockNumber < 0) {
        return;
    }

    const QTextBlock previousBlock = doc->findBlockByNumber(previousBlockNumber);
    if (previousBlock.isValid()) {
        rehighlightBlock(previousBlock);
    }
}

void MdHighlighter::setComposingPosition(int blockNumber, int posInBlock)
{
    setPreeditRange(blockNumber, posInBlock, 0);
}

void MdHighlighter::clearComposingPosition()
{
    clearPreeditRange();
}

void MdHighlighter::runPendingSetextRefresh()
{
    if (!setextRefreshPending_) {
        return;
    }

    if (rehighlightInProgress_ || setextSyncInProgress_) {
        QMetaObject::invokeMethod(this, [this]() {
            runPendingSetextRefresh();
        }, Qt::QueuedConnection);
        return;
    }

    rehighlightInProgress_ = true;
    setextSyncInProgress_ = true;
    setextRefreshPending_ = false;
    rehighlight();
    setextSyncInProgress_ = false;
    rehighlightInProgress_ = false;
}

void MdHighlighter::highlightBlock(const QString &text)
{
    // Always restore/classify/save context so incremental block state stays
    // consistent even when visual highlighting is temporarily paused.
    ContextStack ctx = restoreContext();

    if (!enabled_) {
        const QTextBlock prevLineBlock = currentBlock().previous();
        const QTextBlock nextLineBlock = currentBlock().next();
        const QString prevLine = prevLineBlock.isValid() ? prevLineBlock.text() : QString();
        const QString nextLine = nextLineBlock.isValid() ? nextLineBlock.text() : QString();

        QVector<BlockToken> blockTokens;
        BlockParser::classify(text, ctx, blockTokens, prevLine, nextLine);

        saveContext(ctx);
        setCurrentBlockState(static_cast<int>(ctx.topState()));
        return;
    }

    const int textLen = static_cast<int>(text.length());

    auto queueTableRefresh = [&](bool forceImmediate = false) {
        if (tableSyncInProgress_ || rehighlightInProgress_ || !tableRefreshTimer_) {
            return;
        }

        if (tableRefreshPending_ && tableRefreshTimer_->isActive()) {
            // Keep only one queued refresh; upgrade to immediate when needed.
            if (forceImmediate && tableRefreshTimer_->remainingTime() > 0) {
                tableRefreshTimer_->start(0);
            }
            return;
        }

        int delayMs = 0;
        if (!forceImmediate) {
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            if (lastTableRefreshMs_ >= 0) {
                const qint64 elapsedSinceLast = nowMs - lastTableRefreshMs_;
                if (elapsedSinceLast < tableRefreshDebounceMs_) {
                    delayMs = static_cast<int>(tableRefreshDebounceMs_ - elapsedSinceLast);
                }
            }
        }

        tableRefreshPending_ = true;
        // Run immediately for structural changes, throttle noisy burst updates.
        tableRefreshTimer_->start(delayMs);
    };

    // Setext headings depend on the next line. Queue a single full refresh
    // on potential underline edits to avoid reentrant highlighting crashes.
    if (!setextSyncInProgress_ &&
        !setextRefreshPending_ &&
        !rehighlightInProgress_ &&
        ctx.topState() == BlockState::Normal) {
        const QTextBlock prevBlock = currentBlock().previous();
        const QTextBlock nextBlock = currentBlock().next();
        const bool currentIsSetextUnderline =
            BlockParser::isSetextH1Underline(text) ||
            BlockParser::isSetextH2Underline(text);
        const bool hasAdjacentSetextUnderline =
            (prevBlock.isValid() &&
             (BlockParser::isSetextH1Underline(prevBlock.text()) ||
              BlockParser::isSetextH2Underline(prevBlock.text()))) ||
            (nextBlock.isValid() &&
             (BlockParser::isSetextH1Underline(nextBlock.text()) ||
              BlockParser::isSetextH2Underline(nextBlock.text())));
        const bool trailingBlankAfterText =
            text.trimmed().isEmpty() &&
            prevBlock.isValid() &&
            !prevBlock.text().trimmed().isEmpty() &&
            !nextBlock.isValid();
        const bool prevLineCouldBeHeading =
            prevBlock.isValid() &&
            couldBeSetextHeadingTextLine(prevBlock.text());
        const bool maybeSetextRelated =
            currentIsSetextUnderline ||
            (looksLikeSetextMutationCandidate(text) && prevLineCouldBeHeading) ||
            trailingBlankAfterText ||
            hasAdjacentSetextUnderline;

        if (maybeSetextRelated) {
            setextRefreshPending_ = true;
            QMetaObject::invokeMethod(this, [this]() {
                runPendingSetextRefresh();
            }, Qt::QueuedConnection);
        }
    }

    // Check for setext heading (lookahead) in the same container shape.
    bool isSetextH1 = false;
    bool isSetextH2 = false;
    if (ctx.topState() == BlockState::Normal) {
        const QTextBlock nextBlock = currentBlock().next();
        if (nextBlock.isValid()) {
            BlockParser::isSetextUnderlineForHeadingLine(text, nextBlock.text(), &isSetextH1, &isSetextH2);
        }
    }

    // 2. Classify the block
    const QTextBlock prevLineBlock = currentBlock().previous();
    const QTextBlock nextLineBlock = currentBlock().next();
    const QString prevLine = prevLineBlock.isValid() ? prevLineBlock.text() : QString();
    const QString nextLine = nextLineBlock.isValid() ? nextLineBlock.text() : QString();

    QVector<BlockToken> blockTokens;
    BlockType blockType = BlockParser::classify(text, ctx, blockTokens, prevLine, nextLine);

    auto computeContentOffset = [&](const QVector<BlockToken> &tokens) {
        int offset = 0;
        for (const auto &token : tokens) {
            if (token.type == TokenType::BlockquoteMark || token.type == TokenType::ListBullet) {
                offset = qMax(offset, token.start + token.length);
            }
        }
        return qBound(0, offset, textLen);
    };

    auto extractContainerTokens = [&](const QVector<BlockToken> &tokens) {
        QVector<BlockToken> containerTokens;
        for (const auto &token : tokens) {
            if (token.type == TokenType::BlockquoteMark || token.type == TokenType::ListBullet) {
                containerTokens.append(token);
            }
        }
        return containerTokens;
    };

    // Override for setext heading
    const bool canPromoteToSetextHeading =
        blockType == BlockType::Normal ||
        blockType == BlockType::Blockquote ||
        blockType == BlockType::ListItem;

    if (isSetextH1 && canPromoteToSetextHeading) {
        QVector<BlockToken> containerTokens = extractContainerTokens(blockTokens);
        const int contentOffset = computeContentOffset(containerTokens);
        blockTokens = containerTokens;
        if (contentOffset < textLen) {
            blockTokens.append({contentOffset, textLen - contentOffset, TokenType::HeadingH1});
        }
        blockType = BlockType::Heading;
    } else if (isSetextH2 && canPromoteToSetextHeading) {
        QVector<BlockToken> containerTokens = extractContainerTokens(blockTokens);
        const int contentOffset = computeContentOffset(containerTokens);
        blockTokens = containerTokens;
        if (contentOffset < textLen) {
            blockTokens.append({contentOffset, textLen - contentOffset, TokenType::HeadingH2});
        }
        blockType = BlockType::Heading;
    }
    // Mark setext underline itself. Only do this for normal/HR lines so we do
    // not override fenced code, LaTeX, HTML comments, etc.
    const bool isSetextH1Underline = BlockParser::isSetextH1Underline(text);
    const bool isSetextH2Underline = BlockParser::isSetextH2Underline(text);
    bool shouldRenderSetextMarker = false;
    if (isSetextH1Underline) {
        // Requested behavior: "===" should be visibly rendered even when standalone.
        shouldRenderSetextMarker = true;
    } else if (isSetextH2Underline) {
        bool prevIsH1 = false;
        bool prevIsH2 = false;
        const QTextBlock prevBlock = currentBlock().previous();
        shouldRenderSetextMarker =
            prevBlock.isValid() &&
            BlockParser::isSetextUnderlineForHeadingLine(prevBlock.text(), text, &prevIsH1, &prevIsH2) &&
            prevIsH2;
    }

    if (shouldRenderSetextMarker &&
        (blockType == BlockType::Normal || blockType == BlockType::HR) &&
        ctx.topState() == BlockState::Normal) {
        QVector<BlockToken> containerTokens = extractContainerTokens(blockTokens);

        blockTokens = containerTokens;
        const int contentOffset = computeContentOffset(blockTokens);
        if (contentOffset < textLen) {
            blockTokens.append({contentOffset, textLen - contentOffset, TokenType::SetextMarker});
        }
    }

    const int tableSyncOffset = computeContentOffset(blockTokens);
    auto tableContentAtOffset = [&](const QTextBlock &block) {
        if (!block.isValid()) {
            return QString();
        }

        const QString blockText = block.text();
        return blockText.mid(qMin(tableSyncOffset, static_cast<int>(blockText.length())));
    };

    auto isTableLikeLine = [&](const QString &lineContent) {
        return BlockParser::matchTable(lineContent) || BlockParser::matchTableSeparator(lineContent);
    };

    auto hasSeparatorAnchorAbove = [&](QTextBlock scan) {
        int guard = 0;
        while (scan.isValid() && guard < 32) {
            const QString scanContent = tableContentAtOffset(scan);
            if (scanContent.trimmed().isEmpty()) {
                return false;
            }

            if (BlockParser::matchTableSeparator(scanContent)) {
                const QTextBlock headerBlock = scan.previous();
                return headerBlock.isValid() && BlockParser::matchTable(tableContentAtOffset(headerBlock));
            }

            if (!BlockParser::matchTable(scanContent)) {
                return false;
            }

            scan = scan.previous();
            ++guard;
        }

        return false;
    };

    if (!tableSyncInProgress_ && !tableRefreshPending_ && blockType != BlockType::Table) {
        const QString currentTableText = text.mid(tableSyncOffset);
        const QString prevTableText = tableContentAtOffset(currentBlock().previous());
        const QString nextTableText = tableContentAtOffset(currentBlock().next());

        const bool currentIsTableRow = BlockParser::matchTable(currentTableText);
        const bool currentIsSeparator = BlockParser::matchTableSeparator(currentTableText);
        const bool adjacentTableLike = isTableLikeLine(prevTableText) || isTableLikeLine(nextTableText);
        const bool bodyAnchoredToSeparator = currentIsTableRow && hasSeparatorAnchorAbove(currentBlock());
        const bool blankBetweenTableLikeRows =
            currentTableText.trimmed().isEmpty() &&
            isTableLikeLine(prevTableText) &&
            isTableLikeLine(nextTableText);

        if ((currentIsSeparator && BlockParser::matchTable(prevTableText)) ||
            bodyAnchoredToSeparator ||
            (currentIsTableRow && adjacentTableLike) ||
            blankBetweenTableLikeRows) {
            // Incremental highlight can miss table state propagation across unchanged blocks.
            queueTableRefresh(currentIsSeparator);
        }
    }

    bool inBlockquoteContainer = false;
    for (const auto &token : blockTokens) {
        if (token.type == TokenType::BlockquoteMark) {
            inBlockquoteContainer = true;
            break;
        }
    }

    QColor blockquoteBackground;
    if (inBlockquoteContainer && formats_.contains(TokenType::BlockquoteBody)) {
        const QTextCharFormat blockquoteFmt = formats_[TokenType::BlockquoteBody];
        if (blockquoteFmt.background().style() != Qt::NoBrush) {
            blockquoteBackground = blockquoteFmt.background().color();
        }
    }

    auto applyFormatWithBlockquoteBackground = [&](int start, int length, const QTextCharFormat &baseFmt) {
        if (length <= 0) {
            return;
        }

        auto dimColorLightness = [](const QColor &color, qreal factor) {
            QColor hsl = color.toHsl();
            float h = 0.0f;
            float s = 0.0f;
            float l = 0.0f;
            float a = 1.0f;
            hsl.getHslF(&h, &s, &l, &a);
            l = static_cast<float>(qBound<qreal>(0.0, l * factor, 1.0));
            hsl.setHslF(h, s, l, a);
            return hsl.toRgb();
        };

        auto applyPreparedFormat = [&](int rangeStart, int rangeLength) {
            if (rangeLength <= 0) {
                return;
            }

            QTextCharFormat fmt = baseFmt;
            if (inBlockquoteContainer && blockquoteBackground.isValid()) {
                if (fmt.background().style() == Qt::NoBrush) {
                    fmt.setBackground(blockquoteBackground);
                } else {
                    const QColor tokenBackground = fmt.background().color();
                    if (tokenBackground.isValid()) {
                        if (tokenBackground != blockquoteBackground) {
                            // Keep token-local background hue, but reduce brightness in quotes.
                            fmt.setBackground(dimColorLightness(tokenBackground, 0.78));
                        }
                    }
                }
            }

            setFormat(rangeStart, rangeLength, fmt);
        };

        applyPreparedFormat(start, length);
    };

    // 3. Apply block token formats
    for (const auto &token : blockTokens) {
        if (formats_.contains(token.type)) {
            applyFormatWithBlockquoteBackground(token.start, token.length, formats_[token.type]);
        }
    }

    auto applyPreeditAnchorFormat = [&]() {
        if (preeditBlockNumber_ != currentBlock().blockNumber()) {
            return;
        }
        if (preeditStartInBlock_ <= 0 || preeditStartInBlock_ > textLen) {
            return;
        }

        QTextCharFormat cleanFmt;
        cleanFmt.setForeground(theme_.foreground);
        cleanFmt.setFontWeight(QFont::Normal);
        cleanFmt.setFontItalic(false);
        cleanFmt.setFontUnderline(false);
        cleanFmt.setFontStrikeOut(false);
        cleanFmt.setVerticalAlignment(QTextCharFormat::AlignNormal);
        // Neutral anchor: prevents highlight style from bleeding into the IME
        // composition overlay. Do not add underline here — it would visually
        // corrupt the character before the composing text.
        cleanFmt.setUnderlineStyle(QTextCharFormat::NoUnderline);

        // Use previous real character as preedit base source for Qt merge.
        setFormat(preeditStartInBlock_ - 1, 1, cleanFmt);
    };

    if (blockType == BlockType::BlankLine) {
        applyPreeditAnchorFormat();
        saveContext(ctx);
        setCurrentBlockState(static_cast<int>(ctx.topState()));
        return;
    }

    if (blockType == BlockType::Table) {
        const int tableOffset = computeContentOffset(blockTokens);
        const int tableLength = textLen - tableOffset;
        const QString tableText = text.mid(tableOffset);
        const bool isSeparator = BlockParser::matchTableSeparator(tableText);
        bool isHeader = false;
        if (!isSeparator) {
            QTextBlock next = currentBlock().next();
            if (next.isValid()) {
                const QString nextText = next.text();
                const QString nextTableText = nextText.mid(qMin(tableOffset, static_cast<int>(nextText.length())));
                isHeader = BlockParser::matchTableSeparator(nextTableText);
            }
        } else {
            QTextBlock prev = currentBlock().previous();
            if (prev.isValid()) {
                const QString prevText = prev.text();
                const QString prevTableText = prevText.mid(qMin(tableOffset, static_cast<int>(prevText.length())));
                if (BlockParser::matchTable(prevTableText)) {
                    // Header line depends on separator below, so refresh once.
                    queueTableRefresh(true);
                }
            }
        }

        if (tableLength > 0) {
            if (isSeparator) {
                applyFormatWithBlockquoteBackground(tableOffset, tableLength, formats_[TokenType::TableSeparator]);
            } else if (isHeader) {
                applyFormatWithBlockquoteBackground(tableOffset, tableLength, formats_[TokenType::TableHeader]);
            } else {
                applyFormatWithBlockquoteBackground(tableOffset, tableLength, formats_[TokenType::TableCell]);
            }
        }

        for (const auto &token : blockTokens) {
            if (token.type == TokenType::TablePipe && formats_.contains(TokenType::TablePipe)) {
                applyFormatWithBlockquoteBackground(token.start, token.length, formats_[TokenType::TablePipe]);
            }
        }
    }

    // 4. Run inline parser if not in code fence
    if (blockType != BlockType::CodeFenceBody &&
        blockType != BlockType::CodeFenceStart &&
        blockType != BlockType::CodeFenceEnd &&
        blockType != BlockType::HtmlComment) {

        const int contentOffset = computeContentOffset(blockTokens);

        if (blockType == BlockType::LatexDisplayBody ||
            blockType == BlockType::LatexEnvBody) {
            // Only run LaTeX parser for math bodies
            QVector<InlineToken> latexTokens;
            const int latexLen = textLen - contentOffset;
            if (latexLen > 0) {
                LatexParser::parseLatexBody(text, contentOffset, latexLen, latexTokens);
            }
            for (const auto &token : latexTokens) {
                if (formats_.contains(token.type))
                    applyFormatWithBlockquoteBackground(token.start, token.length, formats_[token.type]);
            }
        } else if (blockType != BlockType::LatexDisplayStart &&
                   blockType != BlockType::LatexDisplayEnd &&
                   blockType != BlockType::LatexEnvStart &&
                   blockType != BlockType::LatexEnvEnd) {
            // Run full inline parser
            QVector<InlineToken> inlineTokens;
            InlineParser::parse(text, contentOffset, ctx, inlineTokens);

            // Keep heading font-size even when inline syntax (like `code`) applies.
            double headingScale = 1.0;
            if (blockType == BlockType::Heading) {
                for (const auto &token : blockTokens) {
                    if (token.type == TokenType::HeadingH1) headingScale = 1.5;
                    else if (token.type == TokenType::HeadingH2) headingScale = 1.3;
                    else if (token.type == TokenType::HeadingH3) headingScale = 1.15;
                    else if (token.type == TokenType::HeadingH4) headingScale = 1.05;
                    else if (token.type == TokenType::HeadingH5) headingScale = 1.0;
                    else if (token.type == TokenType::HeadingH6) headingScale = 1.0;
                }
            }

            for (const auto &token : inlineTokens) {
                if (!formats_.contains(token.type))
                    continue;

                QTextCharFormat fmt = formats_[token.type];
                if (blockType == BlockType::Heading) {
                    fmt.setFontPointSize(baseFontSize_ * headingScale);
                }
                applyFormatWithBlockquoteBackground(token.start, token.length, fmt);
            }
        }
    }

    // 5. Save context
    applyPreeditAnchorFormat();
    saveContext(ctx);
    setCurrentBlockState(static_cast<int>(ctx.topState()));
}

ContextStack MdHighlighter::restoreContext() const
{
    QTextBlock prevBlock = currentBlock().previous();
    if (prevBlock.isValid()) {
        auto *data = dynamic_cast<ContextBlockData *>(prevBlock.userData());
        if (data) return data->ctx_;
    }
    return ContextStack();
}

void MdHighlighter::saveContext(const ContextStack &ctx)
{
    setCurrentBlockUserData(new ContextBlockData(ctx));
}

void MdHighlighter::buildFormats()
{
    formats_.clear();

    auto blendColor = [](const QColor &from, const QColor &to, qreal ratio) {
        const qreal t = qBound<qreal>(0.0, ratio, 1.0);
        return QColor(
            static_cast<int>(from.red() + (to.red() - from.red()) * t),
            static_cast<int>(from.green() + (to.green() - from.green()) * t),
            static_cast<int>(from.blue() + (to.blue() - from.blue()) * t));
    };

    auto colorDistance = [](const QColor &a, const QColor &b) {
        return qAbs(a.red() - b.red()) +
               qAbs(a.green() - b.green()) +
               qAbs(a.blue() - b.blue());
    };

    auto makeFormat = [](const QColor &fg, bool bold = false, bool italic = false, const QColor &bg = QColor()) {
        QTextCharFormat fmt;
        fmt.setForeground(fg);
        if (bold) fmt.setFontWeight(QFont::Bold);
        if (italic) fmt.setFontItalic(true);
        if (bg.isValid()) fmt.setBackground(bg);
        return fmt;
    };

    // Headings — with font size scaling
    const int baseSize = baseFontSize_;
    auto headingFmt = [&](const QColor &color, double scale) {
        QTextCharFormat fmt;
        fmt.setForeground(color);
        fmt.setFontWeight(QFont::Bold);
        fmt.setFontPointSize(baseSize * scale);
        return fmt;
    };

    formats_[TokenType::HeadingH1] = headingFmt(theme_.heading[0], 1.5);
    formats_[TokenType::HeadingH2] = headingFmt(theme_.heading[1], 1.3);
    formats_[TokenType::HeadingH3] = headingFmt(theme_.heading[2], 1.15);
    formats_[TokenType::HeadingH4] = headingFmt(theme_.heading[3], 1.05);
    formats_[TokenType::HeadingH5] = headingFmt(theme_.heading[4], 1.0);
    QTextCharFormat h6 = headingFmt(theme_.heading[5], 1.0);
    h6.setFontItalic(true);
    formats_[TokenType::HeadingH6] = h6;

    auto withHeadingBg = [](QTextCharFormat fmt) {
        QColor bg = fmt.foreground().color();
        bg.setAlpha(44);
        fmt.setBackground(bg);
        return fmt;
    };
    formats_[TokenType::HeadingH1] = withHeadingBg(formats_[TokenType::HeadingH1]);
    formats_[TokenType::HeadingH2] = withHeadingBg(formats_[TokenType::HeadingH2]);
    formats_[TokenType::HeadingH3] = withHeadingBg(formats_[TokenType::HeadingH3]);

    formats_[TokenType::HeadingMarker] = makeFormat(theme_.markerFg);
    formats_[TokenType::SetextMarker]  = makeFormat(theme_.markerFg);

    // Code
    formats_[TokenType::CodeFenceMark] = makeFormat(theme_.codeFenceFg);
    formats_[TokenType::CodeFenceLang] = makeFormat(theme_.codeFenceLangFg);
    formats_[TokenType::CodeFenceBody] = makeFormat(theme_.codeFenceFg);
    formats_[TokenType::InlineCode]    = makeFormat(theme_.codeInlineFg, false, false, theme_.codeInlineBg);
    formats_[TokenType::InlineCodeMark] = makeFormat(theme_.markerFg);

    // Blockquote. Keep a clear visual separation from both editor background
    // and current-line highlight, especially for the white theme.
    const bool lightTheme = theme_.background.lightness() >= 128;
    QColor blockquoteBg = lightTheme
        ? blendColor(theme_.background, theme_.blockquoteBorderFg, 0.46)
        : blendColor(theme_.background, theme_.blockquoteBorderFg, 0.28);

    if (colorDistance(blockquoteBg, theme_.background) < (lightTheme ? 42 : 24)) {
        blockquoteBg = lightTheme
            ? theme_.background.darker(110)
            : theme_.background.lighter(115);
    }

    if (colorDistance(blockquoteBg, theme_.currentLineBg) < (lightTheme ? 36 : 18)) {
        blockquoteBg = lightTheme
            ? theme_.currentLineBg.darker(116)
            : blendColor(blockquoteBg, theme_.blockquoteBorderFg, 0.24);
    }

    if (lightTheme && blockquoteBg.lightness() >= theme_.currentLineBg.lightness()) {
        blockquoteBg = theme_.currentLineBg.darker(116);
    }

    blockquoteBg.setAlpha(255);
    formats_[TokenType::BlockquoteMark] = makeFormat(theme_.blockquoteBorderFg, false, false, blockquoteBg);
    formats_[TokenType::BlockquoteBody] = makeFormat(theme_.blockquoteFg, false, false, blockquoteBg);

    // List
    formats_[TokenType::ListBullet] = makeFormat(theme_.listBulletFg, true);
    formats_[TokenType::ListBody]   = makeFormat(theme_.foreground);

    // Table
    QTextCharFormat tableHeader = makeFormat(theme_.foreground, true);
    QColor tableHeaderBg = theme_.selectionBg;
    tableHeaderBg.setAlpha(56);
    tableHeader.setBackground(tableHeaderBg);
    QTextCharFormat tableCell = makeFormat(theme_.foreground);
    QColor tableCellBg = theme_.lineNumberBg;
    tableCellBg.setAlpha(28);
    tableCell.setBackground(tableCellBg);
    QTextCharFormat tableSep = makeFormat(theme_.tablePipeFg);
    tableSep.setFontWeight(QFont::Bold);

    formats_[TokenType::TablePipe]      = makeFormat(theme_.tablePipeFg, true);
    formats_[TokenType::TableHeader]    = tableHeader;
    formats_[TokenType::TableSeparator] = tableSep;
    formats_[TokenType::TableCell]      = tableCell;

    // HR
    formats_[TokenType::HR] = makeFormat(theme_.hrFg);

    // Inline
    formats_[TokenType::Bold]          = makeFormat(theme_.boldFg, true);
    formats_[TokenType::BoldMarker]    = makeFormat(theme_.markerFg, true);
    formats_[TokenType::Italic]        = makeFormat(theme_.italicFg, false, true);
    formats_[TokenType::ItalicMarker]  = makeFormat(theme_.markerFg, false, true);
    formats_[TokenType::BoldItalic]    = makeFormat(theme_.boldFg, true, true);
    formats_[TokenType::BoldItalicMarker] = makeFormat(theme_.markerFg, true, true);

    QTextCharFormat underlineFmt = makeFormat(theme_.foreground);
    underlineFmt.setFontUnderline(true);
    formats_[TokenType::Underline] = underlineFmt;
    formats_[TokenType::UnderlineMarker] = makeFormat(theme_.markerFg, true);

    QTextCharFormat highlightFmt = makeFormat(theme_.foreground, false, false, theme_.searchHighlightBg);
    formats_[TokenType::Highlight] = highlightFmt;
    formats_[TokenType::HighlightMarker] = makeFormat(theme_.markerFg, true);

    QTextCharFormat superscriptFmt = makeFormat(theme_.foreground);
    superscriptFmt.setVerticalAlignment(QTextCharFormat::AlignSuperScript);
    superscriptFmt.setFontPointSize(baseSize * 0.85);
    formats_[TokenType::Superscript] = superscriptFmt;
    formats_[TokenType::SuperscriptMarker] = makeFormat(theme_.markerFg);

    QTextCharFormat subscriptFmt = makeFormat(theme_.foreground);
    subscriptFmt.setVerticalAlignment(QTextCharFormat::AlignSubScript);
    subscriptFmt.setFontPointSize(baseSize * 0.85);
    formats_[TokenType::Subscript] = subscriptFmt;
    formats_[TokenType::SubscriptMarker] = makeFormat(theme_.markerFg);

    QTextCharFormat strikeFmt = makeFormat(theme_.strikeFg);
    strikeFmt.setFontStrikeOut(true);
    formats_[TokenType::Strikethrough] = strikeFmt;
    formats_[TokenType::StrikeMarker]  = makeFormat(theme_.markerFg);

    formats_[TokenType::LinkText]    = makeFormat(theme_.linkTextFg);
    formats_[TokenType::LinkUrl]     = makeFormat(theme_.linkUrlFg);
    formats_[TokenType::LinkBracket] = makeFormat(theme_.linkTextFg, true);
    formats_[TokenType::ImageAlt]    = makeFormat(theme_.imageFg);
    formats_[TokenType::ImageUrl]    = makeFormat(theme_.linkUrlFg);
    formats_[TokenType::ImageBracket] = makeFormat(theme_.imageFg, true);
    formats_[TokenType::CheckboxMarker] = makeFormat(theme_.listBulletFg, true);
    formats_[TokenType::HtmlComment] = makeFormat(theme_.lineNumberFg, false, true);

    formats_[TokenType::HardBreakSpace]     = makeFormat(theme_.hardBreakFg);
    formats_[TokenType::HardBreakBackslash] = makeFormat(theme_.hardBreakFg);
    formats_[TokenType::Escape] = makeFormat(theme_.markerFg);

    // LaTeX
    formats_[TokenType::LatexDelimiter] = makeFormat(theme_.latexDelimiterFg);
    formats_[TokenType::LatexCommand]   = makeFormat(theme_.latexCommandFg);
    formats_[TokenType::LatexBrace]     = makeFormat(theme_.latexBraceFg);
    QTextCharFormat latexMathBodyFmt = makeFormat(theme_.latexMathBodyFg);
    if (theme_.latexMathBodyFg == theme_.foreground) {
        latexMathBodyFmt.setFontItalic(true);
    }
    formats_[TokenType::LatexMathBody]  = latexMathBodyFmt;
    formats_[TokenType::LatexEnvName]   = makeFormat(theme_.latexEnvNameFg);
    formats_[TokenType::LatexBeginEnd]  = makeFormat(theme_.latexCommandFg);

    // Meta
    formats_[TokenType::Marker] = makeFormat(theme_.markerFg);
}

// Unused methods — the block-specific highlight methods are handled via
// the generic token-based approach in highlightBlock().
void MdHighlighter::highlightHeading(const QString &, int) {}
void MdHighlighter::highlightCodeFence(const QString &, ContextStack &) {}
void MdHighlighter::highlightInline(const QString &, int, const ContextStack &) {}
void MdHighlighter::highlightLatex(const QString &, int, ContextStack &) {}
void MdHighlighter::highlightTable(const QString &, const ContextStack &) {}
void MdHighlighter::highlightBlockquote(const QString &, ContextStack &) {}
void MdHighlighter::highlightListItem(const QString &, const ContextStack &) {}
