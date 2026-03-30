// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include <QTest>
#include <QApplication>
#include <QTemporaryFile>
#include "editor/TabManager.h"
#include "editor/EditorWidget.h"

class TestTabManager : public QObject {
    Q_OBJECT

private slots:
    void testInitialState()
    {
        TabManager mgr;
        // TabManager starts with 1 empty tab
        QCOMPARE(mgr.count(), 1);
        QVERIFY(mgr.currentEditor() != nullptr);
    }

    void testAddEmptyTab()
    {
        TabManager mgr;
        int initialCount = mgr.count();
        mgr.addEmptyTab();
        QCOMPARE(mgr.count(), initialCount + 1);
    }

    void testCloseLastTab()
    {
        TabManager mgr;
        QCOMPARE(mgr.count(), 1);

        // Close the only tab — should auto-create a new empty one
        mgr.closeTab(0);
        QCOMPARE(mgr.count(), 1);
    }

    void testOpenFileDuplicate()
    {
        TabManager mgr;

        // Create a temp file to open
        QTemporaryFile tmpFile;
        tmpFile.setAutoRemove(true);
        QVERIFY(tmpFile.open());
        tmpFile.write("# Test");
        tmpFile.flush();
        QString path = tmpFile.fileName();

        int idx1 = mgr.openFile(path);
        int countAfterFirst = mgr.count();

        // Opening same file again should not create new tab
        int idx2 = mgr.openFile(path);
        QCOMPARE(mgr.count(), countAfterFirst);
        QCOMPARE(idx1, idx2);
    }

    void testCurrentIndex()
    {
        TabManager mgr;
        mgr.addEmptyTab();
        mgr.addEmptyTab();

        mgr.setCurrentIndex(0);
        QCOMPARE(mgr.currentIndex(), 0);

        mgr.setCurrentIndex(2);
        QCOMPARE(mgr.currentIndex(), 2);
    }

    void testHasUnsavedChanges()
    {
        TabManager mgr;
        // Fresh empty tab should not be modified
        QVERIFY(!mgr.hasUnsavedChanges());
    }

    void testOpenFilePaths()
    {
        TabManager mgr;
        QStringList paths = mgr.openFilePaths();
        // Empty tabs have no file path
        QVERIFY(paths.isEmpty());
    }
};

QTEST_MAIN(TestTabManager)
#include "test_tab_manager.moc"
