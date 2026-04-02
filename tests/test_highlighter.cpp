// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include <QTest>
#include <QCoreApplication>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QFont>

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

int colorDistance(const QColor &a, const QColor &b)
{
    return qAbs(a.red() - b.red()) +
           qAbs(a.green() - b.green()) +
           qAbs(a.blue() - b.blue());
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

    void testInlineLatexEmptyPairWhileTypingUsesDelimiterFormat()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        const QString text = QStringLiteral("sum = $$ + 1");
        doc.setPlainText(text);
        highlighter.rehighlight();

        const QTextBlock block = doc.findBlockByNumber(0);
        const int mathStart = text.indexOf(QStringLiteral("$$"));
        QVERIFY(mathStart >= 0);

        const QTextCharFormat openFmt = formatAt(block, mathStart);
        const QTextCharFormat closeFmt = formatAt(block, mathStart + 1);
        QVERIFY(openFmt.isValid());
        QVERIFY(closeFmt.isValid());
        QCOMPARE(openFmt.foreground().color(), theme.latexDelimiterFg);
        QCOMPARE(closeFmt.foreground().color(), theme.latexDelimiterFg);
    }

    void testInlineLatexAdjacentSegmentsAllHighlighted()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        const QString text = QStringLiteral("$adfsf$$adfsdf$$adfasdaf$");
        doc.setPlainText(text);
        highlighter.rehighlight();

        const QTextBlock block = doc.findBlockByNumber(0);
        const int secondStart = text.indexOf(QStringLiteral("$adfsdf$"));
        const int thirdStart = text.indexOf(QStringLiteral("$adfasdaf$"));
        QVERIFY(secondStart >= 0);
        QVERIFY(thirdStart >= 0);

        const QTextCharFormat secondBodyFmt = formatAt(block, secondStart + 1);
        const QTextCharFormat thirdBodyFmt = formatAt(block, thirdStart + 1);
        QVERIFY(secondBodyFmt.isValid());
        QVERIFY(thirdBodyFmt.isValid());
        QCOMPARE(secondBodyFmt.foreground().color(), theme.latexMathBodyFg);
        QCOMPARE(thirdBodyFmt.foreground().color(), theme.latexMathBodyFg);
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

    void testStandaloneSetextH1UnderlineStillHighlighted()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("===");
        highlighter.rehighlight();

        const QTextCharFormat fmt = firstCharFormat(doc, 0);
        QVERIFY(fmt.isValid());
        QCOMPARE(fmt.foreground().color(), theme.markerFg);
    }

    void testSetextH2VariantHeadingLineUpdatesWhenUnderlineTypedLater_data()
    {
        QTest::addColumn<QString>("underline");

        QTest::newRow("dash-compact") << QString("---");
        QTest::newRow("dash-long") << QString("------");
    }

    void testSetextH2VariantHeadingLineUpdatesWhenUnderlineTypedLater()
    {
        QFETCH(QString, underline);

        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("Title\n");
        highlighter.rehighlight();

        QTextCursor cursor(&doc);
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(underline);
        QCoreApplication::processEvents();

        const QTextCharFormat fmt = firstCharFormat(doc, 0);
        QVERIFY(fmt.isValid());
        QCOMPARE(fmt.foreground().color(), theme.heading[1]);
    }

    void testSetextH2VariantUnderlineLineUsesMarkerFormat_data()
    {
        QTest::addColumn<QString>("underline");

        QTest::newRow("dash-compact") << QString("---");
        QTest::newRow("dash-long") << QString("------");
    }

    void testSetextH2VariantUnderlineLineUsesMarkerFormat()
    {
        QFETCH(QString, underline);

        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("Title\n" + underline);
        highlighter.rehighlight();

        const QTextCharFormat underlineFmt = firstCharFormat(doc, 1);
        QVERIFY(underlineFmt.isValid());
        QCOMPARE(underlineFmt.foreground().color(), theme.markerFg);
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

    void testHrVariantsAfterTextDoNotPromoteSetextHeading_data()
    {
        QTest::addColumn<QString>("underline");

        QTest::newRow("star-compact") << QString("***");
        QTest::newRow("dash-spaced") << QString("- - -");
        QTest::newRow("star-spaced") << QString("* * *");
    }

    void testHrVariantsAfterTextDoNotPromoteSetextHeading()
    {
        QFETCH(QString, underline);

        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("Title\n" + underline);
        highlighter.rehighlight();

        const QTextCharFormat headingFmt = firstCharFormat(doc, 0);
        if (headingFmt.isValid()) {
            QVERIFY(headingFmt.foreground().color() != theme.heading[1]);
        }

        const QTextCharFormat underlineFmt = firstCharFormat(doc, 1);
        QVERIFY(underlineFmt.isValid());
        QCOMPARE(underlineFmt.foreground().color(), theme.hrFg);
    }

    void testSetextInBlockquoteUsesSameLevelContainer()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("> Title\n> ---");
        highlighter.rehighlight();

        const QTextBlock headingBlock = doc.findBlockByNumber(0);
        const QTextCharFormat headingFmt = formatAt(headingBlock, 2);
        QVERIFY(headingFmt.isValid());
        QCOMPARE(headingFmt.foreground().color(), theme.heading[1]);

        const QTextBlock underlineBlock = doc.findBlockByNumber(1);
        const QTextCharFormat underlineFmt = formatAt(underlineBlock, 2);
        QVERIFY(underlineFmt.isValid());
        QCOMPARE(underlineFmt.foreground().color(), theme.markerFg);
    }

    void testSetextInNestedBlockquoteUsesSameDepth()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("> > Title\n> > ---");
        highlighter.rehighlight();

        const QTextBlock headingBlock = doc.findBlockByNumber(0);
        const QTextCharFormat headingFmt = formatAt(headingBlock, 4);
        QVERIFY(headingFmt.isValid());
        QCOMPARE(headingFmt.foreground().color(), theme.heading[1]);

        const QTextBlock underlineBlock = doc.findBlockByNumber(1);
        const QTextCharFormat underlineFmt = formatAt(underlineBlock, 4);
        QVERIFY(underlineFmt.isValid());
        QCOMPARE(underlineFmt.foreground().color(), theme.markerFg);
    }

    void testSetextInBlockquoteDoesNotCrossDepth()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("> Title\n> > ---");
        highlighter.rehighlight();

        const QTextBlock headingBlock = doc.findBlockByNumber(0);
        const QTextCharFormat headingFmt = formatAt(headingBlock, 2);
        QVERIFY(headingFmt.isValid());
        QCOMPARE(headingFmt.foreground().color(), theme.blockquoteFg);

        const QTextBlock underlineBlock = doc.findBlockByNumber(1);
        const QTextCharFormat underlineFmt = formatAt(underlineBlock, 4);
        QVERIFY(underlineFmt.isValid());
        QCOMPARE(underlineFmt.foreground().color(), theme.hrFg);
    }

    void testSetextH1HeadingLineUpdatesWhenUnderlineTypedLater()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("Title\n");
        highlighter.rehighlight();

        QTextCursor cursor(&doc);
        cursor.movePosition(QTextCursor::End);
        cursor.insertText("===");
        QCoreApplication::processEvents();

        const QTextCharFormat fmt = firstCharFormat(doc, 0);
        QVERIFY(fmt.isValid());
        QCOMPARE(fmt.foreground().color(), theme.heading[0]);
    }

    void testSetextH1HeadingLineUpdatesWhenTextInsertedAboveUnderline()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("===\n");
        highlighter.rehighlight();

        QTextCursor cursor(&doc);
        cursor.movePosition(QTextCursor::Start);
        cursor.insertText("Title\n");
        QCoreApplication::processEvents();

        const QTextCharFormat headingFmt = firstCharFormat(doc, 0);
        QVERIFY(headingFmt.isValid());
        QCOMPARE(headingFmt.foreground().color(), theme.heading[0]);

        const QTextCharFormat underlineFmt = firstCharFormat(doc, 1);
        QVERIFY(underlineFmt.isValid());
        QCOMPARE(underlineFmt.foreground().color(), theme.markerFg);
    }

    void testTableHeaderLineUpdatesWhenSeparatorTypedLater()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("| col1 | col2 |\n");
        highlighter.rehighlight();

        QTextCursor cursor(&doc);
        cursor.movePosition(QTextCursor::End);
        cursor.insertText("| --- | --- |");
        QCoreApplication::processEvents();

        const QTextBlock headerBlock = doc.findBlockByNumber(0);
        const QTextCharFormat headerFmt = formatAt(headerBlock, 2);
        QVERIFY(headerFmt.isValid());
        QCOMPARE(headerFmt.background().color().alpha(), 56);

        const QTextBlock separatorBlock = doc.findBlockByNumber(1);
        const QTextCharFormat separatorFmt = formatAt(separatorBlock, 2);
        QVERIFY(separatorFmt.isValid());
        QCOMPARE(separatorFmt.foreground().color(), theme.tablePipeFg);
    }

    void testExistingBodyRowsUpdateWhenSeparatorInsertedLater()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("| col1 | col2 |\n| v1 | v2 |\n| v3 | v4 |");
        highlighter.rehighlight();

        QTextCursor cursor(&doc);
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down);
        cursor.insertText("| --- | --- |\n");
        QCoreApplication::processEvents();

        const QTextBlock firstBodyBlock = doc.findBlockByNumber(2);
        const QTextCharFormat firstBodyFmt = formatAt(firstBodyBlock, 2);
        QVERIFY(firstBodyFmt.isValid());
        QCOMPARE(firstBodyFmt.background().color().alpha(), 28);

        const QTextBlock secondBodyBlock = doc.findBlockByNumber(3);
        const QTextCharFormat secondBodyFmt = formatAt(secondBodyBlock, 2);
        QVERIFY(secondBodyFmt.isValid());
        QCOMPARE(secondBodyFmt.background().color().alpha(), 28);
    }

    void testHrVariantKeepsHrFormat_data()
    {
        QTest::addColumn<QString>("line");

        QTest::newRow("compact-dash") << QString("---");
        QTest::newRow("compact-star") << QString("***");
        QTest::newRow("spaced-dash") << QString("- - -");
        QTest::newRow("spaced-star") << QString("* * *");
    }

    void testHrVariantKeepsHrFormat()
    {
        QFETCH(QString, line);

        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText(line);
        highlighter.rehighlight();

        const QTextCharFormat fmt = firstCharFormat(doc, 0);
        QVERIFY(fmt.isValid());
        QCOMPARE(fmt.foreground().color(), theme.hrFg);
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
        Theme theme = Theme::darkDefault();
        theme.background = QColor(QStringLiteral("#ffffff"));
        theme.foreground = QColor(QStringLiteral("#18181b"));
        theme.lineNumberBg = QColor(QStringLiteral("#ffffff"));
        theme.currentLineBg = QColor(QStringLiteral("#f4f4f5"));
        theme.blockquoteBorderFg = QColor(QStringLiteral("#a1a1aa"));

        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("> quote");
        highlighter.rehighlight();

        const QTextBlock block = doc.findBlockByNumber(0);
        const QTextCharFormat markFmt = formatAt(block, 0);
        const QTextCharFormat bodyFmt = formatAt(block, 2);

        QVERIFY(markFmt.isValid());
        QVERIFY(bodyFmt.isValid());

        const QColor markBg = markFmt.background().color();
        const QColor bodyBg = bodyFmt.background().color();
        QCOMPARE(markBg, bodyBg);
        QVERIFY(markBg != theme.background);
        QVERIFY(markBg != theme.currentLineBg);
        QVERIFY(colorDistance(markBg, theme.background) >= 42);
        QVERIFY(colorDistance(markBg, theme.currentLineBg) >= 36);
    }

    void testBlockquoteListAndBoldKeepQuoteBackground()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("> - **bold** tail");
        highlighter.rehighlight();

        const QTextBlock block = doc.findBlockByNumber(0);
        const QTextCharFormat quoteFmt = formatAt(block, 0);
        QVERIFY(quoteFmt.isValid());
        QVERIFY(quoteFmt.background().style() != Qt::NoBrush);

        const QColor quoteBg = quoteFmt.background().color();
        QCOMPARE(formatAt(block, 2).background().color(), quoteBg);   // list marker '-'
        QCOMPARE(formatAt(block, 6).background().color(), quoteBg);   // bold content 'b'
        QCOMPARE(formatAt(block, 13).background().color(), quoteBg);  // plain tail text 't'
    }

    void testBlockquoteOwnedBackgroundDimsLightness()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("> ==mark==");
        highlighter.rehighlight();

        const QTextBlock block = doc.findBlockByNumber(0);
        const QColor markBg = formatAt(block, 4).background().color();

        QVERIFY(markBg.isValid());
        QVERIFY(markBg != theme.searchHighlightBg);
        QVERIFY(markBg.lightness() < theme.searchHighlightBg.lightness());
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

    void testTaskCheckboxUppercaseUsesBulletColor()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("- [X] done");
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

    void testComplexMarkdownFormatRangesStayInBounds()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText(
            "# Title\n"
            "> - [x] ~~done~~ and ==mark==\n"
            "\n"
            "Setext heading\n"
            "---\n"
            "```c++\n"
            "int x = 42;\n"
            "```\n"
            "$$\n"
            "E = mc^2\n"
            "$$\n"
            "\\begin{align*}\n"
            "x &= y\\\\\n"
            "\\end{align*}\n"
            "[![img](pic.png)](https://example.com) and <https://example.com>\n"
            "* * *\n"
        );
        highlighter.rehighlight();

        for (QTextBlock block = doc.begin(); block.isValid(); block = block.next()) {
            const int blockLen = block.text().length();
            if (!block.layout()) {
                continue;
            }

            const auto ranges = block.layout()->formats();
            for (const auto &range : ranges) {
                QVERIFY(range.start >= 0);
                QVERIFY(range.length >= 0);
                QVERIFY(range.start + range.length <= blockLen);
            }
        }
    }

    void testImeComposingAfterBoldMarkerUsesPlainBaseFormat()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        const QString text = QStringLiteral("**aasdfa**這種");
        doc.setPlainText(text);
        highlighter.rehighlight();

        const QTextBlock block = doc.findBlockByNumber(0);
        const int anchor = text.indexOf(QStringLiteral("這"));
        QVERIFY(anchor > 0);

        const QTextCharFormat markerFmt = formatAt(block, anchor - 1);
        const QTextCharFormat rightBeforeFmt = formatAt(block, anchor);
        QVERIFY(markerFmt.isValid());
        QCOMPARE(markerFmt.foreground().color(), theme.markerFg);

        highlighter.setComposingPosition(0, anchor);

        const QTextCharFormat composingLeftFmt = formatAt(block, anchor - 1);
        const QTextCharFormat composingRightFmt = formatAt(block, anchor);
        QVERIFY(composingLeftFmt.isValid());
        QVERIFY(composingRightFmt.isValid());
        QCOMPARE(composingLeftFmt.foreground().color(), theme.foreground);
        QCOMPARE(composingRightFmt.foreground().color(), rightBeforeFmt.foreground().color());
        QVERIFY(composingLeftFmt.fontWeight() <= static_cast<int>(QFont::Normal));
        QVERIFY(!composingLeftFmt.fontItalic());

        highlighter.clearComposingPosition();

        const QTextCharFormat restoredFmt = formatAt(block, anchor - 1);
        QVERIFY(restoredFmt.isValid());
        QCOMPARE(restoredFmt.foreground().color(), theme.markerFg);
    }

    void testImeComposingNearBoldBoundaryKeepsFirstBoldCharFormat()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        const QString text = QStringLiteral("**23123**");
        doc.setPlainText(text);
        highlighter.rehighlight();

        const QTextBlock block = doc.findBlockByNumber(0);
        const int firstContentPos = text.indexOf(QLatin1Char('2'));
        QVERIFY(firstContentPos > 1);

        const QTextCharFormat beforeFmt = formatAt(block, firstContentPos);
        QVERIFY(beforeFmt.isValid());
        QCOMPARE(beforeFmt.foreground().color(), theme.boldFg);

        // Simulate a composing anchor near marker/content boundary.
        highlighter.setComposingPosition(0, firstContentPos);

        const QTextCharFormat markerFmt = formatAt(block, firstContentPos - 1);
        const QTextCharFormat contentFmt = formatAt(block, firstContentPos);
        QVERIFY(markerFmt.isValid());
        QVERIFY(contentFmt.isValid());
        QCOMPARE(markerFmt.foreground().color(), theme.foreground);
        QCOMPARE(contentFmt.foreground().color(), theme.boldFg);

        highlighter.clearComposingPosition();

        const QTextCharFormat restoredMarkerFmt = formatAt(block, firstContentPos - 1);
        const QTextCharFormat restoredContentFmt = formatAt(block, firstContentPos);
        QVERIFY(restoredMarkerFmt.isValid());
        QVERIFY(restoredContentFmt.isValid());
        QCOMPARE(restoredMarkerFmt.foreground().color(), theme.markerFg);
        QCOMPARE(restoredContentFmt.foreground().color(), theme.boldFg);
    }

    void testPreeditRangeDoesNotMutateCommittedFormats()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        const QString text = QStringLiteral("**x23123**");
        doc.setPlainText(text);
        highlighter.rehighlight();

        const QTextBlock block = doc.findBlockByNumber(0);
        const int preeditPos = text.indexOf(QLatin1Char('x'));
        QVERIFY(preeditPos > 1);

        const QTextCharFormat beforePreeditFmt = formatAt(block, preeditPos);
        QVERIFY(beforePreeditFmt.isValid());
        QCOMPARE(beforePreeditFmt.foreground().color(), theme.boldFg);

        highlighter.setPreeditRange(0, preeditPos, 1);

        const QTextCharFormat markerFmt = formatAt(block, preeditPos - 1);
        const QTextCharFormat preeditFmt = formatAt(block, preeditPos);
        const QTextCharFormat nextFmt = formatAt(block, preeditPos + 1);
        QVERIFY(markerFmt.isValid());
        QVERIFY(preeditFmt.isValid());
        QVERIFY(nextFmt.isValid());
        QCOMPARE(markerFmt.foreground().color(), theme.foreground);
        QCOMPARE(preeditFmt.foreground().color(), theme.boldFg);
        QCOMPARE(nextFmt.foreground().color(), theme.boldFg);

        highlighter.clearPreeditRange();

        const QTextCharFormat restoredMarkerFmt = formatAt(block, preeditPos - 1);
        const QTextCharFormat restoredPreeditFmt = formatAt(block, preeditPos);
        QVERIFY(restoredMarkerFmt.isValid());
        QVERIFY(restoredPreeditFmt.isValid());
        QCOMPARE(restoredMarkerFmt.foreground().color(), theme.markerFg);
        QCOMPARE(restoredPreeditFmt.foreground().color(), theme.boldFg);
    }

    void testInlineTokenFormattingCoversWholeRange()
    {
        const Theme theme = Theme::darkDefault();
        QTextDocument doc;
        MdHighlighter highlighter(&doc, theme);

        doc.setPlainText("hello**world**");
        highlighter.rehighlight();

        QTextBlock block = doc.findBlockByNumber(0);
        const int wPos = doc.toPlainText().indexOf("world");
        QVERIFY(wPos >= 0);

        for (int i = 0; i < 5; ++i) {
            QCOMPARE(formatAt(block, wPos + i).foreground().color(), theme.boldFg);
        }
    }
};

QTEST_MAIN(TestHighlighter)
#include "test_highlighter.moc"
