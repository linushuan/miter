// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include <QTest>
#include <QTextCursor>
#include <QTextBlock>
#include <QFontInfo>

#include "editor/MdEditor.h"

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

    void testTabIndentOrderedListAndRecount()
    {
        editor_->setPlainText("1. alpha\n2. \n3. gamma");

        QTextBlock second = editor_->document()->findBlockByNumber(1);
        QTextCursor cursor(editor_->document());
        cursor.setPosition(second.position());
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Tab);

        const QStringList lines = editor_->toPlainText().split('\n');
        QCOMPARE(lines.value(0), QString("1. alpha"));
        QCOMPARE(lines.value(1), QString("  1. "));
        QCOMPARE(lines.value(2), QString("2. gamma"));
    }

    void testBacktabOutdentOrderedListAndRecount()
    {
        editor_->setPlainText("1. alpha\n  1. \n2. gamma");

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
        QCOMPARE(lines.value(1), QString("  1. child-a"));
        QCOMPARE(lines.value(2), QString("  2. child-b"));
        QCOMPARE(lines.value(3), QString("2. tail"));
    }

    void testTabSelectionMovesListBlock()
    {
        editor_->setPlainText("- parent\n  child\n- peer");

        QTextBlock first = editor_->document()->findBlockByNumber(0);
        QTextBlock second = editor_->document()->findBlockByNumber(1);
        QTextCursor cursor(editor_->document());
        cursor.setPosition(first.position());
        cursor.setPosition(second.position() + qMax(0, second.length() - 1), QTextCursor::KeepAnchor);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Tab);

        const QStringList lines = editor_->toPlainText().split('\n');
        QCOMPARE(lines.value(0), QString("  - parent"));
        QCOMPARE(lines.value(1), QString("    child"));
        QCOMPARE(lines.value(2), QString("- peer"));
    }

    void testTabOnNonEmptyListDoesNotMoveSubtree()
    {
        editor_->setPlainText("- parent\n  child\n- peer");

        QTextBlock first = editor_->document()->findBlockByNumber(0);
        QTextCursor cursor(editor_->document());
        cursor.setPosition(first.position() + first.length() - 1);
        editor_->setTextCursor(cursor);

        QTest::keyClick(editor_, Qt::Key_Tab);

        const QStringList lines = editor_->toPlainText().split('\n');
        QCOMPARE(lines.value(0), QString("- parent  "));
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
