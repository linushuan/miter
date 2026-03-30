// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include "SearchBar.h"
#include "MdEditor.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextEdit>
#include <QRegularExpression>

SearchBar::SearchBar(MdEditor *editor, QWidget *parent)
    : QWidget(parent)
    , editor_(editor)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);

    auto *searchIcon = new QLabel("🔍", this);
    layout->addWidget(searchIcon);

    searchInput_ = new QLineEdit(this);
    searchInput_->setPlaceholderText("Search...");
    layout->addWidget(searchInput_);

    prevBtn_ = new QPushButton("◀", this);
    nextBtn_ = new QPushButton("▶", this);
    closeBtn_ = new QPushButton("✕", this);
    layout->addWidget(prevBtn_);
    layout->addWidget(nextBtn_);
    layout->addWidget(closeBtn_);

    caseSensitive_ = new QCheckBox("Aa", this);
    regexMode_ = new QCheckBox(".*", this);
    layout->addWidget(caseSensitive_);
    layout->addWidget(regexMode_);

    matchCountLabel_ = new QLabel("", this);
    layout->addWidget(matchCountLabel_);

    // Connections
    connect(searchInput_, &QLineEdit::textChanged, this, &SearchBar::onSearchTextChanged);
    connect(nextBtn_, &QPushButton::clicked, this, &SearchBar::onFindNext);
    connect(prevBtn_, &QPushButton::clicked, this, &SearchBar::onFindPrev);
    connect(closeBtn_, &QPushButton::clicked, this, &SearchBar::onClose);

    // Enter/Shift+Enter for next/prev
    searchInput_->installEventFilter(this);
}

void SearchBar::setFocus()
{
    searchInput_->setFocus();
    searchInput_->selectAll();
}

bool SearchBar::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == searchInput_ && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            onClose();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return) {
            if (keyEvent->modifiers() & Qt::ShiftModifier)
                onFindPrev();
            else
                onFindNext();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void SearchBar::onSearchTextChanged(const QString &text)
{
    highlightAllMatches(text);
    if (!text.isEmpty()) {
        onFindNext();
    }
}

void SearchBar::onFindNext()
{
    QString searchText = searchInput_->text();
    if (searchText.isEmpty()) return;

    QTextDocument::FindFlags flags;
    if (caseSensitive_->isChecked())
        flags |= QTextDocument::FindCaseSensitively;

    QTextCursor cursor = editor_->textCursor();

    if (regexMode_->isChecked()) {
        QRegularExpression regex(searchText);
        cursor = editor_->document()->find(regex, cursor, flags);
    } else {
        cursor = editor_->document()->find(searchText, cursor, flags);
    }

    // Wrap around
    if (cursor.isNull()) {
        QTextCursor start(editor_->document());
        if (regexMode_->isChecked()) {
            QRegularExpression regex(searchText);
            cursor = editor_->document()->find(regex, start, flags);
        } else {
            cursor = editor_->document()->find(searchText, start, flags);
        }
    }

    if (!cursor.isNull()) {
        editor_->setTextCursor(cursor);
    }
}

void SearchBar::onFindPrev()
{
    QString searchText = searchInput_->text();
    if (searchText.isEmpty()) return;

    QTextDocument::FindFlags flags = QTextDocument::FindBackward;
    if (caseSensitive_->isChecked())
        flags |= QTextDocument::FindCaseSensitively;

    QTextCursor cursor = editor_->textCursor();

    if (regexMode_->isChecked()) {
        QRegularExpression regex(searchText);
        cursor = editor_->document()->find(regex, cursor, flags);
    } else {
        cursor = editor_->document()->find(searchText, cursor, flags);
    }

    // Wrap around to end
    if (cursor.isNull()) {
        QTextCursor end(editor_->document());
        end.movePosition(QTextCursor::End);
        if (regexMode_->isChecked()) {
            QRegularExpression regex(searchText);
            cursor = editor_->document()->find(regex, end, flags);
        } else {
            cursor = editor_->document()->find(searchText, end, flags);
        }
    }

    if (!cursor.isNull()) {
        editor_->setTextCursor(cursor);
    }
}

void SearchBar::onClose()
{
    clearHighlights();
    hide();
    editor_->setFocus();
}

void SearchBar::highlightAllMatches(const QString &text)
{
    clearHighlights();
    if (text.isEmpty()) {
        matchCountLabel_->setText("");
        return;
    }

    QList<QTextEdit::ExtraSelection> extraSelections;
    QTextDocument::FindFlags flags;
    if (caseSensitive_->isChecked())
        flags |= QTextDocument::FindCaseSensitively;

    QTextCursor cursor(editor_->document());
    int count = 0;

    while (true) {
        QTextCursor found;
        if (regexMode_->isChecked()) {
            QRegularExpression regex(text);
            found = editor_->document()->find(regex, cursor, flags);
        } else {
            found = editor_->document()->find(text, cursor, flags);
        }
        if (found.isNull()) break;

        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(QColor("#f9e2af44"));
        sel.cursor = found;
        extraSelections.append(sel);
        cursor = found;
        count++;

        if (count > 10000) break; // Safety limit
    }

    editor_->setExtraSelections(extraSelections);
    matchCountLabel_->setText(QString("%1 matches").arg(count));
}

void SearchBar::clearHighlights()
{
    editor_->setExtraSelections({});
}
