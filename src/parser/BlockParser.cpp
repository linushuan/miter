#include "BlockParser.h"
#include <QRegularExpression>

BlockType BlockParser::classify(const QString &text, ContextStack &ctx, QVector<BlockToken> &tokens)
{
    tokens.clear();
    const int textLen = static_cast<int>(text.length());

    struct PrefixInfo {
        int  blockquoteDepth = 0;
        int  blockquoteEnd   = 0;
        bool hasList         = false;
        int  listStart       = 0;
        int  listEnd         = 0;
        int  contentOffset   = 0;
    };

    auto parseContainerPrefix = [&](const QString &line, int blockquoteLimit, bool parseList) {
        PrefixInfo info;
        int cursor = 0;

        while (blockquoteLimit < 0 || info.blockquoteDepth < blockquoteLimit) {
            int consumed = 0;
            if (!matchBlockquote(line.mid(cursor), consumed)) {
                break;
            }
            cursor += consumed;
            info.blockquoteDepth++;
        }

        info.blockquoteEnd = cursor;

        if (parseList) {
            int indent = 0;
            int consumed = 0;
            const QString afterBlockquote = line.mid(cursor);
            if (matchOrderedList(afterBlockquote, indent, consumed) ||
                matchUnorderedList(afterBlockquote, indent, consumed)) {
                info.hasList = true;
                info.listStart = cursor;
                info.listEnd = cursor + consumed;
                cursor += consumed;
            }
        }

        info.contentOffset = cursor;
        return info;
    };

    auto appendContainerMarkers = [&](const PrefixInfo &prefix) {
        if (prefix.blockquoteDepth > 0 && prefix.blockquoteEnd > 0) {
            tokens.append({0, prefix.blockquoteEnd, TokenType::BlockquoteMark});
        }
        if (prefix.hasList && prefix.listEnd > prefix.listStart) {
            tokens.append({prefix.listStart, prefix.listEnd - prefix.listStart, TokenType::ListBullet});
        }
    };

    // 1. Continue CodeFence
    if (ctx.inCode()) {
        const ContextFrame frame = ctx.top();
        const PrefixInfo prefix = parseContainerPrefix(text, frame.depth, frame.listIndent > 0);
        const int contentOffset = prefix.contentOffset;
        const QString content = text.mid(contentOffset);
        const int contentLen = static_cast<int>(content.length());

        appendContainerMarkers(prefix);

        if (matchCodeFenceEnd(content, frame.fenceChar, frame.fenceLen)) {
            tokens.append({contentOffset, contentLen, TokenType::CodeFenceMark});
            ctx.pop();
            return BlockType::CodeFenceEnd;
        }
        tokens.append({contentOffset, contentLen, TokenType::CodeFenceBody});
        return BlockType::CodeFenceBody;
    }

    // 2. Continue LatexDisplay
    if (ctx.topState() == BlockState::LatexDisplay) {
        const ContextFrame frame = ctx.top();
        const PrefixInfo prefix = parseContainerPrefix(text, frame.depth, frame.listIndent > 0);
        const int contentOffset = prefix.contentOffset;
        const QString content = text.mid(contentOffset);
        const int contentLen = static_cast<int>(content.length());

        appendContainerMarkers(prefix);

        if (content.trimmed() == "$$") {
            tokens.append({contentOffset, contentLen, TokenType::LatexDelimiter});
            ctx.pop();
            return BlockType::LatexDisplayEnd;
        }
        tokens.append({contentOffset, contentLen, TokenType::LatexMathBody});
        return BlockType::LatexDisplayBody;
    }

    // 3. Continue LatexEnv
    if (ctx.topState() == BlockState::LatexEnv) {
        const ContextFrame frame = ctx.top();
        const PrefixInfo prefix = parseContainerPrefix(text, frame.depth, frame.listIndent > 0);
        const int contentOffset = prefix.contentOffset;
        const QString content = text.mid(contentOffset);
        const int contentLen = static_cast<int>(content.length());
        const QString endPattern = "\\end{" + frame.envName + "}";

        appendContainerMarkers(prefix);

        if (content.contains(endPattern)) {
            tokens.append({contentOffset, contentLen, TokenType::LatexBeginEnd});
            ctx.pop();
            return BlockType::LatexEnvEnd;
        }
        tokens.append({contentOffset, contentLen, TokenType::LatexMathBody});
        return BlockType::LatexEnvBody;
    }

    const PrefixInfo prefix = parseContainerPrefix(text, -1, true);
    const int contentOffset = prefix.contentOffset;
    const QString content = text.mid(contentOffset);
    const int contentLen = static_cast<int>(content.length());

    // 4. ATX Heading
    int level, contentStart, contentEnd;
    if (matchATXHeading(content, level, contentStart, contentEnd)) {
        appendContainerMarkers(prefix);
        // # markers
        tokens.append({contentOffset, contentStart, TokenType::HeadingMarker});
        // Heading content
        TokenType headingType = static_cast<TokenType>(
            static_cast<int>(TokenType::HeadingH1) + level - 1);
        tokens.append({contentOffset + contentStart, contentEnd - contentStart, headingType});
        return BlockType::Heading;
    }

    // 5. CodeFence start
    QChar fenceChar;
    int fenceLen, indent;
    QString lang;
    if (matchCodeFenceStart(content, fenceChar, fenceLen, indent, lang)) {
        appendContainerMarkers(prefix);
        tokens.append({contentOffset, contentLen, TokenType::CodeFenceMark});
        if (!lang.isEmpty()) {
            // Find lang position and mark it
            const int langStart = content.indexOf(lang);
            if (langStart >= 0) {
                tokens.append({contentOffset + langStart, static_cast<int>(lang.length()), TokenType::CodeFenceLang});
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

    // 6. $$ display LaTeX start
    static QRegularExpression singleLineDisplayRe(R"(\$\$\s*(.+?)\s*\$\$)");
    auto displayMatch = singleLineDisplayRe.match(content);
    if (displayMatch.hasMatch()) {
        const int start = displayMatch.capturedStart(0);
        const int end = displayMatch.capturedEnd(0);
        const int innerStart = displayMatch.capturedStart(1);
        const int innerEnd = displayMatch.capturedEnd(1);

        appendContainerMarkers(prefix);

        tokens.append({contentOffset + start, 2, TokenType::LatexDelimiter});
        if (innerEnd > innerStart) {
            tokens.append({contentOffset + innerStart, innerEnd - innerStart, TokenType::LatexMathBody});
        }
        tokens.append({contentOffset + end - 2, 2, TokenType::LatexDelimiter});
        return BlockType::LatexDisplayBody;
    }

    if (content.trimmed() == "$$") {
        appendContainerMarkers(prefix);
        tokens.append({contentOffset, contentLen, TokenType::LatexDelimiter});
        ContextFrame frame;
        frame.state = BlockState::LatexDisplay;
        frame.depth = prefix.blockquoteDepth;
        frame.listIndent = prefix.hasList ? (prefix.listEnd - prefix.listStart) : 0;
        ctx.push(frame);
        return BlockType::LatexDisplayStart;
    }

    // 7. \begin{env} LaTeX env start
    static QRegularExpression beginRe(R"(\\begin\{(\w+)\})");
    auto beginMatch = beginRe.match(content);
    if (beginMatch.hasMatch()) {
        appendContainerMarkers(prefix);
        tokens.append({contentOffset, contentLen, TokenType::LatexBeginEnd});
        ContextFrame frame;
        frame.state = BlockState::LatexEnv;
        frame.envName = beginMatch.captured(1);
        frame.depth = prefix.blockquoteDepth;
        frame.listIndent = prefix.hasList ? (prefix.listEnd - prefix.listStart) : 0;
        ctx.push(frame);
        return BlockType::LatexEnvStart;
    }

    // 8. Table
    if (matchTable(content)) {
        appendContainerMarkers(prefix);
        // Tokenize pipe characters
        for (int i = 0; i < contentLen; ++i) {
            if (content[i] == '|') {
                tokens.append({contentOffset + i, 1, TokenType::TablePipe});
            }
        }
        return BlockType::Table;
    }

    // 9. Horizontal rule
    if (matchHR(content)) {
        appendContainerMarkers(prefix);
        tokens.append({contentOffset, contentLen, TokenType::HR});
        return BlockType::HR;
    }

    // 10. Blank line (non-container)
    if (content.trimmed().isEmpty() && prefix.blockquoteDepth == 0 && !prefix.hasList) {
        return BlockType::BlankLine;
    }

    // 11. Blockquote / list fallback
    if (prefix.hasList) {
        appendContainerMarkers(prefix);
        tokens.append({contentOffset, textLen - contentOffset, TokenType::ListBody});
        return BlockType::ListItem;
    }

    if (prefix.blockquoteDepth > 0) {
        appendContainerMarkers(prefix);
        tokens.append({contentOffset, textLen - contentOffset, TokenType::BlockquoteBody});
        return BlockType::Blockquote;
    }

    // 12. Normal paragraph
    return BlockType::Normal;
}

bool BlockParser::isSetextH1Underline(const QString &nextLine)
{
    static QRegularExpression re(R"(^={3,}\s*$)");
    return re.match(nextLine).hasMatch();
}

bool BlockParser::isSetextH2Underline(const QString &nextLine)
{
    static QRegularExpression re(R"(^-{3,}\s*$)");
    return re.match(nextLine).hasMatch();
}

bool BlockParser::matchCodeFenceStart(const QString &text, QChar &fenceChar, int &fenceLen, int &indent, QString &lang)
{
    static QRegularExpression re(R"(^( {0,3})(`{3,}|~{3,})(\w*)\s*$)");
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
    return text.contains('|');
}

bool BlockParser::matchBlockquote(const QString &text, int &contentOffset)
{
    static QRegularExpression re(R"(^ {0,3}> ?)");
    auto m = re.match(text);
    if (!m.hasMatch()) return false;
    contentOffset = m.capturedEnd(0);
    return true;
}

bool BlockParser::matchOrderedList(const QString &text, int &indent, int &contentOffset)
{
    static QRegularExpression re(R"(^(\s*)(\d{1,9})[.)](\s|$))");
    auto m = re.match(text);
    if (!m.hasMatch()) return false;
    indent = m.captured(1).length();
    contentOffset = m.capturedEnd(0);
    return true;
}

bool BlockParser::matchUnorderedList(const QString &text, int &indent, int &contentOffset)
{
    static QRegularExpression re(R"(^(\s*)([-*+])(\s|$))");
    auto m = re.match(text);
    if (!m.hasMatch()) return false;

    // Make sure it's not an HR (e.g., "---" or "***")
    QString trimmed = text.trimmed();
    const int trimmedLen = static_cast<int>(trimmed.length());
    if (trimmedLen >= 3) {
        bool allSame = true;
        QChar c = trimmed[0];
        for (int i = 1; i < trimmedLen; ++i) {
            if (trimmed[i] != c && !trimmed[i].isSpace()) {
                allSame = false;
                break;
            }
        }
        if (allSame && (c == '-' || c == '*' || c == '_'))
            return false; // This is an HR, not a list
    }

    indent = m.captured(1).length();
    contentOffset = m.capturedEnd(0);
    return true;
}

bool BlockParser::matchHR(const QString &text)
{
    static QRegularExpression re(R"(^ {0,3}([-*_])(\s*\1){2,}\s*$)");
    return re.match(text).hasMatch();
}
