#pragma once

#include <QString>

struct Settings {
    // Font
    QString fontFamily    = "JetBrains Mono";
    QString cjkFamily     = "";
    int     fontSize      = 14;

    // Theme
    QString theme         = "dark";

    // Editor
    bool    lineNumbers   = true;
    bool    wordWrap      = true;
    bool    autoSave      = false;
    int     autoSaveInterval = 30;
    bool    showHardBreak = true;
    int     tabSize       = 2;

    // Session
    bool    restoreSession = true;

    // Window
    int     windowWidth   = 900;
    int     windowHeight  = 650;
    bool    rememberWindow = true;

    static Settings load();
    void            save() const;
    static QString  configPath();
};
