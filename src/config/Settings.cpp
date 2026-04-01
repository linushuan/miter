// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#include "Settings.h"
#include "util/TomlParser.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

QString Settings::configPath()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
                        + "/miter";
    QDir().mkpath(configDir);
    return configDir + "/config.toml";
}

int Settings::normalizedTabSize(int tabSize)
{
    if (tabSize < 1) {
        return 1;
    }
    if (tabSize > 16) {
        return 16;
    }
    return tabSize;
}

Settings Settings::load()
{
    Settings s;
    TomlParser toml(configPath());
    if (!toml.isValid()) return s;

    s.fontFamily    = toml.getString("font", "main_family", s.fontFamily);
    s.cjkFamily     = toml.getString("font", "cjk_family", s.cjkFamily);
    s.fontSize      = toml.getInt("font", "size", s.fontSize);

    s.theme         = toml.getString("", "theme", s.theme);

    s.lineNumbers   = toml.getBool("editor", "line_numbers", s.lineNumbers);
    s.wordWrap      = toml.getBool("editor", "word_wrap", s.wordWrap);
    s.autoSave      = toml.getBool("editor", "auto_save", s.autoSave);
    s.autoSaveInterval = toml.getInt("editor", "auto_save_interval", s.autoSaveInterval);
    s.showHardBreak = toml.getBool("editor", "show_hard_break", s.showHardBreak);
    s.tabSize       = normalizedTabSize(toml.getInt("editor", "tab_size", s.tabSize));

    s.restoreSession = toml.getBool("session", "restore", s.restoreSession);

    s.windowWidth   = toml.getInt("window", "width", s.windowWidth);
    s.windowHeight  = toml.getInt("window", "height", s.windowHeight);
    s.rememberWindow = toml.getBool("window", "remember", s.rememberWindow);

    return s;
}

void Settings::save() const
{
    QFile file(configPath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << "theme = \"" << theme << "\"\n\n";

    out << "[font]\n";
    out << "main_family = \"" << fontFamily << "\"\n";
    out << "cjk_family = \"" << cjkFamily << "\"\n";
    out << "size = " << fontSize << "\n\n";

    out << "[editor]\n";
    out << "line_numbers = " << (lineNumbers ? "true" : "false") << "\n";
    out << "word_wrap = " << (wordWrap ? "true" : "false") << "\n";
    out << "auto_save = " << (autoSave ? "true" : "false") << "\n";
    out << "auto_save_interval = " << autoSaveInterval << "\n";
    out << "show_hard_break = " << (showHardBreak ? "true" : "false") << "\n";
    const int safeTabSize = normalizedTabSize(tabSize);
    out << "tab_size = " << safeTabSize << "\n\n";

    out << "[session]\n";
    out << "restore = " << (restoreSession ? "true" : "false") << "\n\n";

    out << "[window]\n";
    out << "width = " << windowWidth << "\n";
    out << "height = " << windowHeight << "\n";
    out << "remember = " << (rememberWindow ? "true" : "false") << "\n";
}
