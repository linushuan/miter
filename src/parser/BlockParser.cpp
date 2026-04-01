// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include "BlockParser.h"
#include <QRegularExpression>

namespace {
struct PrefixInfo {
    int  blockquoteDepth = 0;
    int  blockquoteEnd   = 0;
    bool hasList         = false;
    int  listStart       = 0;
    int  listEnd         = 0;
    int  contentOffset   = 0;
};

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

bool matchTaskCheckboxPrefix(const QString &text, int *start = nullptr, int *length = nullptr)
{
    static QRegularExpression re(R"(^(\s*)(\[[ xX]\])(?=\s|$))");
    const auto m = re.match(text);
    if (!m.hasMatch()) {
        return false;
    }
    if (start) {
        *start = m.capturedStart(2);
    }
    if (length) {
        *length = m.capturedLength(2);
    }
    return true;
}

bool matchSingleBlockquoteMarker(const QString &text, int *consumed = nullptr)
{
    static QRegularExpression re(R"(^ {0,3}> ?)");
    const auto m = re.match(text);
    if (!m.hasMatch()) {
        return false;
    }

    if (consumed) {
        *consumed = m.capturedLength(0);
    }
    return true;
}

bool matchOrderedListPrefixForParser(const QString &text, int *indent = nullptr, int *consumed = nullptr)
{
    static QRegularExpression re(R"(^(\s*)(\d{1,9})[.)](\s|$))");
    const auto m = re.match(text);
    if (!m.hasMatch()) {
        return false;
    }

    if (indent) {
        *indent = m.captured(1).length();
    }
    if (consumed) {
        *consumed = m.capturedEnd(0);
    }
    return true;
}

bool matchUnorderedListPrefixForParser(const QString &text, int *indent = nullptr, int *consumed = nullptr)
{
    static QRegularExpression re(R"(^(\s*)([-*+])(\s|$))");
    const auto m = re.match(text);
    if (!m.hasMatch()) {
        return false;
    }
    if (BlockParser::isHorizontalRule(text)) {
        return false;
    }

    if (indent) {
        *indent = m.captured(1).length();
    }
    if (consumed) {
        *consumed = m.capturedEnd(0);
    }
    return true;
}

PrefixInfo parseContainerPrefix(const QString &line, int blockquoteLimit, bool parseList)
{
    PrefixInfo info;
    int cursor = 0;

    while (blockquoteLimit < 0 || info.blockquoteDepth < blockquoteLimit) {
        int consumed = 0;
        if (!matchSingleBlockquoteMarker(line.mid(cursor), &consumed)) {
            break;
        }
        cursor += consumed;
        ++info.blockquoteDepth;
    }

    info.blockquoteEnd = cursor;

    if (parseList) {
        const QString afterBlockquote = line.mid(cursor);
        int indent = 0;
        int consumed = 0;

        if (matchOrderedListPrefixForParser(afterBlockquote, &indent, &consumed) ||
            matchUnorderedListPrefixForParser(afterBlockquote, &indent, &consumed)) {
            info.hasList = true;
            info.listStart = cursor;
            info.listEnd = cursor + consumed;
            cursor = info.listEnd;
        }
    }

    info.contentOffset = cursor;
    return info;
}

void appendContainerMarkers(const PrefixInfo &prefix, QVector<BlockToken> &tokens)
{
    if (prefix.blockquoteDepth > 0 && prefix.blockquoteEnd > 0) {
        tokens.append({0, prefix.blockquoteEnd, TokenType::BlockquoteMark});
    }
    if (prefix.hasList && prefix.listEnd > prefix.listStart) {
        tokens.append({prefix.listStart, prefix.listEnd - prefix.listStart, TokenType::ListBullet});
    }
}
}

BlockType BlockParser::classify(const QString &text, ContextStack &ctx, QVector<BlockToken> &tokens)
{
    tokens.clear();
    BlockType type = BlockType::Normal;
    if (classifyInOpenContext(text, ctx, tokens, type)) {
        return type;
    }
    return classifyInNormalContext(text, ctx, tokens);
}

