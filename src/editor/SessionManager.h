// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#pragma once

#include <QStringList>
#include <QList>
#include <QString>

class SessionManager {
public:
    struct Session {
        QStringList openFiles;
        int         activeIndex = 0;
        QList<int>  cursorLines;
    };

    static void    save(const Session &session);
    static Session load();
    static QString sessionPath();
};
