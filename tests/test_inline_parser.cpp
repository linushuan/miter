// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include <QTest>
#include "parser/InlineParser.h"

class TestInlineParser : public QObject {
    Q_OBJECT

private slots:
    void testBold()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("**bold**", 0, ctx, tokens);

        // Expect: BoldMarker(0,2), Bold(2,4), BoldMarker(6,2)
        QVERIFY(tokens.size() >= 3);
        QCOMPARE(tokens[0].type, TokenType::BoldMarker);
        QCOMPARE(tokens[0].start, 0);
        QCOMPARE(tokens[0].length, 2);
        QCOMPARE(tokens[1].type, TokenType::Bold);
        QCOMPARE(tokens[2].type, TokenType::BoldMarker);
    }

    void testItalic()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("*italic*", 0, ctx, tokens);

        QVERIFY(tokens.size() >= 3);
        QCOMPARE(tokens[0].type, TokenType::ItalicMarker);
        QCOMPARE(tokens[1].type, TokenType::Italic);
        QCOMPARE(tokens[2].type, TokenType::ItalicMarker);
    }

    void testInlineCode()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("`code`", 0, ctx, tokens);

        QVERIFY(tokens.size() >= 3);
        QCOMPARE(tokens[0].type, TokenType::InlineCodeMark);
        QCOMPARE(tokens[1].type, TokenType::InlineCode);
        QCOMPARE(tokens[2].type, TokenType::InlineCodeMark);
    }

    void testInlineCodeContainingDoubleDollar()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("``$$x^2$$``", 0, ctx, tokens);

        QVERIFY(tokens.size() >= 3);
        QCOMPARE(tokens[0].type, TokenType::InlineCodeMark);
        QCOMPARE(tokens[1].type, TokenType::InlineCode);
        QCOMPARE(tokens[2].type, TokenType::InlineCodeMark);

        for (const auto &t : tokens) {
            QVERIFY(t.type != TokenType::LatexDelimiter);
            QVERIFY(t.type != TokenType::LatexMathBody);
        }
    }

    void testLink()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("[link](url)", 0, ctx, tokens);

        QVERIFY(tokens.size() >= 5);
        QCOMPARE(tokens[0].type, TokenType::LinkBracket);
        QCOMPARE(tokens[1].type, TokenType::LinkText);
        QCOMPARE(tokens[2].type, TokenType::LinkBracket);
        QCOMPARE(tokens[3].type, TokenType::LinkUrl);
        QCOMPARE(tokens[4].type, TokenType::LinkBracket);
    }

    void testAngleAutoLink()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("<https://example.com>", 0, ctx, tokens);

        QVERIFY(tokens.size() >= 3);
        QCOMPARE(tokens[0].type, TokenType::LinkBracket);
        QCOMPARE(tokens[1].type, TokenType::LinkUrl);
        QCOMPARE(tokens[2].type, TokenType::LinkBracket);
    }

    void testAngleAutoLinkRejectsNonLinkText()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("<TagName>", 0, ctx, tokens);

        for (const auto &token : tokens) {
            QVERIFY(token.type != TokenType::LinkUrl);
            QVERIFY(token.type != TokenType::LinkBracket);
        }
    }

    void testAngleAutoLinkEmail()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("<user@example.com>", 0, ctx, tokens);

        QVERIFY(tokens.size() >= 3);
        QCOMPARE(tokens[0].type, TokenType::LinkBracket);
        QCOMPARE(tokens[1].type, TokenType::LinkUrl);
        QCOMPARE(tokens[2].type, TokenType::LinkBracket);
    }

    void testImage()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("![alt](img.png)", 0, ctx, tokens);

        QVERIFY(tokens.size() >= 5);
        QCOMPARE(tokens[0].type, TokenType::ImageBracket);
        QCOMPARE(tokens[1].type, TokenType::ImageAlt);
    }

    void testLinkedImageLink()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("[![logo](img.png)](https://example.com)", 0, ctx, tokens);

        QVERIFY(tokens.size() >= 9);
        QCOMPARE(tokens[0].type, TokenType::LinkBracket);
        QCOMPARE(tokens[1].type, TokenType::ImageBracket);
        QCOMPARE(tokens[2].type, TokenType::ImageAlt);
        QCOMPARE(tokens[3].type, TokenType::ImageBracket);
        QCOMPARE(tokens[4].type, TokenType::ImageUrl);
        QCOMPARE(tokens[5].type, TokenType::ImageBracket);
        QCOMPARE(tokens[6].type, TokenType::LinkBracket);
        QCOMPARE(tokens[7].type, TokenType::LinkUrl);
        QCOMPARE(tokens[8].type, TokenType::LinkBracket);
    }

    void testUnderlinePlusPlus()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("++underlined++", 0, ctx, tokens);

        QVERIFY(tokens.size() >= 3);
        QCOMPARE(tokens[0].type, TokenType::UnderlineMarker);
        QCOMPARE(tokens[1].type, TokenType::Underline);
        QCOMPARE(tokens[2].type, TokenType::UnderlineMarker);
    }

    void testUnderlineRejectsSpaceWrappedContent()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("++ spaced ++", 0, ctx, tokens);

        for (const auto &token : tokens) {
            QVERIFY(token.type != TokenType::Underline);
            QVERIFY(token.type != TokenType::UnderlineMarker);
        }
    }

    void testHighlightEqualsEquals()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("==marked==", 0, ctx, tokens);

        QVERIFY(tokens.size() >= 3);
        QCOMPARE(tokens[0].type, TokenType::HighlightMarker);
        QCOMPARE(tokens[1].type, TokenType::Highlight);
        QCOMPARE(tokens[2].type, TokenType::HighlightMarker);
    }

    void testSuperscriptAndSubscript()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("x^2^ H~2~O", 0, ctx, tokens);

        bool hasSuperscript = false;
        bool hasSubscript = false;
        for (const auto &token : tokens) {
            if (token.type == TokenType::Superscript) {
                hasSuperscript = true;
            }
            if (token.type == TokenType::Subscript) {
                hasSubscript = true;
            }
        }

        QVERIFY(hasSuperscript);
        QVERIFY(hasSubscript);
    }

    void testSuperscriptAndSubscriptRejectWhitespace()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("x^ 2^ H~ 2~O", 0, ctx, tokens);

        for (const auto &token : tokens) {
            QVERIFY(token.type != TokenType::Superscript);
            QVERIFY(token.type != TokenType::Subscript);
        }
    }

    void testStrikethrough()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("~~strike~~", 0, ctx, tokens);

        QVERIFY(tokens.size() >= 3);
        QCOMPARE(tokens[0].type, TokenType::StrikeMarker);
        QCOMPARE(tokens[1].type, TokenType::Strikethrough);
        QCOMPARE(tokens[2].type, TokenType::StrikeMarker);
    }

    void testEscape()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("\\*escaped\\*", 0, ctx, tokens);

        // Should produce Escape tokens, NOT italic
        for (const auto &t : tokens) {
            QVERIFY(t.type != TokenType::Italic);
            QVERIFY(t.type != TokenType::ItalicMarker);
        }
    }

    void testHardBreakSpace()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("text  ", 0, ctx, tokens);

        bool found = false;
        for (const auto &t : tokens) {
            if (t.type == TokenType::HardBreakSpace) found = true;
        }
        QVERIFY(found);
    }

    void testHardBreakBackslash()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("text\\", 0, ctx, tokens);

        bool found = false;
        for (const auto &t : tokens) {
            if (t.type == TokenType::HardBreakBackslash) found = true;
        }
        QVERIFY(found);
    }

    void testCjkBold()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        QString text = QString::fromUtf8("中文**粗體**中文");
        InlineParser::parse(text, 0, ctx, tokens);

        bool foundBold = false;
        for (const auto &t : tokens) {
            if (t.type == TokenType::Bold) foundBold = true;
        }
        QVERIFY(foundBold);
    }

    void testOffset()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("prefix **bold**", 7, ctx, tokens);

        QVERIFY(tokens.size() >= 3);
        QCOMPARE(tokens[0].type, TokenType::BoldMarker);
        QCOMPARE(tokens[0].start, 7);
    }

    void testHtmlCommentInline()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("A <!-- note `x` --> B", 0, ctx, tokens);

        bool foundComment = false;
        for (const auto &t : tokens) {
            if (t.type == TokenType::HtmlComment) {
                foundComment = true;
                QCOMPARE(t.start, 2);
                QCOMPARE(t.length, 17);
            }
            QVERIFY(t.type != TokenType::InlineCode);
            QVERIFY(t.type != TokenType::InlineCodeMark);
        }
        QVERIFY(foundComment);
    }

    void testTokenRangesRemainInBoundsForEdgeSamples()
    {
        const QStringList samples = {
            QStringLiteral("<https://example.com>"),
            QStringLiteral("<user@example.com>"),
            QStringLiteral("[![圖](img.png)](https://example.com)"),
            QStringLiteral("==mark== ++under++ ~~gone~~"),
            QStringLiteral("x^2^ H~2~O"),
            QStringLiteral("``$x$`` and `code`"),
            QStringLiteral("\\*escaped\\*")
        };

        ContextStack ctx;
        QVector<InlineToken> tokens;
        for (const QString &sample : samples) {
            InlineParser::parse(sample, 0, ctx, tokens);
            for (const auto &token : tokens) {
                QVERIFY(token.start >= 0);
                QVERIFY(token.length >= 0);
                QVERIFY(token.start + token.length <= sample.length());
            }
        }
    }
};

QTEST_MAIN(TestInlineParser)
#include "test_inline_parser.moc"
