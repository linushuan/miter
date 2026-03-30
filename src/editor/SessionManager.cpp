// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include "SessionManager.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStandardPaths>

QString SessionManager::sessionPath()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                        + "/mded";
    QDir().mkpath(configDir);
    return configDir + "/session.json";
}

void SessionManager::save(const Session &session)
{
    QJsonObject root;
    QJsonArray files;
    for (const auto &f : session.openFiles)
        files.append(f);
    root["openFiles"] = files;
    root["activeIndex"] = session.activeIndex;

    QJsonArray cursors;
    for (int line : session.cursorLines)
        cursors.append(line);
    root["cursorLines"] = cursors;

    QFile file(sessionPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
    }
}

SessionManager::Session SessionManager::load()
{
    Session session;

    QFile file(sessionPath());
    if (!file.open(QIODevice::ReadOnly))
        return session;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject root = doc.object();

    for (const auto &v : root["openFiles"].toArray())
        session.openFiles.append(v.toString());

    session.activeIndex = root["activeIndex"].toInt(0);

    for (const auto &v : root["cursorLines"].toArray())
        session.cursorLines.append(v.toInt(0));

    return session;
}
