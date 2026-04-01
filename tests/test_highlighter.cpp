// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include <QTest>
#include <QCoreApplication>
#include <QTextBlock>
#include <QTextCursor>
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

    void testSetextHeadingLineUpdatesWhenUnderlineTypedLater()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("Title\n");
        highlighter.rehighlight();

        QTextCursor cursor(&doc);
        cursor.movePosition(QTextCursor::End);
        cursor.insertText("---");
        QCoreApplication::processEvents();

        const QTextCharFormat fmt = firstCharFormat(doc, 0);
        QVERIFY(fmt.isValid());
        QCOMPARE(fmt.foreground().color(), theme.heading[1]);
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

    void testBlockquoteUsesGrayBackground()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("> quote");
        highlighter.rehighlight();

        const QTextBlock block = doc.findBlockByNumber(0);
        const QTextCharFormat markFmt = formatAt(block, 0);
        const QTextCharFormat bodyFmt = formatAt(block, 2);

        QVERIFY(markFmt.isValid());
        QVERIFY(bodyFmt.isValid());

        QColor expected = theme.lineNumberBg;
        expected.setAlpha(84);
        QCOMPARE(markFmt.background().color(), expected);
        QCOMPARE(bodyFmt.background().color(), expected);
    }

    void testStrikethroughUsesStrikeOutFont()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("~~gone~~");
        highlighter.rehighlight();

        const QTextBlock block = doc.findBlockByNumber(0);
        const QTextCharFormat fmt = formatAt(block, 2);
        QVERIFY(fmt.isValid());
        QVERIFY(fmt.fontStrikeOut());
    }

    void testHighlightUsesBackgroundColor()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("==mark==");
        highlighter.rehighlight();

        const QTextBlock block = doc.findBlockByNumber(0);
        const QTextCharFormat fmt = formatAt(block, 2);
        QVERIFY(fmt.isValid());
        QCOMPARE(fmt.background().color(), theme.searchHighlightBg);
    }

    void testTaskCheckboxUsesBulletColor()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("- [x] done");
        highlighter.rehighlight();

        const QTextBlock block = doc.findBlockByNumber(0);
        const QTextCharFormat fmt = formatAt(block, 2);
        QVERIFY(fmt.isValid());
        QCOMPARE(fmt.foreground().color(), theme.listBulletFg);
    }

    void testAngleAutoLinkUsesLinkUrlColor()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("<https://example.com>");
        highlighter.rehighlight();

        const QTextBlock block = doc.findBlockByNumber(0);
        const QTextCharFormat fmt = formatAt(block, 1);
        QVERIFY(fmt.isValid());
        QCOMPARE(fmt.foreground().color(), theme.linkUrlFg);
    }

    void testLinkedImageLinkColoring()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);
        const QString text = "[![logo](img.png)](https://example.com)";

        doc.setPlainText(text);
        highlighter.rehighlight();

        const QTextBlock block = doc.findBlockByNumber(0);
        const int altPos = text.indexOf("logo");
        const int outerUrlPos = text.lastIndexOf("https://");
        QVERIFY(altPos >= 0);
        QVERIFY(outerUrlPos >= 0);

        const QTextCharFormat altFmt = formatAt(block, altPos);
        const QTextCharFormat urlFmt = formatAt(block, outerUrlPos);
        QVERIFY(altFmt.isValid());
        QVERIFY(urlFmt.isValid());
        QCOMPARE(altFmt.foreground().color(), theme.imageFg);
        QCOMPARE(urlFmt.foreground().color(), theme.linkUrlFg);
    }
};

QTEST_MAIN(TestHighlighter)
#include "test_highlighter.moc"
