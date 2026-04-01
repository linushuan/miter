// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include <QTest>
#include <QTextCursor>
#include <QTextBlock>
#include <QFontInfo>

#include "editor/MdEditor.h"
#include "config/Settings.h"

namespace {
int effectiveTabSize()
{
    int tabSize = Settings::load().tabSize;
    if (tabSize < 1) tabSize = 1;
    if (tabSize > 16) tabSize = 16;
    return tabSize;
}

QString spaces(int count)
{
    return QString(count, QLatin1Char(' '));
}
}

class TestMdEditor : public QObject {
    Q_OBJECT

private slots:
    void init()
    {
        editor_ = new MdEditor();
        editor_->resize(640, 360);
        editor_->show();
        editor_->setFocus();
    }

    void cleanup()
    {
        delete editor_;
        editor_ = nullptr;
    }

    void testOrderedListEnterAutoIncrement()
    {
        editor_->setPlainText("1. first");

        QTextCursor cursor = editor_->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Return);

        const QStringList lines = editor_->toPlainText().split('\n');
        QCOMPARE(lines.value(0), QString("1. first"));
        QCOMPARE(lines.value(1), QString("2. "));
    }

    void testOrderedEmptyStarterEnterAutoIncrement()
    {
        editor_->setPlainText("1. ");

        QTextCursor cursor = editor_->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Return);

        const QStringList lines = editor_->toPlainText().split('\n');
        QCOMPARE(lines.value(0), QString("1. "));
        QCOMPARE(lines.value(1), QString("2. "));
    }

    void testShiftEnterSkipsListAutocomplete()
    {
        editor_->setPlainText("- item");

        QTextCursor cursor = editor_->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Return, Qt::ShiftModifier);

        const QStringList lines = editor_->toPlainText().split('\n');
        QCOMPARE(lines.value(0), QString("- item"));
        QCOMPARE(lines.value(1), QString(""));
    }

    void testSecondEnterOnEmptyUnorderedListExitsList()
    {
        editor_->setPlainText("- first");

        QTextCursor cursor = editor_->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Return);
        QTest::keyClick(editor_, Qt::Key_Return);

        QCOMPARE(editor_->toPlainText(), QString("- first\n"));
    }

    void testShiftEnterOnEmptyUnorderedListExitsList()
    {
        editor_->setPlainText("- first");

        QTextCursor cursor = editor_->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Return);
        QTest::keyClick(editor_, Qt::Key_Return, Qt::ShiftModifier);

        QCOMPARE(editor_->toPlainText(), QString("- first\n"));
    }

    void testParenthesisAutopairAtLineEnd()
    {
        editor_->clear();

        QTest::keyClicks(editor_, "(");

        QCOMPARE(editor_->toPlainText(), QString("()"));
        QCOMPARE(editor_->textCursor().positionInBlock(), 1);
    }

    void testParenthesisNoAutopairInMiddleOfText()
    {
        editor_->setPlainText("ab");

        QTextCursor cursor = editor_->textCursor();
        cursor.setPosition(1);
        editor_->setTextCursor(cursor);

        QTest::keyClicks(editor_, "(");

        QCOMPARE(editor_->toPlainText(), QString("a(b"));
        QCOMPARE(editor_->textCursor().positionInBlock(), 2);
    }

    void testTypingClosingParenSkipsExistingCloser()
    {
        editor_->setPlainText("()");

        QTextCursor cursor = editor_->textCursor();
        cursor.setPosition(1);
        editor_->setTextCursor(cursor);

        QTest::keyClicks(editor_, ")");

        QCOMPARE(editor_->toPlainText(), QString("()"));
        QCOMPARE(editor_->textCursor().positionInBlock(), 2);
    }

    void testBracketAutopairAtLineEnd_data()
    {
        QTest::addColumn<QString>("typed");
        QTest::addColumn<QString>("expected");

        QTest::newRow("square") << QString("[") << QString("[]");
        QTest::newRow("curly") << QString("{") << QString("{}");
    }

    void testBracketAutopairAtLineEnd()
    {
        QFETCH(QString, typed);
        QFETCH(QString, expected);

        editor_->clear();
        QTest::keyClicks(editor_, typed);

        QCOMPARE(editor_->toPlainText(), expected);
        QCOMPARE(editor_->textCursor().positionInBlock(), 1);
    }

    void testTypingClosingBracketSkipsExistingCloser_data()
    {
        QTest::addColumn<QString>("line");
        QTest::addColumn<int>("cursorPos");
        QTest::addColumn<QString>("typed");

        QTest::newRow("square") << QString("[]") << 1 << QString("]");
        QTest::newRow("curly") << QString("{}") << 1 << QString("}");
    }

    void testTypingClosingBracketSkipsExistingCloser()
    {
        QFETCH(QString, line);
        QFETCH(int, cursorPos);
        QFETCH(QString, typed);

        editor_->setPlainText(line);

        QTextCursor cursor = editor_->textCursor();
        cursor.setPosition(cursorPos);
        editor_->setTextCursor(cursor);

        QTest::keyClicks(editor_, typed);

        QCOMPARE(editor_->toPlainText(), line);
        QCOMPARE(editor_->textCursor().positionInBlock(), cursorPos + 1);
    }

    void testAngleBracketAutopairAndSkipOverCloser()
    {
        editor_->clear();

        QTest::keyClicks(editor_, "<");
        QCOMPARE(editor_->toPlainText(), QString("<>"));
        QCOMPARE(editor_->textCursor().positionInBlock(), 1);

        QTest::keyClicks(editor_, ">");
        QCOMPARE(editor_->toPlainText(), QString("<>"));
        QCOMPARE(editor_->textCursor().positionInBlock(), 2);
    }

    void testAngleBracketNoAutopairInMiddleOfText()
    {
        editor_->setPlainText("ab");

        QTextCursor cursor = editor_->textCursor();
        cursor.setPosition(1);
        editor_->setTextCursor(cursor);

        QTest::keyClicks(editor_, "<");

        QCOMPARE(editor_->toPlainText(), QString("a<b"));
        QCOMPARE(editor_->textCursor().positionInBlock(), 2);
    }

    void testDollarAutopairAndSkipOverCloser()
    {
        editor_->clear();

        QTest::keyClicks(editor_, "$");
        QCOMPARE(editor_->toPlainText(), QString("$$"));
        QCOMPARE(editor_->textCursor().positionInBlock(), 1);

        QTest::keyClicks(editor_, "$");
        QCOMPARE(editor_->toPlainText(), QString("$$"));
        QCOMPARE(editor_->textCursor().positionInBlock(), 2);
    }

    void testBacktickAutopairAndTripleBacktickInput()
    {
        editor_->clear();

        QTest::keyClicks(editor_, "`");
        QCOMPARE(editor_->toPlainText(), QString("``"));
        QCOMPARE(editor_->textCursor().positionInBlock(), 1);

        QTest::keyClicks(editor_, "`");
        QCOMPARE(editor_->toPlainText(), QString("``"));
        QCOMPARE(editor_->textCursor().positionInBlock(), 2);

        QTest::keyClicks(editor_, "`");
        QCOMPARE(editor_->toPlainText(), QString("```"));
        QCOMPARE(editor_->textCursor().positionInBlock(), 3);
    }

    void testLatexFenceEnterCreatesClosingFence()
    {
        editor_->setPlainText("$$");

        QTextCursor cursor = editor_->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Return);

        QCOMPARE(editor_->toPlainText(), QString("$$\n\n$$"));
        QCOMPARE(editor_->textCursor().blockNumber(), 1);
        QCOMPARE(editor_->textCursor().positionInBlock(), 0);
    }

    void testCodeFenceEnterCreatesClosingFenceWithLanguage()
    {
        editor_->setPlainText("```python");

        QTextCursor cursor = editor_->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Return);

        QCOMPARE(editor_->toPlainText(), QString("```python\n\n```"));
        QCOMPARE(editor_->textCursor().blockNumber(), 1);
        QCOMPARE(editor_->textCursor().positionInBlock(), 0);
    }

    void testLatexBeginEnvEnterCreatesClosingLine()
    {
        editor_->setPlainText("\\begin{equation}");

        QTextCursor cursor = editor_->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Return);

        QCOMPARE(editor_->toPlainText(), QString("\\begin{equation}\n\n\\end{equation}"));
        QCOMPARE(editor_->textCursor().blockNumber(), 1);
        QCOMPARE(editor_->textCursor().positionInBlock(), 0);
    }

    void testLatexBeginEnvWithIndentAndStarInNameCreatesClosingLine()
    {
        editor_->setPlainText("  \\begin{align*}");

        QTextCursor cursor = editor_->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Return);

        QCOMPARE(editor_->toPlainText(), QString("  \\begin{align*}\n  \n  \\end{align*}"));
        QCOMPARE(editor_->textCursor().blockNumber(), 1);
        QCOMPARE(editor_->textCursor().positionInBlock(), 2);
    }

    void testLatexBeginEnvWithTrailingContentDoesNotAutoClose()
    {
        editor_->setPlainText("\\begin{equation} x = 1");

        QTextCursor cursor = editor_->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Return);

        QCOMPARE(editor_->toPlainText(), QString("\\begin{equation} x = 1\n"));
        QCOMPARE(editor_->textCursor().blockNumber(), 1);
        QCOMPARE(editor_->textCursor().positionInBlock(), 0);
    }

    void testLatexBeginEnvInBlockquoteKeepsPrefix()
    {
        editor_->setPlainText("> \\begin{align}");

        QTextCursor cursor = editor_->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Return);

        QCOMPARE(editor_->toPlainText(), QString("> \\begin{align}\n> \n> \\end{align}"));
        QCOMPARE(editor_->textCursor().blockNumber(), 1);
        QCOMPARE(editor_->textCursor().positionInBlock(), 2);
    }

    void testEnterOnClosingCodeFenceDoesNotDuplicateFence()
    {
        editor_->setPlainText("```python\nline\n```");

        QTextBlock closing = editor_->document()->findBlockByNumber(2);
        QTextCursor cursor(editor_->document());
        cursor.setPosition(closing.position() + qMax(0, closing.length() - 1));
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Return);

        QCOMPARE(editor_->toPlainText(), QString("```python\nline\n```\n"));
        QCOMPARE(editor_->textCursor().blockNumber(), 3);
        QCOMPARE(editor_->textCursor().positionInBlock(), 0);
    }

    void testEnterOnClosingLatexFenceDoesNotDuplicateFence()
    {
        editor_->setPlainText("$$\nline\n$$");

        QTextBlock closing = editor_->document()->findBlockByNumber(2);
        QTextCursor cursor(editor_->document());
        cursor.setPosition(closing.position() + qMax(0, closing.length() - 1));
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Return);

        QCOMPARE(editor_->toPlainText(), QString("$$\nline\n$$\n"));
        QCOMPARE(editor_->textCursor().blockNumber(), 3);
        QCOMPARE(editor_->textCursor().positionInBlock(), 0);
    }

    void testBlockquoteEnterAutocompletesPrefix()
    {
        editor_->setPlainText("> quote");

        QTextCursor cursor = editor_->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Return);

        QCOMPARE(editor_->toPlainText(), QString("> quote\n> "));
        QCOMPARE(editor_->textCursor().blockNumber(), 1);
        QCOMPARE(editor_->textCursor().positionInBlock(), 2);
    }

    void testBlockquoteEnterKeepsCurrentIndentAndDepth()
    {
        editor_->setPlainText("   > > deep quote");

        QTextCursor cursor = editor_->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Return);

        QCOMPARE(editor_->toPlainText(), QString("   > > deep quote\n   > > "));
        QCOMPARE(editor_->textCursor().blockNumber(), 1);
        QCOMPARE(editor_->textCursor().positionInBlock(), 7);
    }

    void testBlockquoteEnterAutocompletesCompactPrefix()
    {
        editor_->setPlainText(">>compact");

        QTextCursor cursor = editor_->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Return);

        QCOMPARE(editor_->toPlainText(), QString(">>compact\n>> "));
        QCOMPARE(editor_->textCursor().blockNumber(), 1);
        QCOMPARE(editor_->textCursor().positionInBlock(), 3);
    }

    void testEnterAfterHorizontalRuleDoesNotStartList_data()
    {
        QTest::addColumn<QString>("line");

        QTest::newRow("compact-stars") << QString("***");
        QTest::newRow("spaced-dashes") << QString("- - -");
        QTest::newRow("spaced-stars") << QString("* * *");
    }

    void testEnterAfterHorizontalRuleDoesNotStartList()
    {
        QFETCH(QString, line);

        editor_->setPlainText(line);

        QTextCursor cursor = editor_->textCursor();
        cursor.movePosition(QTextCursor::End);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Return);

        QCOMPARE(editor_->toPlainText(), line + QString("\n"));
        QCOMPARE(editor_->textCursor().blockNumber(), 1);
        QCOMPARE(editor_->textCursor().positionInBlock(), 0);
    }

    void testTabIndentOrderedListAndRecount()
    {
        const int tabSize = effectiveTabSize();
        editor_->setPlainText("1. alpha\n2. \n3. gamma");

        QTextBlock second = editor_->document()->findBlockByNumber(1);
        QTextCursor cursor(editor_->document());
        cursor.setPosition(second.position());
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Tab);

        const QStringList lines = editor_->toPlainText().split('\n');
        QCOMPARE(lines.value(0), QString("1. alpha"));
        QCOMPARE(lines.value(1), spaces(tabSize) + QString("1. "));
        QCOMPARE(lines.value(2), QString("2. gamma"));
    }

    void testBacktabOutdentOrderedListAndRecount()
    {
        const int tabSize = effectiveTabSize();
        editor_->setPlainText(QString("1. alpha\n") + spaces(tabSize) + QString("1. \n2. gamma"));

        QTextBlock second = editor_->document()->findBlockByNumber(1);
        QTextCursor cursor(editor_->document());
        cursor.setPosition(second.position());
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Backtab);

        const QStringList lines = editor_->toPlainText().split('\n');
        QCOMPARE(lines.value(0), QString("1. alpha"));
        QCOMPARE(lines.value(1), QString("2. "));
        QCOMPARE(lines.value(2), QString("3. gamma"));
    }

    void testTabIndentOrderedSubtreeRenumbersSiblings()
    {
        const int tabSize = effectiveTabSize();
        editor_->setPlainText("1. parent\n2. child-a\n3. child-b\n4. tail");

        QTextBlock second = editor_->document()->findBlockByNumber(1);
        QTextBlock third = editor_->document()->findBlockByNumber(2);
        QTextCursor cursor(editor_->document());
        cursor.setPosition(second.position());
        cursor.setPosition(third.position() + qMax(0, third.length() - 1), QTextCursor::KeepAnchor);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Tab);

        const QStringList lines = editor_->toPlainText().split('\n');
        QCOMPARE(lines.value(0), QString("1. parent"));
        QCOMPARE(lines.value(1), spaces(tabSize) + QString("1. child-a"));
        QCOMPARE(lines.value(2), spaces(tabSize) + QString("2. child-b"));
        QCOMPARE(lines.value(3), QString("2. tail"));
    }

    void testTabSelectionMovesListBlock()
    {
        const int tabSize = effectiveTabSize();
        editor_->setPlainText("- parent\n  child\n- peer");

        QTextBlock first = editor_->document()->findBlockByNumber(0);
        QTextBlock second = editor_->document()->findBlockByNumber(1);
        QTextCursor cursor(editor_->document());
        cursor.setPosition(first.position());
        cursor.setPosition(second.position() + qMax(0, second.length() - 1), QTextCursor::KeepAnchor);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Tab);

        const QStringList lines = editor_->toPlainText().split('\n');
        QCOMPARE(lines.value(0), spaces(tabSize) + QString("- parent"));
        QCOMPARE(lines.value(1), spaces(tabSize + 2) + QString("child"));
        QCOMPARE(lines.value(2), QString("- peer"));
    }

    void testTabOnNonEmptyListDoesNotMoveSubtree()
    {
        const int tabSize = effectiveTabSize();
        editor_->setPlainText("- parent\n  child\n- peer");

        QTextBlock first = editor_->document()->findBlockByNumber(0);
        QTextCursor cursor(editor_->document());
        cursor.setPosition(first.position() + first.length() - 1);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Tab);

        const QStringList lines = editor_->toPlainText().split('\n');
        QCOMPARE(lines.value(0), QString("- parent") + spaces(tabSize));
        QCOMPARE(lines.value(1), QString("  child"));
        QCOMPARE(lines.value(2), QString("- peer"));
    }

    void testEditorUsesFixedPitchFont()
    {
        QFontInfo info(editor_->font());
        QVERIFY(info.fixedPitch());
    }

private:
    MdEditor *editor_ = nullptr;
};

QTEST_MAIN(TestMdEditor)
#include "test_md_editor.moc"
