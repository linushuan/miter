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

bool setextH1UnderlineContentMatches(const QString &content)
{
    static QRegularExpression re(R"(^ {0,3}={3,}\s*$)");
    return re.match(content).hasMatch();
}

bool setextH2UnderlineContentMatches(const QString &content)
{
    // Only classic dash underlines remain setext H2 markers.
    static QRegularExpression re(R"(^ {0,3}-{3,}\s*$)");
    return re.match(content).hasMatch();
}

bool hasSameContainerShapeForSetext(const PrefixInfo &a, const PrefixInfo &b)
{
    if (a.blockquoteDepth != b.blockquoteDepth || a.hasList != b.hasList) {
        return false;
    }
    if (!a.hasList) {
        return true;
    }
    return (a.listEnd - a.listStart) == (b.listEnd - b.listStart);
}

const QRegularExpression &tableSeparatorRegex()
{
    static const QRegularExpression re(
        R"(^\s*\|?\s*:?-{3,}:?\s*(\|\s*:?-{3,}:?\s*)+\|?\s*$)");
    return re;
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

BlockType BlockParser::classify(const QString &text,
                                ContextStack &ctx,
                                QVector<BlockToken> &tokens,
                                const QString &prevLine,
                                const QString &nextLine)
{
    tokens.clear();
    BlockType type = BlockType::Normal;
    if (classifyInOpenContext(text, ctx, tokens, type)) {
        return type;
    }
    return classifyInNormalContext(text, ctx, tokens, prevLine, nextLine);
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

        if (content.trimmed() == endPattern) {
            tokens.append({contentOffset, contentLen, TokenType::LatexBeginEnd});
            ctx.pop();
            type = BlockType::LatexEnvEnd;
            return true;
        }

        tokens.append({contentOffset, contentLen, TokenType::LatexMathBody});
        type = BlockType::LatexEnvBody;
        return true;
    }

    if (ctx.topState() == BlockState::Table) {
        const ContextFrame frame = ctx.top();
        const PrefixInfo prefix = parseContainerPrefix(text, frame.depth, frame.listIndent > 0);
        const int listIndent = prefix.hasList ? (prefix.listEnd - prefix.listStart) : 0;
        if (prefix.blockquoteDepth != frame.depth || listIndent != frame.listIndent) {
            ctx.pop();
            return false;
        }

        const int contentOffset = prefix.contentOffset;
        const QString content = text.mid(contentOffset);
        const int contentLen = static_cast<int>(content.length());

        if (!matchTable(content) && !matchTableSeparator(content)) {
            ctx.pop();
            return false;
        }

        appendContainerMarkers(prefix, tokens);
        for (int i = 0; i < contentLen; ++i) {
            if (content[i] == QLatin1Char('|')) {
                tokens.append({contentOffset + i, 1, TokenType::TablePipe});
            }
        }
        type = BlockType::Table;
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

namespace {
// Shared state for normal-context classification helpers.
struct ClassifyContext {
    const QString &text;
    const QString &prevLine;
    const QString &nextLine;
    int            textLen;
    PrefixInfo     prefix;
    int            contentOffset;
    QString        content;
    int            contentLen;
    int            firstNonSpace;
};

ClassifyContext buildClassifyContext(const QString &text,
                                     const QString &prevLine,
                                     const QString &nextLine)
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

    return {text, prevLine, nextLine, textLen, prefix, contentOffset, content, contentLen, firstNonSpace};
}

int containerListIndent(const PrefixInfo &prefix)
{
    return prefix.hasList ? (prefix.listEnd - prefix.listStart) : 0;
}

bool hasSameContainerShape(const PrefixInfo &a, const PrefixInfo &b)
{
    if (a.blockquoteDepth != b.blockquoteDepth || a.hasList != b.hasList) {
        return false;
    }
    if (!a.hasList) {
        return true;
    }
    return containerListIndent(a) == containerListIndent(b);
}

QString adjacentContentIfSameContainer(const QString &line, const PrefixInfo &currentPrefix)
{
    if (line.isNull()) {
        return QString();
    }

    const PrefixInfo candidatePrefix = parseContainerPrefix(line, -1, true);
    if (!hasSameContainerShape(candidatePrefix, currentPrefix)) {
        return QString();
    }
    return line.mid(candidatePrefix.contentOffset);
}

bool tryClassifyHtmlComment(ClassifyContext &c, ContextStack &ctx, QVector<BlockToken> &tokens, BlockType &out)
{
    if (c.firstNonSpace + 3 >= c.contentLen ||
        c.content[c.firstNonSpace]     != QLatin1Char('<') ||
        c.content[c.firstNonSpace + 1] != QLatin1Char('!') ||
        c.content[c.firstNonSpace + 2] != QLatin1Char('-') ||
        c.content[c.firstNonSpace + 3] != QLatin1Char('-')) {
        return false;
    }

    appendContainerMarkers(c.prefix, tokens);

    const int closePos = c.content.indexOf(QStringLiteral("-->"), c.firstNonSpace + 4);
    if (closePos >= 0) {
        tokens.append({
            c.contentOffset + c.firstNonSpace,
            closePos + 3 - c.firstNonSpace,
            TokenType::HtmlComment
        });
    } else {
        tokens.append({
            c.contentOffset + c.firstNonSpace,
            c.contentLen - c.firstNonSpace,
            TokenType::HtmlComment
        });
        ContextFrame frame;
        frame.state = BlockState::HtmlComment;
        frame.depth = c.prefix.blockquoteDepth;
        frame.listIndent = containerListIndent(c.prefix);
        ctx.push(frame);
    }
    out = BlockType::HtmlComment;
    return true;
}

bool tryClassifyATXHeading(ClassifyContext &c, QVector<BlockToken> &tokens, BlockType &out)
{
    int level = 0;
    int headingStart = 0;
    int headingEnd = 0;
    if (!BlockParser::matchATXHeading(c.content, level, headingStart, headingEnd)) {
        return false;
    }

    appendContainerMarkers(c.prefix, tokens);
    tokens.append({c.contentOffset, headingStart, TokenType::HeadingMarker});

    TokenType headingType = static_cast<TokenType>(
        static_cast<int>(TokenType::HeadingH1) + level - 1);
    tokens.append({c.contentOffset + headingStart, headingEnd - headingStart, headingType});
    out = BlockType::Heading;
    return true;
}

bool tryClassifyCodeFence(ClassifyContext &c, ContextStack &ctx, QVector<BlockToken> &tokens, BlockType &out)
{
    QChar fenceChar;
    int fenceLen = 0;
    int indent = 0;
    QString lang;
    if (!BlockParser::matchCodeFenceStart(c.content, fenceChar, fenceLen, indent, lang)) {
        return false;
    }

    appendContainerMarkers(c.prefix, tokens);
    tokens.append({c.contentOffset, c.contentLen, TokenType::CodeFenceMark});

    if (!lang.isEmpty()) {
        const int langStart = c.content.indexOf(lang);
        if (langStart >= 0) {
            tokens.append({
                c.contentOffset + langStart,
                static_cast<int>(lang.length()),
                TokenType::CodeFenceLang
            });
        }
    }

    ContextFrame frame;
    frame.state = BlockState::CodeFence;
    frame.fenceChar = fenceChar;
    frame.fenceLen = fenceLen;
    frame.depth = c.prefix.blockquoteDepth;
    frame.listIndent = containerListIndent(c.prefix);
    ctx.push(frame);
    out = BlockType::CodeFenceStart;
    return true;
}

bool tryClassifyLatex(ClassifyContext &c, ContextStack &ctx, QVector<BlockToken> &tokens, BlockType &out)
{
    // Single-line $$ ... $$
    static QRegularExpression singleLineDisplayRe(R"(^\s*(\$\$)\s*(.+?)\s*(\$\$)\s*$)");
    const auto displayMatch = singleLineDisplayRe.match(c.content);
    if (displayMatch.hasMatch()) {
        const int openStart  = displayMatch.capturedStart(1);
        const int openLen    = displayMatch.capturedLength(1);
        const int innerStart = displayMatch.capturedStart(2);
        const int innerEnd   = displayMatch.capturedEnd(2);
        const int closeStart = displayMatch.capturedStart(3);
        const int closeLen   = displayMatch.capturedLength(3);

        appendContainerMarkers(c.prefix, tokens);
        tokens.append({c.contentOffset + openStart, openLen, TokenType::LatexDelimiter});
        if (innerEnd > innerStart) {
            tokens.append({c.contentOffset + innerStart, innerEnd - innerStart, TokenType::LatexMathBody});
        }
        tokens.append({c.contentOffset + closeStart, closeLen, TokenType::LatexDelimiter});
        out = BlockType::LatexDisplayBody;
        return true;
    }

    // Standalone $$ fence
    if (BlockParser::isStandaloneLatexDisplayFence(c.content)) {
        appendContainerMarkers(c.prefix, tokens);
        tokens.append({c.contentOffset, c.contentLen, TokenType::LatexDelimiter});

        ContextFrame frame;
        frame.state = BlockState::LatexDisplay;
        frame.depth = c.prefix.blockquoteDepth;
        frame.listIndent = containerListIndent(c.prefix);
        ctx.push(frame);
        out = BlockType::LatexDisplayStart;
        return true;
    }

    // \begin{env}
    static QRegularExpression beginRe(R"(^\s*\\begin\{([^\}\s]+)\}\s*$)");
    const auto beginMatch = beginRe.match(c.content);
    if (beginMatch.hasMatch()) {
        appendContainerMarkers(c.prefix, tokens);
        tokens.append({c.contentOffset, c.contentLen, TokenType::LatexBeginEnd});

        ContextFrame frame;
        frame.state = BlockState::LatexEnv;
        frame.envName = beginMatch.captured(1);
        frame.depth = c.prefix.blockquoteDepth;
        frame.listIndent = containerListIndent(c.prefix);
        ctx.push(frame);
        out = BlockType::LatexEnvStart;
        return true;
    }

    return false;
}

bool tryClassifyTable(ClassifyContext &c, ContextStack &ctx, QVector<BlockToken> &tokens, BlockType &out)
{
    const bool isSeparator = BlockParser::matchTableSeparator(c.content);
    const QString prevContent = adjacentContentIfSameContainer(c.prevLine, c.prefix);
    const QString nextContent = adjacentContentIfSameContainer(c.nextLine, c.prefix);

    const bool hasHeaderAbove = isSeparator && BlockParser::matchTable(prevContent);
    const bool hasSeparatorAbove = BlockParser::matchTable(c.content) && BlockParser::matchTableSeparator(prevContent);
    const bool hasSeparatorBelow = BlockParser::matchTable(c.content) && BlockParser::matchTableSeparator(nextContent);
    if (!hasHeaderAbove && !hasSeparatorAbove && !hasSeparatorBelow) {
        return false;
    }

    appendContainerMarkers(c.prefix, tokens);
    for (int i = 0; i < c.contentLen; ++i) {
        if (c.content[i] == QLatin1Char('|')) {
            tokens.append({c.contentOffset + i, 1, TokenType::TablePipe});
        }
    }

    if (hasHeaderAbove || hasSeparatorAbove) {
        ContextFrame frame;
        frame.state = BlockState::Table;
        frame.depth = c.prefix.blockquoteDepth;
        frame.listIndent = containerListIndent(c.prefix);
        ctx.push(frame);
    }

    out = BlockType::Table;
    return true;
}

bool tryClassifyHR(ClassifyContext &c, QVector<BlockToken> &tokens, BlockType &out)
{
    if (!BlockParser::matchHR(c.content)) {
        return false;
    }

    appendContainerMarkers(c.prefix, tokens);
    tokens.append({c.contentOffset, c.contentLen, TokenType::HR});
    out = BlockType::HR;
    return true;
}

bool tryClassifyContainer(ClassifyContext &c, QVector<BlockToken> &tokens, BlockType &out)
{
    if (c.content.trimmed().isEmpty() && c.prefix.blockquoteDepth == 0 && !c.prefix.hasList) {
        out = BlockType::BlankLine;
        return true;
    }

    if (c.prefix.hasList) {
        appendContainerMarkers(c.prefix, tokens);
        tokens.append({c.contentOffset, c.textLen - c.contentOffset, TokenType::ListBody});

        int checkboxStart = 0;
        int checkboxLen = 0;
        if (matchTaskCheckboxPrefix(c.content, &checkboxStart, &checkboxLen) && checkboxLen > 0) {
            tokens.append({c.contentOffset + checkboxStart, checkboxLen, TokenType::CheckboxMarker});
        }
        out = BlockType::ListItem;
        return true;
    }

    if (c.prefix.blockquoteDepth > 0) {
        appendContainerMarkers(c.prefix, tokens);
        tokens.append({c.contentOffset, c.textLen - c.contentOffset, TokenType::BlockquoteBody});
        out = BlockType::Blockquote;
        return true;
    }

    return false;
}
}

BlockType BlockParser::classifyInNormalContext(const QString &text,
                                               ContextStack &ctx,
                                               QVector<BlockToken> &tokens,
                                               const QString &prevLine,
                                               const QString &nextLine)
{
    ClassifyContext c = buildClassifyContext(text, prevLine, nextLine);
    BlockType type;

    if (tryClassifyHtmlComment(c, ctx, tokens, type)) return type;
    if (tryClassifyATXHeading(c, tokens, type))       return type;
    if (tryClassifyCodeFence(c, ctx, tokens, type))   return type;
    if (tryClassifyLatex(c, ctx, tokens, type))       return type;
    if (tryClassifyTable(c, ctx, tokens, type))       return type;
    if (tryClassifyHR(c, tokens, type))               return type;
    if (tryClassifyContainer(c, tokens, type))        return type;

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
    const PrefixInfo prefix = parseContainerPrefix(nextLine, -1, true);
    return setextH1UnderlineContentMatches(nextLine.mid(prefix.contentOffset));
}

bool BlockParser::isSetextH2Underline(const QString &nextLine)
{
    const PrefixInfo prefix = parseContainerPrefix(nextLine, -1, true);
    return setextH2UnderlineContentMatches(nextLine.mid(prefix.contentOffset));
}

bool BlockParser::isSetextUnderlineForHeadingLine(const QString &headingLine,
                                                  const QString &underlineLine,
                                                  bool *isH1,
                                                  bool *isH2)
{
    if (isH1) {
        *isH1 = false;
    }
    if (isH2) {
        *isH2 = false;
    }

    const PrefixInfo headingPrefix = parseContainerPrefix(headingLine, -1, true);
    const PrefixInfo underlinePrefix = parseContainerPrefix(underlineLine, -1, true);
    if (!hasSameContainerShapeForSetext(headingPrefix, underlinePrefix)) {
        return false;
    }

    const QString headingContent = headingLine.mid(headingPrefix.contentOffset);
    if (headingContent.trimmed().isEmpty()) {
        return false;
    }

    const QString underlineContent = underlineLine.mid(underlinePrefix.contentOffset);
    const bool localH1 = setextH1UnderlineContentMatches(underlineContent);
    const bool localH2 = setextH2UnderlineContentMatches(underlineContent);

    if (isH1) {
        *isH1 = localH1;
    }
    if (isH2) {
        *isH2 = localH2;
    }

    return localH1 || localH2;
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

    int end = static_cast<int>(text.length());
    while (end > contentStart && text[end - 1].isSpace()) {
        --end;
    }

    int hashRunStart = end;
    while (hashRunStart > contentStart && text[hashRunStart - 1] == QLatin1Char('#')) {
        --hashRunStart;
    }

    if (hashRunStart < end &&
        hashRunStart > contentStart &&
        text[hashRunStart - 1].isSpace()) {
        end = hashRunStart - 1;
        while (end > contentStart && text[end - 1].isSpace()) {
            --end;
        }
    }

    contentEnd = end;
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

    if (matchTableSeparator(trimmed)) {
        return false;
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

bool BlockParser::matchTableSeparator(const QString &text)
{
    return tableSeparatorRegex().match(text.trimmed()).hasMatch();
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
