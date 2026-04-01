# miter

miter is a lightweight Markdown editor for Linux, macOS, and Windows.
It focuses on fast typing, syntax highlighting, and clean reading while editing.

## What You Can Do

- Edit Markdown with multi-tab workflow.
- Highlight Markdown, HTML-style comments, code fences, tables, links/images, and LaTeX.
- Highlight extended inline syntax: `++underline++`, `==highlight==`, superscript `^text^`, subscript `~text~`.
- Highlight linked-image syntax: `[![alt](image-url)](link-url)`.
- Highlight angle autolinks: `<https://example.com>`.
- Render strikethrough text with strike-out font style.
- Color task checkbox markers (`[ ]`, `[x]`) in list items.
- Use dark/white theme toggle from the toolbar.
- Auto-continue list items on Enter (bullets, numbers, checkboxes).
- Auto-continue blockquote prefixes with current indentation and depth.
- Auto-close pairs for `()`, `[]`, `{}`, `<>`, `$`, and backticks, with skip-over behavior when closer already exists.
- Auto-insert closing blocks for `$$`, triple-backtick code fences (including language-tagged fences), and `\\begin{env}`.
- Prevent horizontal-rule lines (`***`, `- - -`, `* * *`) from being treated as list items on Enter.
- Save files with standard keyboard shortcuts.
- Detect external file changes, mark the tab, and offer to reload on focus.

## Markdown Behavior Spec

Full behavior and edge-case spec is documented in [spec.md](spec.md).

## Quick Start

### Linux

Build and run from source:

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
./build/miter
```

Open files directly:

```bash
./build/miter notes.md todo.md
```

### macOS

Install dependencies (Homebrew example):

```bash
brew install cmake qt
```

Build:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j$(sysctl -n hw.ncpu)
```

Run:

```bash
open build/miter.app
```

### Windows

Install Qt 6 and CMake, then build from a Developer PowerShell:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64"
cmake --build build --config Release
./build/Release/miter.exe
```

## Packaging (macOS / Windows)

Run packaging on the target OS (you cannot build a runnable macOS package on Linux, and vice versa).

### macOS package

```bash
cd ~/linus/coding/vibe-coding/lite-md
./scripts/package-macos.sh
```

This produces a `.dmg` package in the build directory (default: `build-macos/`).

### Windows package (PowerShell)

```powershell
cd ~\linus\coding\vibe-coding\lite-md
powershell -ExecutionPolicy Bypass -File .\scripts\package-windows.ps1
```

This produces a `.zip` package in the build directory (default: `build-win\`).

### Windows package from Bash (Git Bash / WSL with PowerShell)

```bash
cd ~/linus/coding/vibe-coding/lite-md
./scripts/package-windows.sh
```

## Install / Update In `~/` (Linux)

Use this for first install and for every later update:

```bash
cd ~/linus/coding/vibe-coding/lite-md
./scripts/update-local.sh
```

Default install target is `~/.local`.

## Shortcuts

- `Ctrl+O`: Import/Open file
- `Ctrl+S`: Save
- `Ctrl+Shift+S`: Save As
- `Ctrl+T`: New tab
- `Ctrl+W`: Close tab
- `Ctrl+Tab`: Next tab
- `Ctrl+Shift+Tab`: Previous tab
- `Ctrl+F`: Search
- `Ctrl++` / `Ctrl+-` / `Ctrl+0`: Font zoom in/out/reset
- `Ctrl+L`: Toggle line numbers
- `Ctrl+Shift+W`: Toggle word wrap
- `F11`: Focus mode
- `F12`: Fullscreen

## Themes

- Use the toolbar theme icon button to switch between dark and white.
- Theme files are stored in [themes](themes).

## Config File

User settings are saved at:

- Linux (typical): `~/.config/miter/config.toml`
- macOS (typical): `~/Library/Preferences/miter/config.toml`
- Windows (typical): `%LOCALAPPDATA%\\miter\\config.toml`

Example editor settings:

```toml
[editor]
tab_size = 2
```

`tab_size` controls how many spaces are inserted when you press `Tab`
and the indent width used by list Tab/Shift+Tab behavior.

## Acknowledgements

This project was developed with AI assistance. The following projects were referenced for design decisions and architecture during development:

- [Ghostwriter](https://github.com/KDE/ghostwriter) (GPL-3.0) - editor structure, line number area, focus mode
- [QOwnNotes](https://github.com/pbek/QOwnNotes) (GPL-2.0) - syntax highlighter design
- [Zettlr](https://github.com/Zettlr/Zettlr) (GPL-3.0) - CJK text handling, display math state
- [lite-xl](https://github.com/lite-xl/lite-xl) (MIT) - UI minimalism philosophy
- [render-markdown.nvim](https://github.com/MeanderingProgrammer/render-markdown.nvim) (MIT) - token color semantics
- [Marktext](https://github.com/marktext/marktext) (MIT) - theme structure

See [NOTICE](./NOTICE) for full copyright notices.

## License

GPL-3.0
