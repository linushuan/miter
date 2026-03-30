// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#pragma once

#include <QString>
#include <QHash>

class TomlParser {
public:
    explicit TomlParser(const QString &path);
    bool isValid() const;

    QString getString(const QString &section, const QString &key,
                      const QString &defaultValue = {}) const;
    int     getInt(const QString &section, const QString &key,
                   int defaultValue = 0) const;
    bool    getBool(const QString &section, const QString &key,
                    bool defaultValue = false) const;

private:
    bool valid_ = false;
    // Stored as "section.key" -> value
    QHash<QString, QString> values_;

    QString makeKey(const QString &section, const QString &key) const;
};
