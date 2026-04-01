// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include "ContextStack.h"

#include <QDataStream>
#include <QIODevice>

void ContextStack::push(ContextFrame frame)
{
    stack_.push(frame);
}

void ContextStack::pop()
{
    if (!stack_.isEmpty())
        stack_.pop();
}

ContextFrame ContextStack::top() const
{
    if (stack_.isEmpty())
        return ContextFrame{};
    return stack_.top();
}

BlockState ContextStack::topState() const
{
    return top().state;
}

int ContextStack::listDepth() const
{
    int depth = 0;
    for (const auto &frame : stack_) {
        if (frame.state == BlockState::ListItem)
            depth++;
    }
    return depth;
}

bool ContextStack::inCode() const
{
    return topState() == BlockState::CodeFence;
}

bool ContextStack::inLatex() const
{
    auto s = topState();
    return s == BlockState::LatexDisplay || s == BlockState::LatexEnv;
}

bool ContextStack::inTable() const
{
    return topState() == BlockState::Table;
}

int ContextStack::size() const
{
    return stack_.size();
}

void ContextStack::clear()
{
    stack_.clear();
}

QByteArray ContextStack::serialize() const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);

    stream << static_cast<int>(stack_.size());
    for (const auto &frame : stack_) {
        stream << static_cast<int>(frame.state)
               << frame.depth
               << frame.fenceChar
               << frame.fenceLen
               << frame.envName
               << frame.listIndent;
    }
    return data;
}

ContextStack ContextStack::deserialize(const QByteArray &data)
{
    ContextStack ctx;
    if (data.isEmpty())
        return ctx;

    QDataStream stream(data);
    int count;
    stream >> count;

    for (int i = 0; i < count; ++i) {
        ContextFrame frame;
        int state;
        stream >> state
               >> frame.depth
               >> frame.fenceChar
               >> frame.fenceLen
               >> frame.envName
               >> frame.listIndent;
        frame.state = static_cast<BlockState>(state);
        ctx.push(frame);
    }
    return ctx;
}
