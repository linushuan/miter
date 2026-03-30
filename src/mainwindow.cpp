// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include "mainwindow.h"
#include "editor/TabManager.h"
#include "editor/EditorWidget.h"
#include "editor/MdEditor.h"
#include "config/Settings.h"

#include <QCloseEvent>
#include <QDialog>
#include <QFileDialog>
#include <QFile>
#include <QShortcut>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QToolBar>
#include <QAction>
#include <QStyle>

namespace {
QIcon themeIcon(bool dark)
{
    const QStringList names = dark
        ? QStringList{"weather-clear-night", "night-light", "weather-many-clouds-night"}
        : QStringList{"weather-clear", "weather-sunny", "weather-few-clouds"};

    for (const QString &name : names) {
        const QIcon icon = QIcon::fromTheme(name);
        if (!icon.isNull()) {
            return icon;
        }
    }
    return QIcon();
}
}

MainWindow::MainWindow(const QStringList &filesToOpen, QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("mded");

    // Load settings
    Settings settings = Settings::load();
    resize(settings.windowWidth, settings.windowHeight);

    // Create TabManager as central widget
    tabManager_ = new TabManager(this);
    setCentralWidget(tabManager_);

    // Status bar
    statusBar_ = new QStatusBar(this);
    setStatusBar(statusBar_);
    themeIndicatorLabel_ = new QLabel(this);
    themeIndicatorLabel_->setMinimumWidth(20);
    statusBar_->addPermanentWidget(themeIndicatorLabel_);
    refreshStatusBar();

    // Connect signals
    connect(tabManager_, &TabManager::currentEditorChanged,
            this, &MainWindow::onCurrentEditorChanged);

    // Setup shortcuts
    setupShortcuts();
    setupToolbar();

    // Open files from CLI only; no automatic session restore.
    if (!filesToOpen.isEmpty()) {
        for (const auto &path : filesToOpen) {
            tabManager_->openFile(path);
        }
    }

    // Ensure at least one tab
    if (tabManager_->count() == 0) {
        tabManager_->addEmptyTab();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Batch-ask about all unsaved tabs
    if (tabManager_->hasUnsavedChanges()) {
        // Collect unsaved tabs
        QStringList unsavedFiles;
        QList<int> unsavedIndices;
        for (int i = 0; i < tabManager_->count(); ++i) {
            auto *editor = tabManager_->editorAt(i);
            if (editor && editor->isModified()) {
                unsavedIndices.append(i);
                QString name = editor->filePath().isEmpty()
                    ? QString("Untitled %1").arg(i + 1)
                    : editor->filePath();
                unsavedFiles.append(name);
            }
        }

        // Show a dialog with checkboxes for each unsaved file
        QDialog dialog(this);
        dialog.setWindowTitle("Unsaved Changes");
        auto *layout = new QVBoxLayout(&dialog);
        layout->addWidget(new QLabel("The following files have unsaved changes.\nSelect which to save:"));

        QList<QCheckBox *> checkboxes;
        for (const auto &name : unsavedFiles) {
            auto *cb = new QCheckBox(name, &dialog);
            cb->setChecked(true);
            layout->addWidget(cb);
            checkboxes.append(cb);
        }

        auto *buttons = new QDialogButtonBox(
            QDialogButtonBox::Save | QDialogButtonBox::Discard | QDialogButtonBox::Cancel,
            &dialog);
        layout->addWidget(buttons);

        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons->button(QDialogButtonBox::Discard), &QPushButton::clicked,
                &dialog, &QDialog::reject);
        connect(buttons->button(QDialogButtonBox::Cancel), &QPushButton::clicked,
                [&dialog]() { dialog.done(2); });

        int result = dialog.exec();
        if (result == 2) {  // Cancel
            event->ignore();
            return;
        }
        if (result == QDialog::Accepted) {
            for (int i = 0; i < checkboxes.size(); ++i) {
                if (checkboxes[i]->isChecked()) {
                    tabManager_->editorAt(unsavedIndices[i])->save();
                }
            }
        }
        // Discard: just close without saving
    }

    event->accept();
}

