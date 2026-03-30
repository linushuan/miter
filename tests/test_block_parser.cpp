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
    }

    void testTable()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        QCOMPARE(BlockParser::classify("| col1 | col2 |", ctx, tokens), BlockType::Table);
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
        QVERIFY(!BlockParser::isSetextH1Underline("=="));

        QVERIFY(BlockParser::isSetextH2Underline("---"));
        QVERIFY(BlockParser::isSetextH2Underline("--------"));
        QVERIFY(!BlockParser::isSetextH2Underline("--"));
    }

    void testIndentedCodeNotFence()
    {
        ContextStack ctx;
        QVector<BlockToken> tokens;

        // 4+ spaces before ``` is not a fence
        QCOMPARE(BlockParser::classify("    ```", ctx, tokens), BlockType::Normal);
    }
};

QTEST_MAIN(TestBlockParser)
#include "test_block_parser.moc"
