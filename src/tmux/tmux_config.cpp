#include "key_binding_parser.hpp"
#include "key_combo.hpp"
#include "string_encoding.hpp"
#include "string_utils.hpp"
#include "tmux_config.hpp"
#include "wmux_engine.hpp"

std::string GetHomeDirectory() {
    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile && *userProfile) return userProfile;

    const char* homeDrive = std::getenv("HOMEDRIVE");
    const char* homePath = std::getenv("HOMEPATH");
    if (homeDrive && homePath && *homeDrive && *homePath) return std::string(homeDrive) + homePath;

    const char* home = std::getenv("HOME");
    if (home && *home) return home;

    return ".";
}

std::string GetTmuxRcPath() {
    std::string home = GetHomeDirectory();
    if (!home.empty() && home.back() != '\\' && home.back() != '/') home += "\\";
    return home + ".tmuxrc";
}

void EnsureTmuxRcExists() {
    std::string path = GetTmuxRcPath();
    std::ifstream existing(path);
    if (existing.good()) return;

    std::ofstream rc(path);
    if (!rc.is_open()) return;
    rc << "# cmd-extended tmux startup configuration\n"
       << "# This file is created automatically so you can customize local keys.\n"
    << "# Available themes: blue, black, orange, silver, pink, yellow\n"
    << "# PowerShell is the default shell; uncomment shell to override.\n"
    << "# shell cmd.exe\n"
       << "theme blue\n"
       << "# Default avoids Windows-reserved shortcuts such as Win+key and Alt+Tab.\n"
    << "prefix-key Ctrl+B\n"
       << "theme-key Ctrl+Alt+T\n"
    << "bind split-vertical Alt+V\n"
    << "bind split-horizontal Alt+H\n"
    << "bind new-window Alt+T\n"
    << "bind next-window Alt+N\n"
    << "bind previous-window Alt+P\n"
    << "bind zoom Alt+Z\n"
    << "bind kill-pane Alt+W\n"
    << "bind kill-pane-alt Alt+Q\n"
    << "bind exit Ctrl+Alt+Q\n"
    << "# Also accepted: bind theme Ctrl+Alt+F8\n";
}

void LoadTmuxRc(WmuxEngine& mux) {
    std::ifstream rc(GetTmuxRcPath());
    if (!rc.is_open()) return;

    std::string line;
    while (std::getline(rc, line)) {
        size_t comment = line.find('#');
        if (comment != std::string::npos) line.erase(comment);
        line = TrimCopy(line);
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string command;
        ss >> command;
        std::string value;
        std::getline(ss, value);
        value = TrimCopy(value);
        command = LowerCopy(command);

        if (command == "shell" || command == "default-shell") {
            if (!value.empty()) mux.defaultShell = StringToWString(value);
        } else if (command == "theme") {
            mux.SetTheme(value);
        } else if (command == "prefix-key") {
            KeyCombo parsed;
            if (ParseKeyCombo(value, parsed)) mux.keys.prefixKey = parsed;
        } else if (command == "theme-key" || command == "bind-theme-key") {
            KeyCombo parsed;
            if (ParseKeyCombo(value, parsed)) mux.keys.themeCycle = parsed;
        } else if (command == "bind") {
            std::stringstream bindArgs(value);
            std::string action;
            bindArgs >> action;
            std::string keySpec;
            std::getline(bindArgs, keySpec);
            KeyCombo* target = FindHelperKey(mux.keys, action);
            KeyCombo parsed;
            if (target && ParseKeyCombo(keySpec, parsed)) *target = parsed;
        }
    }
}