void MainWindow::onCurrentEditorChanged(EditorWidget *editor)
{
    if (!editor) return;

    auto *md = editor->editor();
    if (!md) return;

    if (statusEditor_) {
        disconnect(statusEditor_, nullptr, this, nullptr);
    }
    statusEditor_ = md;

    connect(md, &MdEditor::cursorPositionChanged, this,
            [this](int line, int col) {
                currentLine_ = line;
                currentCol_ = col;
                refreshStatusBar();
            });

    connect(md, &MdEditor::wordCountChanged, this,
            [this](int words, int chars) {
                currentWords_ = words;
                currentChars_ = chars;
                refreshStatusBar();
            });

    connect(md, &MdEditor::fileChanged, this,
            [this](const QString &path) {
                currentPath_ = path;
                refreshStatusBar();
            });

    currentLine_ = editor->cursorLine();
    currentCol_ = editor->cursorColumn();
    currentPath_ = editor->filePath();
    refreshStatusBar();
}

void MainWindow::onNewTab()
{
    tabManager_->addEmptyTab();
}

void MainWindow::onOpenFile()
{
    QString path = QFileDialog::getOpenFileName(this, "Open Markdown File",
                                                 QString(), "Markdown (*.md *.markdown);;All Files (*)");
    if (!path.isEmpty()) {
        tabManager_->openFile(path);
    }
}

void MainWindow::onSave()
{
    auto *editor = tabManager_->currentEditor();
    if (editor) editor->save();
}

void MainWindow::onSaveAs()
{
    auto *editor = tabManager_->currentEditor();
    if (!editor) return;

    QString path = QFileDialog::getSaveFileName(this, "Save As",
                                                 QString(), "Markdown (*.md *.markdown);;All Files (*)");
    if (!path.isEmpty()) {
        editor->saveAs(path);
    }
}

void MainWindow::onCloseTab()
{
    tabManager_->closeCurrentTab();
}

void MainWindow::onZoomInAll()
{
    tabManager_->zoomAllEditorsIn();
    Settings s = Settings::load();
    s.fontSize = tabManager_->globalFontSize();
    s.save();
    statusBar_->showMessage(QString("Font size: %1").arg(s.fontSize), 1200);
}

void MainWindow::onZoomOutAll()
{
    tabManager_->zoomAllEditorsOut();
    Settings s = Settings::load();
    s.fontSize = tabManager_->globalFontSize();
    s.save();
    statusBar_->showMessage(QString("Font size: %1").arg(s.fontSize), 1200);
}

void MainWindow::onZoomResetAll()
{
    tabManager_->zoomAllEditorsReset();
    Settings s = Settings::load();
    s.fontSize = tabManager_->globalFontSize();
    s.save();
    statusBar_->showMessage(QString("Font size reset: %1").arg(s.fontSize), 1200);
}

void MainWindow::onToggleTheme()
{
    applyTheme(toggledThemeName());
}

void MainWindow::onNextTab()
{
    int next = (tabManager_->currentIndex() + 1) % tabManager_->count();
    tabManager_->setCurrentIndex(next);
}

void MainWindow::onPrevTab()
{
    int prev = (tabManager_->currentIndex() - 1 + tabManager_->count()) % tabManager_->count();
    tabManager_->setCurrentIndex(prev);
}

void MainWindow::onJumpToTab(int index)
{
    if (index < tabManager_->count()) {
        tabManager_->setCurrentIndex(index);
    } else if (tabManager_->count() > 0) {
        tabManager_->setCurrentIndex(tabManager_->count() - 1);
    }
}

