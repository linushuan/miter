// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#pragma once

#include <QMainWindow>
#include <QStringList>
#include <QStatusBar>

class TabManager;
class EditorWidget;
class MdEditor;
class QAction;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QStringList &filesToOpen = {}, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onCurrentEditorChanged(EditorWidget *editor);
    void onNewTab();
    void onOpenFile();
    void onSave();
    void onSaveAs();
    void onCloseTab();
    void onNextTab();
    void onPrevTab();
    void onJumpToTab(int index);
    void onZoomInAll();
    void onZoomOutAll();
    void onZoomResetAll();
    void onToggleTheme();

private:
    TabManager *tabManager_;
    QStatusBar *statusBar_;
    int currentLine_ = 1;
    int currentCol_ = 1;
    int currentWords_ = 0;
    int currentChars_ = 0;
    QString currentPath_;
    MdEditor *statusEditor_ = nullptr;
    QAction *themeToggleAction_ = nullptr;
    QLabel  *themeIndicatorLabel_ = nullptr;

    void setupShortcuts();
    void setupToolbar();
    void refreshStatusBar();
    void applyTheme(const QString &themeName);
    QString toggledThemeName() const;
};
