// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include <QTest>
#include "parser/BlockParser.h"

class TestBlockParser : public QObject {
    Q_OBJECT

private slots:
    void testATXHeading()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("# Heading 1", ctx, tokens), BlockType::Heading);
        QCOMPARE(BlockParser::classify("## Heading 2", ctx, tokens), BlockType::Heading);
        QCOMPARE(BlockParser::classify("###### H6", ctx, tokens), BlockType::Heading);
    }

    void testCodeFenceStart()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("```cpp", ctx, tokens), BlockType::CodeFenceStart);
        QVERIFY(ctx.inCode());
    }

    void testCodeFenceBody()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        // Start fence
        BlockParser::classify("```", ctx, tokens);
        QVERIFY(ctx.inCode());

        // Body
        QCOMPARE(BlockParser::classify("some code here", ctx, tokens), BlockType::CodeFenceBody);

        // End fence
        QCOMPARE(BlockParser::classify("```", ctx, tokens), BlockType::CodeFenceEnd);
        QVERIFY(!ctx.inCode());
    }

    void testTildeFence()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        BlockParser::classify("~~~python", ctx, tokens);
        QVERIFY(ctx.inCode());

        // ``` does NOT close a ~~~ fence
        QCOMPARE(BlockParser::classify("```", ctx, tokens), BlockType::CodeFenceBody);

        // ~~~ closes it
        QCOMPARE(BlockParser::classify("~~~", ctx, tokens), BlockType::CodeFenceEnd);
    }

    void testBlockquote()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("> quote", ctx, tokens), BlockType::Blockquote);
    }

    void testNestedBlockquoteDepthToken()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("> > deep", ctx, tokens), BlockType::Blockquote);
        QVERIFY(!tokens.isEmpty());
        QCOMPARE(tokens[0].type, TokenType::BlockquoteMark);
        QCOMPARE(tokens[0].start, 0);
        QCOMPARE(tokens[0].length, 4);
    }

    void testUnorderedList()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("- item", ctx, tokens), BlockType::ListItem);
        QCOMPARE(BlockParser::classify("* item", ctx, tokens), BlockType::ListItem);
        QCOMPARE(BlockParser::classify("+ item", ctx, tokens), BlockType::ListItem);
        QCOMPARE(BlockParser::classify("    - nested item", ctx, tokens), BlockType::ListItem);
    }

    void testOrderedList()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("1. item", ctx, tokens), BlockType::ListItem);
        QCOMPARE(BlockParser::classify("99) item", ctx, tokens), BlockType::ListItem);
        QCOMPARE(BlockParser::classify("        2. nested item", ctx, tokens), BlockType::ListItem);
    }

    void testHR()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("---", ctx, tokens), BlockType::HR);
        QCOMPARE(BlockParser::classify("***", ctx, tokens), BlockType::HR);
        QCOMPARE(BlockParser::classify("___", ctx, tokens), BlockType::HR);
        QCOMPARE(BlockParser::classify("- - -", ctx, tokens), BlockType::HR);
        QCOMPARE(BlockParser::classify("* * *", ctx, tokens), BlockType::HR);
    }

    void testTable()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("| col1 | col2 |", ctx, tokens), BlockType::Table);
    }

    void testSinglePipeTextIsNotTable()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("a | b", ctx, tokens), BlockType::Normal);
        QCOMPARE(BlockParser::classify("plain|text", ctx, tokens), BlockType::Normal);
    }

    void testLatexDisplayStart()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("$$", ctx, tokens), BlockType::LatexDisplayStart);
        QCOMPARE(ctx.topState(), BlockState::LatexDisplay);

        QCOMPARE(BlockParser::classify("E = mc^2", ctx, tokens), BlockType::LatexDisplayBody);
        QCOMPARE(BlockParser::classify("$$", ctx, tokens), BlockType::LatexDisplayEnd);
    }

    void testSingleLineLatexDisplay()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("$$x^2 + y^2$$", ctx, tokens), BlockType::LatexDisplayBody);
        QVERIFY(!tokens.isEmpty());
    }

    void testSingleLineLatexDisplayNotTriggeredInsideInlineCode()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("``$$x^2$$``", ctx, tokens), BlockType::Normal);
    }

    void testLatexDisplayInBlockquote()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("> $$", ctx, tokens), BlockType::LatexDisplayStart);
        QVERIFY(ctx.inLatex());
        QCOMPARE(ctx.top().depth, 1);

        QCOMPARE(BlockParser::classify("> E = mc^2", ctx, tokens), BlockType::LatexDisplayBody);
        QCOMPARE(BlockParser::classify("> $$", ctx, tokens), BlockType::LatexDisplayEnd);
        QVERIFY(!ctx.inLatex());
    }

    void testLatexDisplayInList()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("- $$", ctx, tokens), BlockType::LatexDisplayStart);
        QVERIFY(ctx.inLatex());
        QVERIFY(ctx.top().listIndent > 0);

        QCOMPARE(BlockParser::classify("  E = mc^2", ctx, tokens), BlockType::LatexDisplayBody);
        QCOMPARE(BlockParser::classify("  $$", ctx, tokens), BlockType::LatexDisplayEnd);
        QVERIFY(!ctx.inLatex());
    }

    void testLatexEnvStartsAtLineStartOnly()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("\\begin{equation}", ctx, tokens), BlockType::LatexEnvStart);
        QVERIFY(ctx.inLatex());
        QCOMPARE(ctx.topState(), BlockState::LatexEnv);
    }

    void testLatexEnvWithStarName()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("\\begin{align*}", ctx, tokens), BlockType::LatexEnvStart);
        QVERIFY(ctx.inLatex());
        QCOMPARE(ctx.top().envName, QString("align*"));

        QCOMPARE(BlockParser::classify("x &= y", ctx, tokens), BlockType::LatexEnvBody);
        QCOMPARE(BlockParser::classify("\\end{align*}", ctx, tokens), BlockType::LatexEnvEnd);
        QVERIFY(!ctx.inLatex());
    }

    void testLatexEnvDoesNotStartWithTrailingContent()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("\\begin{equation} x = 1", ctx, tokens), BlockType::Normal);
        QVERIFY(!ctx.inLatex());
    }

    void testLatexEnvInlineMentionDoesNotStartBlock()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("目標：`\\begin{env}` 有顏色。", ctx, tokens), BlockType::Normal);
        QVERIFY(!ctx.inLatex());
    }

    void testInlineBeginMentionDoesNotBreakFenceAndHeading()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(
            BlockParser::classify("目標：`$...$`、`$$...$$`、`\\cmd`、`\\begin{env}` 有顏色。", ctx, tokens),
            BlockType::Normal
        );
        QVERIFY(!ctx.inLatex());

        QCOMPARE(BlockParser::classify("```", ctx, tokens), BlockType::CodeFenceStart);
        QVERIFY(ctx.inCode());

        QCOMPARE(BlockParser::classify("[x] nested brace {} 計數", ctx, tokens), BlockType::CodeFenceBody);
        QVERIFY(ctx.inCode());

        QCOMPARE(BlockParser::classify("```", ctx, tokens), BlockType::CodeFenceEnd);
        QVERIFY(!ctx.inCode());

        QCOMPARE(BlockParser::classify("### Milestone 5：搜尋與 UX 完善（1–2 天）", ctx, tokens), BlockType::Heading);
    }

    void testCodeFenceInBlockquote()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("> ```cpp", ctx, tokens), BlockType::CodeFenceStart);
        QVERIFY(ctx.inCode());
        QCOMPARE(ctx.top().depth, 1);

        QCOMPARE(BlockParser::classify("> int x = 42;", ctx, tokens), BlockType::CodeFenceBody);
        QCOMPARE(BlockParser::classify("> ```", ctx, tokens), BlockType::CodeFenceEnd);
        QVERIFY(!ctx.inCode());
    }

    void testListInBlockquoteTokens()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("> - item", ctx, tokens), BlockType::ListItem);

        bool hasBlockquoteMark = false;
        bool hasListBullet = false;
        for (const auto &token : tokens) {
            if (token.type == TokenType::BlockquoteMark) hasBlockquoteMark = true;
            if (token.type == TokenType::ListBullet) hasListBullet = true;
        }
        QVERIFY(hasBlockquoteMark);
        QVERIFY(hasListBullet);
    }

    void testBlankLine()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("", ctx, tokens), BlockType::BlankLine);
        QCOMPARE(BlockParser::classify("   ", ctx, tokens), BlockType::BlankLine);
    }

    void testNormal()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("Just some text", ctx, tokens), BlockType::Normal);
    }

    void testSetextDetection()
    {
        QVERIFY(BlockParser::isSetextH1Underline("==="));
        QVERIFY(BlockParser::isSetextH1Underline("========"));
        QVERIFY(BlockParser::isSetextH1Underline("   ===="));
        QVERIFY(!BlockParser::isSetextH1Underline("=="));

        QVERIFY(BlockParser::isSetextH2Underline("---"));
        QVERIFY(BlockParser::isSetextH2Underline("--------"));
        QVERIFY(BlockParser::isSetextH2Underline("   -----"));
        QVERIFY(BlockParser::isSetextH2Underline("***"));
        QVERIFY(BlockParser::isSetextH2Underline("* * *"));
        QVERIFY(BlockParser::isSetextH2Underline("- - -"));
        QVERIFY(!BlockParser::isSetextH2Underline("--"));
        QVERIFY(!BlockParser::isSetextH2Underline("___"));
    }

    void testListItemCheckboxToken()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("- [x] done", ctx, tokens), BlockType::ListItem);

        bool hasCheckbox = false;
        for (const auto &token : tokens) {
            if (token.type == TokenType::CheckboxMarker) {
                hasCheckbox = true;
                break;
            }
        }
        QVERIFY(hasCheckbox);
    }

    void testIndentedCodeNotFence()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        // 4+ spaces before ``` is not a fence
        QCOMPARE(BlockParser::classify("    ```", ctx, tokens), BlockType::Normal);
    }

    void testCodeFenceInfoStringWithSymbols()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("```c++", ctx, tokens), BlockType::CodeFenceStart);

        bool hasFenceLang = false;
        for (const auto &token : tokens) {
            if (token.type == TokenType::CodeFenceLang) {
                hasFenceLang = true;
                break;
            }
        }
        QVERIFY(hasFenceLang);
    }

    void testTokenRangesRemainInBoundsForEdgeSamples()
    {
        const QStringList lines = {
            QStringLiteral("***"),
            QStringLiteral("- - -"),
            QStringLiteral("* * *"),
            QStringLiteral("> > - [x] done"),
            QStringLiteral("| head | value |"),
            QStringLiteral("```python"),
            QStringLiteral("\t```"),
            QStringLiteral("\\begin{align*}"),
            QStringLiteral("<https://example.com>"),
            QStringLiteral("[![img](a.png)](https://example.com)")
        };

        ContextStack ctx;
        QVector<BlockToken> tokens;

        for (const QString &line : lines) {
            BlockParser::classify(line, ctx, tokens);
            const int lineLen = line.length();
            for (const auto &token : tokens) {
                QVERIFY(token.start >= 0);
                QVERIFY(token.length >= 0);
                QVERIFY(token.start + token.length <= lineLen);
            }
        }
    }

    void testHtmlCommentSingleLine()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("<!-- hidden -->", ctx, tokens), BlockType::HtmlComment);
        QVERIFY(!tokens.isEmpty());
        QCOMPARE(tokens[0].type, TokenType::HtmlComment);
        QCOMPARE(ctx.topState(), BlockState::Normal);
    }

    void testHtmlCommentMultiline()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("<!--", ctx, tokens), BlockType::HtmlComment);
        QCOMPARE(ctx.topState(), BlockState::HtmlComment);

        QCOMPARE(BlockParser::classify("still hidden", ctx, tokens), BlockType::HtmlComment);
        QCOMPARE(ctx.topState(), BlockState::HtmlComment);

        QCOMPARE(BlockParser::classify("-->", ctx, tokens), BlockType::HtmlComment);
        QCOMPARE(ctx.topState(), BlockState::Normal);
    }
};

QTEST_MAIN(TestBlockParser)
#include "test_block_parser.moc"