void MainWindow::setupShortcuts()
{
    auto addShortcut = [this](const QKeySequence &key, auto slot) {
        auto *sc = new QShortcut(key, this);
        connect(sc, &QShortcut::activated, this, slot);
    };

    addShortcut(QKeySequence("Ctrl+T"),       &MainWindow::onNewTab);
    addShortcut(QKeySequence("Ctrl+O"),       &MainWindow::onOpenFile);
    addShortcut(QKeySequence("Ctrl+S"),       &MainWindow::onSave);
    addShortcut(QKeySequence("Ctrl+Shift+S"), &MainWindow::onSaveAs);
    addShortcut(QKeySequence("Ctrl+W"),       &MainWindow::onCloseTab);
    addShortcut(QKeySequence("Ctrl+Tab"),     &MainWindow::onNextTab);
    addShortcut(QKeySequence("Ctrl+Shift+Tab"), &MainWindow::onPrevTab);
    addShortcut(QKeySequence("Ctrl+Q"),       [this]() { close(); });

    // Ctrl+1..9
    for (int i = 1; i <= 9; ++i) {
        auto *sc = new QShortcut(QKeySequence(QString("Ctrl+%1").arg(i)), this);
        connect(sc, &QShortcut::activated, this, [this, i]() { onJumpToTab(i - 1); });
    }

    // Global font size across all tabs.
    addShortcut(QKeySequence("Ctrl++"),       &MainWindow::onZoomInAll);
    addShortcut(QKeySequence("Ctrl+="),       &MainWindow::onZoomInAll);
    addShortcut(QKeySequence("Ctrl+-"),       &MainWindow::onZoomOutAll);
    addShortcut(QKeySequence("Ctrl+0"),       &MainWindow::onZoomResetAll);

    // Focus mode, fullscreen
    auto *focus = new QShortcut(QKeySequence("F11"), this);
    connect(focus, &QShortcut::activated, this, [this]() {
        auto *e = tabManager_->currentEditor();
        if (e) e->toggleFocusMode();
    });
    auto *fullscreen = new QShortcut(QKeySequence("F12"), this);
    connect(fullscreen, &QShortcut::activated, this, [this]() {
        if (isFullScreen()) showNormal();
        else showFullScreen();
    });

    // Search
    auto *search = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(search, &QShortcut::activated, this, [this]() {
        auto *e = tabManager_->currentEditor();
        if (e) e->showSearchBar();
    });

    // Toggle line numbers
    auto *toggleLineNumbers = new QShortcut(QKeySequence("Ctrl+L"), this);
    connect(toggleLineNumbers, &QShortcut::activated, this, [this]() {
        auto *e = tabManager_->currentEditor();
        if (e) e->toggleLineNumbers();
    });

    // Toggle word wrap
    auto *toggleWordWrap = new QShortcut(QKeySequence("Ctrl+Shift+W"), this);
    connect(toggleWordWrap, &QShortcut::activated, this, [this]() {
        auto *e = tabManager_->currentEditor();
        if (e) e->toggleWordWrap();
    });
}

void MainWindow::setupToolbar()
{
    auto *tb = addToolBar("Main");
    tb->setMovable(false);

    auto *importMd = tb->addAction(style()->standardIcon(QStyle::SP_DialogOpenButton), "Import");
    connect(importMd, &QAction::triggered, this, &MainWindow::onOpenFile);

    auto *save = tb->addAction(style()->standardIcon(QStyle::SP_DialogSaveButton), "Save");
    connect(save, &QAction::triggered, this, &MainWindow::onSave);

    tb->addSeparator();
    themeToggleAction_ = tb->addAction(QIcon(), QString());
    connect(themeToggleAction_, &QAction::triggered, this, &MainWindow::onToggleTheme);
    refreshStatusBar();
}

void MainWindow::refreshStatusBar()
{
    const QString theme = tabManager_ ? tabManager_->themeName() : QString("dark");
    const QString path = currentPath_.isEmpty() ? "Untitled" : currentPath_;
    statusBar_->showMessage(
        QString("Ln %1, Col %2 | Words %3, Chars %4 | %5")
            .arg(currentLine_)
            .arg(currentCol_)
            .arg(currentWords_)
            .arg(currentChars_)
            .arg(path)
    );

    const bool dark = (theme == "dark");
    if (themeIndicatorLabel_) {
        const QIcon currentThemeIcon = themeIcon(dark);
        if (!currentThemeIcon.isNull()) {
            themeIndicatorLabel_->setPixmap(currentThemeIcon.pixmap(16, 16));
            themeIndicatorLabel_->setText(QString());
        } else {
            themeIndicatorLabel_->setPixmap(QPixmap());
            themeIndicatorLabel_->setText(dark ? QString::fromUtf8("🌙") : QString::fromUtf8("☀"));
        }
        themeIndicatorLabel_->setToolTip(dark ? "Dark theme" : "Light theme");
    }

    if (themeToggleAction_) {
        const QString nextTheme = (theme == "white") ? "dark" : "white";
        const QIcon nextThemeIcon = themeIcon(nextTheme == "dark");
        themeToggleAction_->setIcon(nextThemeIcon);
        themeToggleAction_->setText(QString());
        themeToggleAction_->setToolTip(nextTheme == "dark" ? "Switch to dark theme" : "Switch to light theme");
    }
}

void MainWindow::applyTheme(const QString &themeName)
{
    tabManager_->setThemeName(themeName);
    Settings s = Settings::load();
    s.theme = themeName;
    s.save();
    refreshStatusBar();
}

QString MainWindow::toggledThemeName() const
{
    const QString current = tabManager_ ? tabManager_->themeName() : QString("dark");
    return (current == "white") ? "dark" : "white";
}

