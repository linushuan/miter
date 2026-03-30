// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#pragma once

#include <QWidget>
#include <QString>

class MdEditor;
class SearchBar;

class EditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit EditorWidget(QWidget *parent = nullptr);

    MdEditor *editor() const;

    // Convenience wrappers
    void    loadFile(const QString &path);
    bool    save();
    bool    saveAs(const QString &path);
    QString filePath() const;
    bool    isModified() const;
    int     cursorLine() const;
    int     cursorColumn() const;
    void    setCursorLine(int line);

    void zoomIn();
    void zoomOut();
    void zoomReset();
    void toggleFocusMode();
    void toggleLineNumbers();
    void toggleWordWrap();
    void showSearchBar();

signals:
    void modifiedChanged(bool modified);
    void fileSaved(const QString &path);

private:
    MdEditor  *editor_;
    SearchBar *searchBar_;
    bool       focusMode_ = false;
};
