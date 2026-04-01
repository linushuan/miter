// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include "ContextStack.h"

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