bool BlockParser::classifyInOpenContext(const QString &text,
                                        ContextStack &ctx,
                                        QVector<BlockToken> &tokens,
                                        BlockType &type)
{
    if (ctx.inCode()) {
        const ContextFrame frame = ctx.top();
        const PrefixInfo prefix = parseContainerPrefix(text, frame.depth, frame.listIndent > 0);
        const int contentOffset = prefix.contentOffset;
        const QString content = text.mid(contentOffset);
        const int contentLen = static_cast<int>(content.length());

        appendContainerMarkers(prefix, tokens);

        if (matchCodeFenceEnd(content, frame.fenceChar, frame.fenceLen)) {
            tokens.append({contentOffset, contentLen, TokenType::CodeFenceMark});
            ctx.pop();
            type = BlockType::CodeFenceEnd;
            return true;
        }

        tokens.append({contentOffset, contentLen, TokenType::CodeFenceBody});
        type = BlockType::CodeFenceBody;
        return true;
    }

    if (ctx.topState() == BlockState::LatexDisplay) {
        const ContextFrame frame = ctx.top();
        const PrefixInfo prefix = parseContainerPrefix(text, frame.depth, frame.listIndent > 0);
        const int contentOffset = prefix.contentOffset;
        const QString content = text.mid(contentOffset);
        const int contentLen = static_cast<int>(content.length());

        appendContainerMarkers(prefix, tokens);

        if (isStandaloneLatexDisplayFence(content)) {
            tokens.append({contentOffset, contentLen, TokenType::LatexDelimiter});
            ctx.pop();
            type = BlockType::LatexDisplayEnd;
            return true;
        }

        tokens.append({contentOffset, contentLen, TokenType::LatexMathBody});
        type = BlockType::LatexDisplayBody;
        return true;
    }

    if (ctx.topState() == BlockState::LatexEnv) {
        const ContextFrame frame = ctx.top();
        const PrefixInfo prefix = parseContainerPrefix(text, frame.depth, frame.listIndent > 0);
        const int contentOffset = prefix.contentOffset;
        const QString content = text.mid(contentOffset);
        const int contentLen = static_cast<int>(content.length());
        const QString endPattern = "\\end{" + frame.envName + "}";

        appendContainerMarkers(prefix, tokens);

        if (content.contains(endPattern)) {
            tokens.append({contentOffset, contentLen, TokenType::LatexBeginEnd});
            ctx.pop();
            type = BlockType::LatexEnvEnd;
            return true;
        }

        tokens.append({contentOffset, contentLen, TokenType::LatexMathBody});
        type = BlockType::LatexEnvBody;
        return true;
    }

    if (ctx.topState() == BlockState::HtmlComment) {
        const ContextFrame frame = ctx.top();
        const PrefixInfo prefix = parseContainerPrefix(text, frame.depth, frame.listIndent > 0);
        const int contentOffset = prefix.contentOffset;
        const QString content = text.mid(contentOffset);
        const int contentLen = static_cast<int>(content.length());

        appendContainerMarkers(prefix, tokens);

        const int closePos = content.indexOf("-->");
        if (closePos >= 0) {
            tokens.append({contentOffset, closePos + 3, TokenType::HtmlComment});
            ctx.pop();
        } else {
            tokens.append({contentOffset, contentLen, TokenType::HtmlComment});
        }

        type = BlockType::HtmlComment;
        return true;
    }

    return false;
}

