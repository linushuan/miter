#include "TabManager.h"
#include "EditorWidget.h"
#include "MdEditor.h"
#include "config/Settings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFileInfo>

TabManager::TabManager(QWidget *parent)
    : QWidget(parent)
{
    const Settings settings = Settings::load();
    defaultFontSize_ = qMax(6, settings.fontSize);
    globalFontSize_ = defaultFontSize_;
    themeName_ = settings.theme;

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Tab bar row
    auto *tabBarLayout = new QHBoxLayout();
    tabBarLayout->setContentsMargins(0, 0, 0, 0);
    tabBarLayout->setSpacing(0);

    tabBar_ = new QTabBar(this);
    tabBar_->setMovable(true);
    tabBar_->setTabsClosable(true);
    tabBar_->setUsesScrollButtons(true);
    tabBar_->setExpanding(false);
    tabBarLayout->addWidget(tabBar_);

    addTabBtn_ = new QToolButton(this);
    addTabBtn_->setText("+");
    tabBarLayout->addWidget(addTabBtn_);

    mainLayout->addLayout(tabBarLayout);

    // Stacked widget for editor pages
    stack_ = new QStackedWidget(this);
    mainLayout->addWidget(stack_);

    // File system watcher
    watcher_ = new QFileSystemWatcher(this);

    // Connections
    connect(tabBar_, &QTabBar::currentChanged, this, &TabManager::onCurrentChanged);
    connect(tabBar_, &QTabBar::tabCloseRequested, this, &TabManager::onTabCloseRequested);
    connect(addTabBtn_, &QToolButton::clicked, this, [this]() { addEmptyTab(); });
    connect(watcher_, &QFileSystemWatcher::fileChanged, this, &TabManager::onFileWatcherTriggered);

    // Start with one empty tab
    addEmptyTab();
}

int TabManager::addEmptyTab()
{
    auto *editor = new EditorWidget(this);
    editor->editor()->setThemeName(themeName_);
    editor->editor()->setGlobalFontPointSize(globalFontSize_);
    int index = stack_->addWidget(editor);
    tabBar_->addTab("Untitled");

    // Connect modified signal
    connect(editor, &EditorWidget::modifiedChanged,
            this, &TabManager::onEditorModifiedChanged);
    connect(editor, &EditorWidget::fileSaved,
            this, &TabManager::onEditorFileSaved);

    tabBar_->setCurrentIndex(index);
    emit tabCountChanged(count());
    return index;
}

int TabManager::openFile(const QString &path)
{
    externallyModifiedPaths_.remove(path);

    // Check if already open
    int existing = findTabByPath(path);
    if (existing >= 0) {
        setCurrentIndex(existing);
        return existing;
    }

    // If current tab is empty and unmodified, replace it
    auto *current = currentEditor();
    int index;
    if (current && current->filePath().isEmpty() && !current->isModified()) {
        current->loadFile(path);
        index = currentIndex();
    } else {
        auto *editor = new EditorWidget(this);
        editor->editor()->setThemeName(themeName_);
        editor->editor()->setGlobalFontPointSize(globalFontSize_);
        editor->loadFile(path);
        index = stack_->addWidget(editor);
        tabBar_->addTab("");

        connect(editor, &EditorWidget::modifiedChanged,
                this, &TabManager::onEditorModifiedChanged);
        connect(editor, &EditorWidget::fileSaved,
                this, &TabManager::onEditorFileSaved);
    }

    updateTabTitle(index);
    tabBar_->setCurrentIndex(index);

    // Watch the file
    watcher_->addPath(path);

    emit tabCountChanged(count());
    return index;
}

void TabManager::closeTab(int index)
{
    if (index < 0 || index >= count()) return;

    auto *editor = editorAt(index);
    if (editor && editor->isModified()) {
        if (!confirmClose(editor))
            return;
    }

    // Remove file from watcher
    QString path = editor ? editor->filePath() : QString();
    if (!path.isEmpty()) {
        watcher_->removePath(path);
        pendingInternalWrite_.remove(path);
        externallyModifiedPaths_.remove(path);
    }

    // Remove from stack and tab bar
    QWidget *widget = stack_->widget(index);
    stack_->removeWidget(widget);
    tabBar_->removeTab(index);
    widget->deleteLater();

    // Never allow 0 tabs
    if (count() == 0) {
        addEmptyTab();
    }

    emit tabCountChanged(count());
}

void TabManager::closeCurrentTab()
{
    closeTab(currentIndex());
}

EditorWidget *TabManager::currentEditor() const
{
    return qobject_cast<EditorWidget *>(stack_->currentWidget());
}

EditorWidget *TabManager::editorAt(int index) const
{
    return qobject_cast<EditorWidget *>(stack_->widget(index));
}

int TabManager::count() const
{
    return stack_->count();
}

int TabManager::currentIndex() const
{
    return tabBar_->currentIndex();
}

