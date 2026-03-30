// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include <QTest>
#include "parser/LatexParser.h"
#include "parser/InlineParser.h"  // for InlineToken

class TestLatexParser : public QObject {
    Q_OBJECT

private slots:
    void testInlineMath()
    {
        QVector<InlineToken> tokens;
        int pos = 0;
        QString text = "$x^2$";
        bool result = LatexParser::parseInline(text, pos, tokens);

        QVERIFY(result);
        QVERIFY(tokens.size() >= 3);
        QCOMPARE(tokens[0].type, TokenType::LatexDelimiter);  // $
        QCOMPARE(tokens[1].type, TokenType::LatexMathBody);   // x^2
        QCOMPARE(tokens[2].type, TokenType::LatexDelimiter);  // $
        QCOMPARE(pos, 5);
    }

    void testInlineMathSpaceAfterDollar()
    {
        QVector<InlineToken> tokens;
        int pos = 0;
        QString text = "$ x$";
        bool result = LatexParser::parseInline(text, pos, tokens);

        // $ followed by space should NOT trigger inline math
        QVERIFY(!result);
    }

    void testDoubleDollarNotInline()
    {
        QVector<InlineToken> tokens;
        int pos = 0;
        QString text = "$$x$$";
        bool result = LatexParser::parseInline(text, pos, tokens);

        // $$ is display math, parseInline should skip
        QVERIFY(!result);
    }

    void testEscapedDollar()
    {
        QVector<InlineToken> tokens;
        int pos = 0;
        QString text = "$x \\$ y$";
        bool result = LatexParser::parseInline(text, pos, tokens);

        // \$ inside should not end the math
        QVERIFY(result);
        QCOMPARE(tokens[1].type, TokenType::LatexMathBody);
    }

    void testLatexBodyCommands()
    {
        QVector<InlineToken> tokens;
        QString text = "\\frac{a}{b}";
        LatexParser::parseLatexBody(text, 0, text.length(), tokens);

        bool foundCommand = false;
        bool foundBrace = false;
        for (const auto &t : tokens) {
            if (t.type == TokenType::LatexCommand) foundCommand = true;
            if (t.type == TokenType::LatexBrace) foundBrace = true;
        }
        QVERIFY(foundCommand);
        QVERIFY(foundBrace);
    }

    void testNestedBraces()
    {
        QVector<InlineToken> tokens;
        QString text = "\\frac{a+{b}}{c}";
        LatexParser::parseLatexBody(text, 0, text.length(), tokens);

        // Should handle nested braces without crashing
        QVERIFY(tokens.size() > 0);
    }

    void testNotTriggeredByNormalDollar()
    {
        QVector<InlineToken> tokens;
        int pos = 0;
        QString text = "Price is $5";

        // $ followed by digit — but our parser doesn't check that,
        // it checks for closing $. Since there's no closing $, should fail.
        bool result = LatexParser::parseInline(text, pos, tokens);
        // "5" has no closing $, so should not trigger
        // (depends on whether there's a second $ later - there isn't)
        QVERIFY(!result);
    }
};

QTEST_MAIN(TestLatexParser)
#include "test_latex_parser.moc"