BlockType BlockParser::classifyInNormalContext(const QString &text,
                                               ContextStack &ctx,
                                               QVector<BlockToken> &tokens)
{
    const int textLen = static_cast<int>(text.length());
    const PrefixInfo prefix = parseContainerPrefix(text, -1, true);
    const int contentOffset = prefix.contentOffset;
    const QString content = text.mid(contentOffset);
    const int contentLen = static_cast<int>(content.length());

    int firstNonSpace = 0;
    while (firstNonSpace < contentLen && content[firstNonSpace].isSpace()) {
        ++firstNonSpace;
    }

    if (firstNonSpace + 3 < contentLen &&
        content[firstNonSpace] == QLatin1Char('<') &&
        content[firstNonSpace + 1] == QLatin1Char('!') &&
        content[firstNonSpace + 2] == QLatin1Char('-') &&
        content[firstNonSpace + 3] == QLatin1Char('-')) {
        appendContainerMarkers(prefix, tokens);

        const int closePos = content.indexOf(QStringLiteral("-->"), firstNonSpace + 4);
        if (closePos >= 0) {
            tokens.append({
                contentOffset + firstNonSpace,
                closePos + 3 - firstNonSpace,
                TokenType::HtmlComment
            });
        } else {
            tokens.append({
                contentOffset + firstNonSpace,
                contentLen - firstNonSpace,
                TokenType::HtmlComment
            });
            ContextFrame frame;
            frame.state = BlockState::HtmlComment;
            frame.depth = prefix.blockquoteDepth;
            frame.listIndent = prefix.hasList ? (prefix.listEnd - prefix.listStart) : 0;
            ctx.push(frame);
        }
        return BlockType::HtmlComment;
    }

    int level = 0;
    int headingStart = 0;
    int headingEnd = 0;
    if (matchATXHeading(content, level, headingStart, headingEnd)) {
        appendContainerMarkers(prefix, tokens);
        tokens.append({contentOffset, headingStart, TokenType::HeadingMarker});

        TokenType headingType = static_cast<TokenType>(
            static_cast<int>(TokenType::HeadingH1) + level - 1);
        tokens.append({contentOffset + headingStart, headingEnd - headingStart, headingType});
        return BlockType::Heading;
    }

    QChar fenceChar;
    int fenceLen = 0;
    int indent = 0;
    QString lang;
    if (matchCodeFenceStart(content, fenceChar, fenceLen, indent, lang)) {
        appendContainerMarkers(prefix, tokens);
        tokens.append({contentOffset, contentLen, TokenType::CodeFenceMark});

        if (!lang.isEmpty()) {
            const int langStart = content.indexOf(lang);
            if (langStart >= 0) {
                tokens.append({
                    contentOffset + langStart,
                    static_cast<int>(lang.length()),
                    TokenType::CodeFenceLang
                });
            }
        }

        ContextFrame frame;
        frame.state = BlockState::CodeFence;
        frame.fenceChar = fenceChar;
        frame.fenceLen = fenceLen;
        frame.depth = prefix.blockquoteDepth;
        frame.listIndent = prefix.hasList ? (prefix.listEnd - prefix.listStart) : 0;
        ctx.push(frame);
        return BlockType::CodeFenceStart;
    }

    static QRegularExpression singleLineDisplayRe(R"(^\s*(\$\$)\s*(.+?)\s*(\$\$)\s*$)");
    const auto displayMatch = singleLineDisplayRe.match(content);
    if (displayMatch.hasMatch()) {
        const int openStart = displayMatch.capturedStart(1);
        const int openLen = displayMatch.capturedLength(1);
        const int innerStart = displayMatch.capturedStart(2);
        const int innerEnd = displayMatch.capturedEnd(2);
        const int closeStart = displayMatch.capturedStart(3);
        const int closeLen = displayMatch.capturedLength(3);

        appendContainerMarkers(prefix, tokens);

        tokens.append({contentOffset + openStart, openLen, TokenType::LatexDelimiter});
        if (innerEnd > innerStart) {
            tokens.append({contentOffset + innerStart, innerEnd - innerStart, TokenType::LatexMathBody});
        }
        tokens.append({contentOffset + closeStart, closeLen, TokenType::LatexDelimiter});
        return BlockType::LatexDisplayBody;
    }

    if (isStandaloneLatexDisplayFence(content)) {
        appendContainerMarkers(prefix, tokens);
        tokens.append({contentOffset, contentLen, TokenType::LatexDelimiter});

        ContextFrame frame;
        frame.state = BlockState::LatexDisplay;
        frame.depth = prefix.blockquoteDepth;
        frame.listIndent = prefix.hasList ? (prefix.listEnd - prefix.listStart) : 0;
        ctx.push(frame);
        return BlockType::LatexDisplayStart;
    }

    static QRegularExpression beginRe(R"(^\s*\\begin\{([^}\s]+)\}\s*$)");
    const auto beginMatch = beginRe.match(content);
    if (beginMatch.hasMatch()) {
        appendContainerMarkers(prefix, tokens);
        tokens.append({contentOffset, contentLen, TokenType::LatexBeginEnd});

        ContextFrame frame;
        frame.state = BlockState::LatexEnv;
        frame.envName = beginMatch.captured(1);
        frame.depth = prefix.blockquoteDepth;
        frame.listIndent = prefix.hasList ? (prefix.listEnd - prefix.listStart) : 0;
        ctx.push(frame);
        return BlockType::LatexEnvStart;
    }

    if (matchTable(content)) {
        appendContainerMarkers(prefix, tokens);
        for (int i = 0; i < contentLen; ++i) {
            if (content[i] == QLatin1Char('|')) {
                tokens.append({contentOffset + i, 1, TokenType::TablePipe});
            }
        }
        return BlockType::Table;
    }

    if (matchHR(content)) {
        appendContainerMarkers(prefix, tokens);
        tokens.append({contentOffset, contentLen, TokenType::HR});
        return BlockType::HR;
    }

    if (content.trimmed().isEmpty() && prefix.blockquoteDepth == 0 && !prefix.hasList) {
        return BlockType::BlankLine;
    }

    if (prefix.hasList) {
        appendContainerMarkers(prefix, tokens);
        tokens.append({contentOffset, textLen - contentOffset, TokenType::ListBody});

        int checkboxStart = 0;
        int checkboxLen = 0;
        if (matchTaskCheckboxPrefix(content, &checkboxStart, &checkboxLen) && checkboxLen > 0) {
            tokens.append({contentOffset + checkboxStart, checkboxLen, TokenType::CheckboxMarker});
        }
        return BlockType::ListItem;
    }

    if (prefix.blockquoteDepth > 0) {
        appendContainerMarkers(prefix, tokens);
        tokens.append({contentOffset, textLen - contentOffset, TokenType::BlockquoteBody});
        return BlockType::Blockquote;
    }

    return BlockType::Normal;
}