void TabManager::setCurrentIndex(int index)
{
    if (index >= 0 && index < count()) {
        tabBar_->setCurrentIndex(index);
    }
}

bool TabManager::hasUnsavedChanges() const
{
    for (int i = 0; i < count(); ++i) {
        auto *editor = editorAt(i);
        if (editor && editor->isModified())
            return true;
    }
    return false;
}

QStringList TabManager::openFilePaths() const
{
    QStringList paths;
    for (int i = 0; i < count(); ++i) {
        auto *editor = editorAt(i);
        if (editor && !editor->filePath().isEmpty())
            paths.append(editor->filePath());
    }
    return paths;
}

void TabManager::onTabCloseRequested(int index)
{
    closeTab(index);
}

void TabManager::onEditorModifiedChanged(bool /*modified*/)
{
    // Find which editor sent this and update its tab title
    auto *editor = qobject_cast<EditorWidget *>(sender());
    if (!editor) return;

    for (int i = 0; i < count(); ++i) {
        if (editorAt(i) == editor) {
            updateTabTitle(i);
            break;
        }
    }
}

void TabManager::onCurrentChanged(int index)
{
    if (index >= 0 && index < stack_->count()) {
        stack_->setCurrentIndex(index);
        emit currentEditorChanged(currentEditor());
    }
}

void TabManager::onFileWatcherTriggered(const QString &path)
{
    if (pendingInternalWrite_.contains(path)) {
        pendingInternalWrite_.remove(path);
        if (QFileInfo::exists(path)) {
            watcher_->addPath(path);
        }
        return;
    }

    int index = findTabByPath(path);
    if (index < 0) return;

    externallyModifiedPaths_.insert(path);
    updateTabTitle(index);

    // Re-add to watcher (Qt removes it after trigger)
    if (QFileInfo::exists(path)) {
        watcher_->addPath(path);
    }
}

void TabManager::onEditorFileSaved(const QString &path)
{
    if (path.isEmpty())
        return;

    pendingInternalWrite_.insert(path);
    externallyModifiedPaths_.remove(path);

    if (!watcher_->files().contains(path) && QFileInfo::exists(path)) {
        watcher_->addPath(path);
    }

    const int index = findTabByPath(path);
    if (index >= 0) {
        updateTabTitle(index);
    }
}

void TabManager::updateTabTitle(int index)
{
    auto *editor = editorAt(index);
    if (!editor) return;

    QString path = editor->filePath();
    QString title = path.isEmpty()
        ? "Untitled"
        : QFileInfo(path).fileName();

    if (editor->isModified())
        title = "● " + title;

    if (externallyModifiedPaths_.contains(path))
        title += " [已在外部修改]";

    tabBar_->setTabText(index, title);
    tabBar_->setTabToolTip(index, path);
}

bool TabManager::confirmClose(EditorWidget *editor)
{
    QString name = editor->filePath().isEmpty()
        ? "Untitled"
        : QFileInfo(editor->filePath()).fileName();

    auto result = QMessageBox::question(this,
        "Unsaved Changes",
        QString("%1 has been modified. Save changes?").arg(name),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);

    if (result == QMessageBox::Save) {
        return editor->save();
    } else if (result == QMessageBox::Cancel) {
        return false;
    }
    return true; // Discard
}

int TabManager::findTabByPath(const QString &path)
{
    if (path.isEmpty()) return -1;
    for (int i = 0; i < count(); ++i) {
        auto *editor = editorAt(i);
        if (editor && editor->filePath() == path)
            return i;
    }
    return -1;
}

void TabManager::zoomAllEditorsIn()
{
    ++globalFontSize_;
    for (int i = 0; i < count(); ++i) {
        if (auto *editor = editorAt(i)) {
            editor->editor()->setGlobalFontPointSize(globalFontSize_);
        }
    }
}

void TabManager::zoomAllEditorsOut()
{
    if (globalFontSize_ <= 6)
        return;

    --globalFontSize_;
    for (int i = 0; i < count(); ++i) {
        if (auto *editor = editorAt(i)) {
            editor->editor()->setGlobalFontPointSize(globalFontSize_);
        }
    }
}

void TabManager::zoomAllEditorsReset()
{
    globalFontSize_ = defaultFontSize_;
    for (int i = 0; i < count(); ++i) {
        if (auto *editor = editorAt(i)) {
            editor->editor()->setGlobalFontPointSize(globalFontSize_);
        }
    }
}

int TabManager::globalFontSize() const
{
    return globalFontSize_;
}

void TabManager::setThemeName(const QString &themeName)
{
    themeName_ = themeName;
    for (int i = 0; i < count(); ++i) {
        if (auto *editor = editorAt(i)) {
            editor->editor()->setThemeName(themeName_);
        }
    }
}

QString TabManager::themeName() const
{
    return themeName_;
}
