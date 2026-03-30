// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include <QTest>
#include "parser/ContextStack.h"

class TestContextStack : public QObject {
    Q_OBJECT

private slots:
    void testEmptyStack()
    {
        ContextStack ctx;
        QCOMPARE(ctx.size(), 0);
        QCOMPARE(ctx.topState(), BlockState::Normal);
        QVERIFY(!ctx.inCode());
        QVERIFY(!ctx.inLatex());
        QVERIFY(!ctx.inTable());
        QCOMPARE(ctx.listDepth(), 0);
    }

    void testPushPop()
    {
        ContextStack ctx;
        ContextFrame frame;
        frame.state = BlockState::CodeFence;
        frame.fenceChar = '`';
        frame.fenceLen = 3;

        ctx.push(frame);
        QCOMPARE(ctx.size(), 1);
        QCOMPARE(ctx.topState(), BlockState::CodeFence);
        QVERIFY(ctx.inCode());

        ctx.pop();
        QCOMPARE(ctx.size(), 0);
        QCOMPARE(ctx.topState(), BlockState::Normal);
    }

    void testListDepth()
    {
        ContextStack ctx;
        ContextFrame f1;
        f1.state = BlockState::ListItem;
        f1.depth = 0;
        ctx.push(f1);

        ContextFrame f2;
        f2.state = BlockState::ListItem;
        f2.depth = 1;
        ctx.push(f2);

        QCOMPARE(ctx.listDepth(), 2);
    }

    void testSerializeDeserialize()
    {
        ContextStack ctx;
        ContextFrame frame;
        frame.state = BlockState::LatexEnv;
        frame.envName = "equation";
        frame.depth = 1;
        ctx.push(frame);

        QByteArray data = ctx.serialize();
        ContextStack restored = ContextStack::deserialize(data);

        QCOMPARE(restored.size(), 1);
        QCOMPARE(restored.topState(), BlockState::LatexEnv);
        QCOMPARE(restored.top().envName, QString("equation"));
        QCOMPARE(restored.top().depth, 1);
    }

    void testInLatex()
    {
        ContextStack ctx;
        ContextFrame frame;
        frame.state = BlockState::LatexDisplay;
        ctx.push(frame);
        QVERIFY(ctx.inLatex());

        ctx.pop();
        frame.state = BlockState::LatexEnv;
        frame.envName = "align";
        ctx.push(frame);
        QVERIFY(ctx.inLatex());
    }

    void testPopEmptyStack()
    {
        ContextStack ctx;
        ctx.pop(); // Should not crash
        QCOMPARE(ctx.size(), 0);
    }
};

QTEST_MAIN(TestContextStack)
#include "test_context_stack.moc"
