// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include <QTest>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

#include "highlight/MdHighlighter.h"
#include "config/Theme.h"

namespace {
QTextCharFormat formatAt(const QTextBlock &block, int column)
{
    if (!block.isValid() || !block.layout()) {
        return QTextCharFormat();
    }

    const auto ranges = block.layout()->formats();
    for (const auto &range : ranges) {
        if (column >= range.start && column < (range.start + range.length)) {
            return range.format;
        }
    }

    return QTextCharFormat();
}

QTextCharFormat firstCharFormat(const QTextDocument &doc, int blockNumber)
{
    const QTextBlock block = doc.findBlockByNumber(blockNumber);
    if (!block.isValid() || block.text().isEmpty()) {
        return QTextCharFormat();
    }
    return formatAt(block, 0);
}
}

class TestHighlighter : public QObject {
    Q_OBJECT

private slots:
    void testHtmlCommentKeepsCommentFormatForUnderlineLikeLines_data()
    {
        QTest::addColumn<QString>("underline");

        QTest::newRow("dash") << QString("---");
        QTest::newRow("equals") << QString("===");
    }

    void testHtmlCommentKeepsCommentFormatForUnderlineLikeLines()
    {
        QFETCH(QString, underline);

        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("<!--\ntheme:\n" + underline + "\n-->");
        highlighter.rehighlight();

        const QTextCharFormat fmt = firstCharFormat(doc, 2);
        QVERIFY(fmt.isValid());
        QCOMPARE(fmt.foreground().color(), theme.lineNumberFg);
        QVERIFY(fmt.fontItalic());
    }

    void testCodeFenceKeepsCodeFenceFormatForUnderlineLikeLines_data()
    {
        QTest::addColumn<QString>("underline");

        QTest::newRow("dash") << QString("---");
        QTest::newRow("equals") << QString("===");
    }

    void testCodeFenceKeepsCodeFenceFormatForUnderlineLikeLines()
    {
        QFETCH(QString, underline);

        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("```\n" + underline + "\n```");
        highlighter.rehighlight();

        const QTextCharFormat fmt = firstCharFormat(doc, 1);
        QVERIFY(fmt.isValid());
        QCOMPARE(fmt.foreground().color(), theme.codeFenceFg);
    }

    void testLatexDisplayKeepsMathBodyFormatForUnderlineLikeLines_data()
    {
        QTest::addColumn<QString>("underline");

        QTest::newRow("dash") << QString("---");
        QTest::newRow("equals") << QString("===");
    }

    void testLatexDisplayKeepsMathBodyFormatForUnderlineLikeLines()
    {
        QFETCH(QString, underline);

        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("$$\n" + underline + "\n$$");
        highlighter.rehighlight();

        const QTextCharFormat fmt = firstCharFormat(doc, 1);
        QVERIFY(fmt.isValid());
        QCOMPARE(fmt.foreground().color(), theme.latexMathBodyFg);
    }

    void testSetextUnderlineStillHighlighted()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("Title\n---");
        highlighter.rehighlight();

        const QTextCharFormat fmt = firstCharFormat(doc, 1);
        QVERIFY(fmt.isValid());
        QCOMPARE(fmt.foreground().color(), theme.markerFg);
    }

    void testSetextH1UnderlineStillHighlighted()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("Title\n===");
        highlighter.rehighlight();

        const QTextCharFormat fmt = firstCharFormat(doc, 1);
        QVERIFY(fmt.isValid());
        QCOMPARE(fmt.foreground().color(), theme.markerFg);
    }

    void testHrAfterBlankLineKeepsHrFormat()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("\n---");
        highlighter.rehighlight();

        const QTextCharFormat fmt = firstCharFormat(doc, 1);
        QVERIFY(fmt.isValid());
        QCOMPARE(fmt.foreground().color(), theme.hrFg);
    }
};

QTEST_MAIN(TestHighlighter)
#include "test_highlighter.moc"
