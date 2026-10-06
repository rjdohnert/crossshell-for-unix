#include "tmux_help.hpp"

void PrintHelp() {
    std::cout << R"(tmux(1)                 CrossShell for UNIX Reference Manual                 tmux(1)

    NAME
        tmux - terminal multiplexer

    SYNOPSIS
        tmux [OPTIONS] [COMMAND]

    DESCRIPTION
        tmux is a terminal multiplexer that enables multiple terminal sessions
        to be accessed and controlled concurrently from a single window.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    KEYBINDINGS
        GLOBAL ALT HOTKEYS (Fast, No Prefix Key Needed):
        Alt+v               Split active pane vertically (Left / Right)
        Alt+h               Split active pane horizontally (Top / Bottom)
        Alt+<Arrow Keys>    Navigate focus between adjacent panes
        Alt+z               Toggle zoom / full-screen on active pane
        Alt+w / Alt+q       Kill active pane
        Alt+t               Create new window tab
        Alt+n / Alt+p       Switch to next / previous window tab
        Ctrl+Alt+t          Cycle color theme (blue, black, orange, silver, pink, yellow)
        Ctrl+Alt+q          Exit / Detach tmux session

        PREFIX HOTKEYS (Press Ctrl+b, release, then press command key):
        Ctrl+b %            Split active pane vertically (Left / Right)
        Ctrl+b "            Split active pane horizontally (Top / Bottom)
        Ctrl+b <Arrow Keys> Navigate focus between adjacent panes
        Ctrl+b o            Cycle focus to next pane
        Ctrl+b z            Toggle zoom / full-screen on active pane
        Ctrl+b x            Kill active pane
        Ctrl+b c            Create new window tab
        Ctrl+b n / p        Switch to next / previous window tab
        Ctrl+b 0..9         Jump directly to window tab by index
        Ctrl+b ,            Rename current window tab
        Ctrl+b &            Kill current window tab
        Ctrl+b [            Scrollback mode (PgUp/PgDn/Arrows, 'q' to exit)
        Ctrl+b ]            Paste Windows OS clipboard to active pane
        Ctrl+b ?            Toggle interactive help overlay
        Ctrl+b d            Detach / Exit tmux session

        MOUSE SUPPORT & SGR PASSTHROUGH:
        Click               Focus pane / In-App clicking (Vim/Lazygit/Micro)
        Drag Border         Resize split panes dynamically
        Mouse Wheel         Scroll through history / in-app wheel reporting

    CONFIGURATION
        On startup tmux creates %USERPROFILE%\.tmuxrc if it is missing.
        Supported lines include:
            shell powershell.exe
            theme blue|black|orange|silver|pink|yellow
            prefix-key Ctrl+B
            theme-key Ctrl+Alt+T
            bind split-vertical Alt+V
            bind split-horizontal Alt+H
            bind new-window Alt+T
            bind next-window Alt+N
            bind previous-window Alt+P
            bind zoom Alt+Z
            bind kill-pane Alt+W
            bind kill-pane-alt Alt+Q
            bind exit Ctrl+Alt+Q

    EXAMPLES
        tmux
            Start a new tmux session with default shell.

        tmux cmd.exe
            Start a tmux session running cmd.exe.

    CrossShell for UNIX                                                      tmux(1)
)";
}
