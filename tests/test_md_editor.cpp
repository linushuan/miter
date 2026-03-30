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
