// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include "TomlParser.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

TomlParser::TomlParser(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    valid_ = true;
    QString currentSection;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        // Skip empty lines and comments
        if (line.isEmpty() || line.startsWith('#'))
            continue;

        // Section header: [section]
        static QRegularExpression sectionRe(R"(^\[(\w+)\]$)");
        auto sectionMatch = sectionRe.match(line);
        if (sectionMatch.hasMatch()) {
            currentSection = sectionMatch.captured(1);
            continue;
        }

        // Key = value
        static QRegularExpression kvRe(R"(^(\w+)\s*=\s*(.+)$)");
        auto kvMatch = kvRe.match(line);
        if (kvMatch.hasMatch()) {
            QString key = kvMatch.captured(1);
            QString value = kvMatch.captured(2).trimmed();

            // Strip quotes from string values
            if (value.startsWith('"') && value.endsWith('"')) {
                value = value.mid(1, value.length() - 2);
            }

            values_[makeKey(currentSection, key)] = value;
        }
    }
}

bool TomlParser::isValid() const
{
    return valid_;
}

QString TomlParser::getString(const QString &section, const QString &key,
                               const QString &defaultValue) const
{
    auto it = values_.find(makeKey(section, key));
    if (it != values_.end())
        return it.value();
    return defaultValue;
}

int TomlParser::getInt(const QString &section, const QString &key,
                        int defaultValue) const
{
    QString val = getString(section, key);
    if (val.isEmpty()) return defaultValue;

    bool ok;
    int result = val.toInt(&ok);
    return ok ? result : defaultValue;
}

bool TomlParser::getBool(const QString &section, const QString &key,
                          bool defaultValue) const
{
    QString val = getString(section, key).toLower();
    if (val.isEmpty()) return defaultValue;
    if (val == "true" || val == "1") return true;
    if (val == "false" || val == "0") return false;
    return defaultValue;
}

QString TomlParser::makeKey(const QString &section, const QString &key) const
{
    if (section.isEmpty())
        return key;
    return section + "." + key;
}
