// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>

class MdEditor;

class SearchBar : public QWidget {
    Q_OBJECT
public:
    explicit SearchBar(MdEditor *editor, QWidget *parent = nullptr);

    void setFocus();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onSearchTextChanged(const QString &text);
    void onFindNext();
    void onFindPrev();
    void onClose();

private:
    MdEditor    *editor_;
    QLineEdit   *searchInput_;
    QPushButton *nextBtn_;
    QPushButton *prevBtn_;
    QPushButton *closeBtn_;
    QCheckBox   *caseSensitive_;
    QCheckBox   *regexMode_;
    QLabel      *matchCountLabel_;

    void highlightAllMatches(const QString &text);
    void clearHighlights();
};
