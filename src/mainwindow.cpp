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
#include <QPainter>
#include <QPainterPath>
#include <QGuiApplication>

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

QIcon themeToggleLineIcon()
{
    const QStringList names = {
        "color-select-symbolic",
        "applications-graphics-symbolic",
        "preferences-desktop-theme-symbolic",
        "applications-graphics",
        "preferences-desktop-theme"
    };

    for (const QString &name : names) {
        const QIcon icon = QIcon::fromTheme(name);
        if (!icon.isNull()) {
            return icon;
        }
    }

    QPixmap pix(18, 18);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);

    QPen pen(QColor("#6b7280"));
    pen.setWidthF(1.5);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    QPainterPath palette;
    palette.moveTo(9.0, 2.0);
    palette.cubicTo(4.5, 2.0, 2.0, 4.8, 2.0, 8.8);
    palette.cubicTo(2.0, 12.8, 5.1, 16.0, 8.9, 16.0);
    palette.lineTo(10.6, 16.0);
    palette.cubicTo(11.5, 16.0, 12.2, 15.3, 12.2, 14.4);
    palette.cubicTo(12.2, 13.5, 12.9, 12.8, 13.8, 12.8);
    palette.lineTo(14.7, 12.8);
    palette.cubicTo(16.9, 12.8, 18.0, 11.3, 18.0, 9.5);
    palette.cubicTo(18.0, 5.6, 14.8, 2.0, 9.0, 2.0);
    p.drawPath(palette);

    p.drawEllipse(QPointF(6.0, 7.0), 0.7, 0.7);
    p.drawEllipse(QPointF(8.8, 5.5), 0.7, 0.7);
    p.drawEllipse(QPointF(11.6, 6.4), 0.7, 0.7);

    return QIcon(pix);
}

QIcon firstAvailableThemeIcon(const QStringList &names)
{
    for (const QString &name : names) {
        const QIcon icon = QIcon::fromTheme(name);
        if (!icon.isNull()) {
            return icon;
        }
    }
    return QIcon();
}

QIcon saveAsBadgeIcon(const QIcon &baseIcon)
{
    QPixmap pix = baseIcon.pixmap(18, 18);
    if (pix.isNull()) {
        pix = QPixmap(18, 18);
        pix.fill(Qt::transparent);
    }

    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor badgeBg = QGuiApplication::palette().color(QPalette::Highlight);
    if (!badgeBg.isValid()) {
        badgeBg = QColor("#3b82f6");
    }
    badgeBg.setAlpha(230);

    QColor badgeFg = QGuiApplication::palette().color(QPalette::HighlightedText);
    if (!badgeFg.isValid()) {
        badgeFg = QColor("#ffffff");
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(badgeBg);
    painter.drawEllipse(QRectF(10.0, 10.0, 7.0, 7.0));

    QPen plusPen(badgeFg);
    plusPen.setWidthF(1.3);
    plusPen.setCapStyle(Qt::RoundCap);
    painter.setPen(plusPen);
    painter.drawLine(QPointF(13.5, 11.7), QPointF(13.5, 15.3));
    painter.drawLine(QPointF(11.7, 13.5), QPointF(15.3, 13.5));

    return QIcon(pix);
}
}

MainWindow::MainWindow(const QStringList &filesToOpen, QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("miter");

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

    onCurrentEditorChanged(tabManager_->currentEditor());
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
                    auto *editor = tabManager_->editorAt(unsavedIndices[i]);
                    if (!editor || !editor->saveInteractive(this)) {
                        event->ignore();
                        return;
                    }
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
    currentWords_ = md->wordCount();
    currentChars_ = md->charCount();
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
    if (!editor) {
        return;
    }

    const bool hadPathBeforeSave = !editor->filePath().isEmpty();
    if (editor->saveInteractive(this)) {
        const QString savedPath = editor->filePath();
        if (!savedPath.isEmpty()) {
            statusBar_->showMessage(QString("Saved %1").arg(QFileInfo(savedPath).fileName()), 3000);
        }
        return;
    }

    // For untitled documents, false may simply mean Save As was cancelled.
    if (!hadPathBeforeSave) {
        return;
    }

    const QString path = editor->filePath();
    const QString label = path.isEmpty() ? QString("current file") : QFileInfo(path).fileName();
    statusBar_->showMessage(QString("Failed to save %1").arg(label), 5000);
    QMessageBox::warning(this,
                         "Save Failed",
                         QString("Could not save %1.\nPlease check file permissions or disk space.")
                             .arg(label));
}

void MainWindow::onSaveAs()
{
    auto *editor = tabManager_->currentEditor();
    if (!editor) return;
    editor->saveAsInteractive(this);
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

    QIcon saveIcon = firstAvailableThemeIcon({
        "document-save-symbolic",
        "document-save",
        "filesave"
    });
    if (saveIcon.isNull()) {
        saveIcon = style()->standardIcon(QStyle::SP_DialogSaveButton);
    }

    auto *save = tb->addAction(saveIcon, "Save");
    connect(save, &QAction::triggered, this, &MainWindow::onSave);

    QIcon saveAsIcon = firstAvailableThemeIcon({
        "document-save-as-symbolic",
        "document-save-as",
        "filesaveas"
    });
    if (saveAsIcon.isNull()) {
        saveAsIcon = saveAsBadgeIcon(saveIcon);
    }

    auto *saveAs = tb->addAction(saveAsIcon, "Save As");
    connect(saveAs, &QAction::triggered, this, &MainWindow::onSaveAs);

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
        themeToggleAction_->setIcon(themeToggleLineIcon());
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

