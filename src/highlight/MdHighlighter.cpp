// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include "MdHighlighter.h"
#include "parser/BlockParser.h"
#include "parser/InlineParser.h"
#include "parser/LatexParser.h"

#include <QTextBlock>
#include <QTextBlockUserData>
#include <QRegularExpression>
#include <QMetaObject>

// Store ContextStack in QTextBlockUserData
class ContextBlockData : public QTextBlockUserData {
public:
    ContextBlockData(const ContextStack &ctx) : ctx_(ctx) {}
    ContextStack ctx_;
};

MdHighlighter::MdHighlighter(QTextDocument *parent, const Theme &theme)
    : QSyntaxHighlighter(parent)
    , theme_(theme)
{
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

void MdHighlighter::highlightBlock(const QString &text)
{
    if (!enabled_) return;
    const int textLen = static_cast<int>(text.length());
    static QRegularExpression tableSeparatorRe(R"(^\s*\|?\s*:?-{3,}:?\s*(\|\s*:?-{3,}:?\s*)+\|?\s*$)");

    // 1. Restore context from previous block
    ContextStack ctx = restoreContext();

    // Setext headings depend on the next line. Queue a single full refresh
    // on potential underline edits to avoid reentrant highlighting crashes.
    if (!setextSyncInProgress_ && !setextRefreshPending_ && ctx.topState() == BlockState::Normal) {
        const QString trimmed = text.trimmed();
        const QTextBlock prevBlock = currentBlock().previous();
        const QTextBlock nextBlock = currentBlock().next();
        const bool hasAdjacentSetextUnderline =
            (prevBlock.isValid() &&
             (BlockParser::isSetextH1Underline(prevBlock.text()) ||
              BlockParser::isSetextH2Underline(prevBlock.text()))) ||
            (nextBlock.isValid() &&
             (BlockParser::isSetextH1Underline(nextBlock.text()) ||
              BlockParser::isSetextH2Underline(nextBlock.text())));
        const bool maybeSetextRelated =
            trimmed.isEmpty() ||
            trimmed.startsWith(QLatin1Char('-')) ||
            trimmed.startsWith(QLatin1Char('=')) ||
            hasAdjacentSetextUnderline;

        if (maybeSetextRelated) {
            setextRefreshPending_ = true;
            QMetaObject::invokeMethod(this, [this]() {
                setextSyncInProgress_ = true;
                rehighlight();
                setextSyncInProgress_ = false;
                setextRefreshPending_ = false;
            }, Qt::QueuedConnection);
        }
    }

    // Check for setext heading (lookahead)
    bool isSetextH1 = false, isSetextH2 = false;
    if (ctx.topState() == BlockState::Normal) {
        QTextBlock nextBlock = currentBlock().next();
        if (nextBlock.isValid()) {
            QString nextLine = nextBlock.text();
            isSetextH1 = BlockParser::isSetextH1Underline(nextLine);
            isSetextH2 = BlockParser::isSetextH2Underline(nextLine);
        }
    }

    // 2. Classify the block
    QVector<BlockToken> blockTokens;
    BlockType blockType = BlockParser::classify(text, ctx, blockTokens);

    auto computeContentOffset = [&]() {
        int offset = 0;
        for (const auto &token : blockTokens) {
            if (token.type == TokenType::BlockquoteMark || token.type == TokenType::ListBullet) {
                offset = qMax(offset, token.start + token.length);
            }
        }
        return qBound(0, offset, textLen);
    };

    // Override for setext heading
    if (isSetextH1 && blockType == BlockType::Normal) {
        blockTokens.clear();
        blockTokens.append({0, textLen, TokenType::HeadingH1});
        blockType = BlockType::Heading;
    } else if (isSetextH2 && blockType == BlockType::Normal) {
        blockTokens.clear();
        blockTokens.append({0, textLen, TokenType::HeadingH2});
        blockType = BlockType::Heading;
    }
    // Mark setext underline itself. Only do this for normal/HR lines so we do
    // not override fenced code, LaTeX, HTML comments, etc.
    const bool isSetextUnderline =
        BlockParser::isSetextH1Underline(text) || BlockParser::isSetextH2Underline(text);
    if (isSetextUnderline &&
        (blockType == BlockType::Normal || blockType == BlockType::HR) &&
        ctx.topState() == BlockState::Normal) {
        const QTextBlock prevBlock = currentBlock().previous();
        if (prevBlock.isValid() && !prevBlock.text().trimmed().isEmpty()) {
            QVector<BlockToken> containerTokens;
            for (const auto &token : blockTokens) {
                if (token.type == TokenType::BlockquoteMark || token.type == TokenType::ListBullet) {
                    containerTokens.append(token);
                }
            }

            blockTokens = containerTokens;
            const int contentOffset = computeContentOffset();
            if (contentOffset < textLen) {
                blockTokens.append({contentOffset, textLen - contentOffset, TokenType::SetextMarker});
            }
        }
    }

    // 3. Apply block token formats
    for (const auto &token : blockTokens) {
        if (formats_.contains(token.type)) {
            setFormat(token.start, token.length, formats_[token.type]);
        }
    }

    if (blockType == BlockType::Table) {
        const int tableOffset = computeContentOffset();
        const int tableLength = textLen - tableOffset;
        const bool isSeparator = tableSeparatorRe.match(text).hasMatch();
        bool isHeader = false;
        if (!isSeparator) {
            QTextBlock next = currentBlock().next();
            isHeader = next.isValid() && tableSeparatorRe.match(next.text()).hasMatch();
        }

        if (tableLength > 0) {
            if (isSeparator) {
                setFormat(tableOffset, tableLength, formats_[TokenType::TableSeparator]);
            } else if (isHeader) {
                setFormat(tableOffset, tableLength, formats_[TokenType::TableHeader]);
            } else {
                setFormat(tableOffset, tableLength, formats_[TokenType::TableCell]);
            }
        }

        for (const auto &token : blockTokens) {
            if (token.type == TokenType::TablePipe && formats_.contains(TokenType::TablePipe)) {
                setFormat(token.start, token.length, formats_[TokenType::TablePipe]);
            }
        }
    }

    // 4. Run inline parser if not in code fence
    if (blockType != BlockType::CodeFenceBody &&
        blockType != BlockType::CodeFenceStart &&
        blockType != BlockType::CodeFenceEnd &&
        blockType != BlockType::HtmlComment) {

        const int contentOffset = computeContentOffset();

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
                    setFormat(token.start, token.length, formats_[token.type]);
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
                setFormat(token.start, token.length, fmt);
            }
        }
    }

    // 5. Save context
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

    // Blockquote
    QColor blockquoteBg = theme_.blockquoteBorderFg;
    blockquoteBg.setAlpha(112);
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
