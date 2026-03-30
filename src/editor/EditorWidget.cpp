#include "EditorWidget.h"
#include "MdEditor.h"
#include "SearchBar.h"

#include <QVBoxLayout>
#include <QTextCursor>
#include <QTextBlock>
#include <QFont>

EditorWidget::EditorWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    editor_ = new MdEditor(this);
    layout->addWidget(editor_);

    searchBar_ = new SearchBar(editor_, this);
    searchBar_->hide();
    layout->addWidget(searchBar_);

    // Forward modified signal
    connect(editor_, &MdEditor::modifiedChanged,
            this, &EditorWidget::modifiedChanged);
    connect(editor_, &MdEditor::fileSaved,
            this, &EditorWidget::fileSaved);
}

MdEditor *EditorWidget::editor() const
{
    return editor_;
}

void EditorWidget::loadFile(const QString &path)
{
    editor_->loadFile(path);
}

bool EditorWidget::save()
{
    return editor_->saveFile();
}

bool EditorWidget::saveAs(const QString &path)
{
    return editor_->saveFile(path);
}

QString EditorWidget::filePath() const
{
    return editor_->currentFilePath();
}

bool EditorWidget::isModified() const
{
    return editor_->isModified();
}

int EditorWidget::cursorLine() const
{
    return editor_->textCursor().blockNumber() + 1;
}

int EditorWidget::cursorColumn() const
{
    return editor_->textCursor().columnNumber() + 1;
}

void EditorWidget::setCursorLine(int line)
{
    if (line <= 0)
        return;

    QTextBlock block = editor_->document()->findBlockByNumber(line - 1);
    if (!block.isValid())
        return;

    QTextCursor cursor(editor_->document());
    cursor.setPosition(block.position());
    editor_->setTextCursor(cursor);
}

void EditorWidget::zoomIn()
{
    editor_->setGlobalFontPointSize(editor_->globalFontPointSize() + 1);
}

void EditorWidget::zoomOut()
{
    editor_->setGlobalFontPointSize(editor_->globalFontPointSize() - 1);
}

void EditorWidget::zoomReset()
{
    editor_->setGlobalFontPointSize(editor_->defaultFontPointSize());
}

void EditorWidget::toggleFocusMode()
{
    focusMode_ = !focusMode_;
    editor_->setFocusModeEnabled(focusMode_);
}

void EditorWidget::toggleLineNumbers()
{
    editor_->setLineNumbersVisible(!editor_->lineNumbersVisible());
}

void EditorWidget::toggleWordWrap()
{
    editor_->setWordWrapEnabled(!editor_->isWordWrapEnabled());
}

void EditorWidget::showSearchBar()
{
    searchBar_->show();
    searchBar_->setFocus();
}
