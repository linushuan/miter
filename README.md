# mded

mded is a lightweight Markdown editor for Linux.
It focuses on fast typing, syntax highlighting, and clean reading while editing.

## What You Can Do

- Edit Markdown with multi-tab workflow.
- Highlight Markdown, code fences, tables, links/images, and LaTeX.
- Use dark/white theme toggle from the toolbar.
- Auto-continue list items on Enter (bullets, numbers, checkboxes).
- Save files with standard keyboard shortcuts.

## Quick Start

Build and run from source:

```bash
mkdir -p build
cd build
cmake ..
cmake --build . -j$(nproc)
./mded
```

Open files directly:

```bash
./mded notes.md todo.md
```

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

- Use the toolbar button `Theme: ...` to switch between dark and white.
- Theme files are stored in [themes](themes).

## Config File

User settings are saved at:

`~/.config/mded/config.toml`

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
