// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include "LineNumberArea.h"
#include "MdEditor.h"

LineNumberArea::LineNumberArea(MdEditor *editor)
    : QWidget(editor)
    , editor_(editor)
{
}

QSize LineNumberArea::sizeHint() const
{
    return QSize(editor_->lineNumberAreaWidth(), 0);
}

void LineNumberArea::paintEvent(QPaintEvent *event)
{
    editor_->lineNumberAreaPaintEvent(event);
}