bool BlockParser::parseOrderedListLine(const QString &text, OrderedListLineMatch *match)
{
    static const QRegularExpression re(
        R"(^(\s*)(\d{1,9})([.)])(\s+|$)(\[[ xX]\]\s+)?(.*)$)");

    const auto m = re.match(text);
    if (!m.hasMatch()) {
        return false;
    }

    if (match) {
        match->indent = m.captured(1).length();
        match->number = m.captured(2).toInt();
        match->delimiter = m.captured(3);
        match->checkbox = m.captured(5);
        match->content = m.captured(6);
        match->numberStart = m.capturedStart(2);
        match->numberLength = m.capturedLength(2);
        match->contentStart = m.capturedStart(6);
        match->markerEnd = m.capturedEnd(4) + m.capturedLength(5);
    }

    return true;
}

bool BlockParser::parseUnorderedListLine(const QString &text, UnorderedListLineMatch *match)
{
    static const QRegularExpression re(
        R"(^(\s*)([-*+])(\s+|$)(\[[ xX]\]\s+)?(.*)$)");

    const auto m = re.match(text);
    if (!m.hasMatch()) {
        return false;
    }

    if (isHorizontalRule(text)) {
        return false;
    }

    if (match) {
        match->indent = m.captured(1).length();
        match->marker = m.captured(2);
        match->checkbox = m.captured(4);
        match->content = m.captured(5);
        match->contentStart = m.capturedStart(5);
        match->markerEnd = m.capturedEnd(3) + m.capturedLength(4);
    }

    return true;
}

bool BlockParser::parseBlockquoteLine(const QString &text, BlockquoteLineMatch *match)
{
    static const QRegularExpression re(R"(^(\s*(?:> ?)+)(.*)$)");

    const auto m = re.match(text);
    if (!m.hasMatch()) {
        return false;
    }

    BlockquoteLineMatch parsed;
    parsed.prefix = m.captured(1);
    parsed.content = m.captured(2);
    if (!parseBlockquotePrefixShape(parsed.prefix, &parsed.indent, &parsed.depth)) {
        return false;
    }

    if (match) {
        *match = parsed;
    }
    return true;
}

bool BlockParser::isHorizontalRule(const QString &text)
{
    return matchHR(text);
}

bool BlockParser::isStandaloneLatexDisplayFence(const QString &text)
{
    return text.trimmed() == QLatin1String("$$");
}

