// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#pragma once

#include <QWidget>
#include <QSize>

class MdEditor;

class LineNumberArea : public QWidget {
public:
    explicit LineNumberArea(MdEditor *editor);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    MdEditor *editor_;
};
