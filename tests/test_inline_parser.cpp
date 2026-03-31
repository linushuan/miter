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

    void testImage()
    {
        ContextStack ctx;
        QVector<InlineToken> tokens;
        InlineParser::parse("![alt](img.png)", 0, ctx, tokens);

        QVERIFY(tokens.size() >= 5);
        QCOMPARE(tokens[0].type, TokenType::ImageBracket);
        QCOMPARE(tokens[1].type, TokenType::ImageAlt);
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
};

QTEST_MAIN(TestInlineParser)
#include "test_inline_parser.moc"