bool BlockParser::isCodeFenceStartLine(const QString &text)
{
    QChar fenceChar;
    int fenceLen = 0;
    int indent = 0;
    QString lang;
    if (!matchCodeFenceStart(text, fenceChar, fenceLen, indent, lang)) {
        return false;
    }
    return fenceChar == QLatin1Char('`');
}

bool BlockParser::isSetextH1Underline(const QString &nextLine)
{
    static QRegularExpression re(R"(^ {0,3}={3,}\s*$)");
    return re.match(nextLine).hasMatch();
}

bool BlockParser::isSetextH2Underline(const QString &nextLine)
{
    // Accept classic "---" plus requested HR-style variants for heading underline
    // behavior: "***", "- - -", "* * *".
    static QRegularExpression re(R"(^ {0,3}([-*])(\s*\1){2,}\s*$)");
    return re.match(nextLine).hasMatch();
}

bool BlockParser::matchCodeFenceStart(const QString &text, QChar &fenceChar, int &fenceLen, int &indent, QString &lang)
{
    // Allow common info strings like c++, objective-c, python3.
    static QRegularExpression re(R"(^( {0,3})(`{3,}|~{3,})([^\s`]*)\s*$)");
    auto m = re.match(text);
    if (!m.hasMatch()) return false;

    indent = m.captured(1).length();
    QString fence = m.captured(2);
    fenceChar = fence[0];
    fenceLen = fence.length();
    lang = m.captured(3);
    return true;
}

bool BlockParser::matchCodeFenceEnd(const QString &text, QChar fenceChar, int fenceLen)
{
    static QRegularExpression reBacktick(R"(^ {0,3}`{3,}\s*$)");
    static QRegularExpression reTilde(R"(^ {0,3}~{3,}\s*$)");

    const auto &re = (fenceChar == '`') ? reBacktick : reTilde;
    auto m = re.match(text);
    if (!m.hasMatch()) return false;

    // Count fence chars
    int count = 0;
    for (QChar c : text.trimmed()) {
        if (c == fenceChar) count++;
        else break;
    }
    return count >= fenceLen;
}

bool BlockParser::matchATXHeading(const QString &text, int &level, int &contentStart, int &contentEnd)
{
    static QRegularExpression re(R"(^ {0,3}(#{1,6})(\s+|$))");
    auto m = re.match(text);
    if (!m.hasMatch()) return false;

    level = m.captured(1).length();
    contentStart = m.capturedEnd(0);
    contentEnd = static_cast<int>(text.length());
    return true;
}

bool BlockParser::matchTable(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    if (trimmed.count(QLatin1Char('|')) < 2) {
        return false;
    }

    static const QRegularExpression separatorRe(
        R"(^\s*\|?\s*:?-{3,}:?\s*(\|\s*:?-{3,}:?\s*)+\|?\s*$)");
    if (separatorRe.match(trimmed).hasMatch()) {
        return true;
    }

    bool hasCellContent = false;
    for (const QChar ch : trimmed) {
        if (ch != QLatin1Char('|') && !ch.isSpace()) {
            hasCellContent = true;
            break;
        }
    }
    return hasCellContent;
}

bool BlockParser::matchBlockquote(const QString &text, int &contentOffset)
{
    int consumed = 0;
    if (!matchSingleBlockquoteMarker(text, &consumed)) {
        return false;
    }
    contentOffset = consumed;
    return true;
}

bool BlockParser::matchOrderedList(const QString &text, int &indent, int &contentOffset)
{
    OrderedListLineMatch match;
    if (!parseOrderedListLine(text, &match)) {
        return false;
    }
    indent = match.indent;
    contentOffset = match.markerEnd;
    return true;
}

bool BlockParser::matchUnorderedList(const QString &text, int &indent, int &contentOffset)
{
    UnorderedListLineMatch match;
    if (!parseUnorderedListLine(text, &match)) {
        return false;
    }
    indent = match.indent;
    contentOffset = match.markerEnd;
    return true;
}

bool BlockParser::matchHR(const QString &text)
{
    static QRegularExpression re(R"(^ {0,3}([-*_])(\s*\1){2,}\s*$)");
    return re.match(text).hasMatch();
}
