# miter

miter is a lightweight, cross-platform Markdown editor built with Qt 6.
It focuses on fast typing, stable highlighting, and predictable editing behavior.

## Core Features

- Multi-tab Markdown editing workflow.
- Real-time Markdown highlighting with stateful block parsing.
- Extended inline syntax support:
	- `++underline++`
	- `==highlight==`
	- superscript `^text^`
	- subscript `~text~`
	- strikethrough `~~text~~` (strike-out font style)
- LaTeX support:
	- inline `$...$`
	- display `$$ ... $$`
	- environment blocks `\begin{env} ... \end{env}`
- Link and image support:
	- `[text](url)`
	- `![alt](url)`
	- linked image `[![alt](image-url)](link-url)`
	- angle autolinks `<https://example.com>` and `<user@example.com>`
- Task list checkbox marker highlighting (`[ ]`, `[x]`, `[X]`).
- Blockquote styling with a gray background and quote-border color.

## Autocomplete Behavior

- Pair autocomplete at line end (no non-space text after cursor):
	- `()`, `[]`, `{}`
	- `<>`
	- `$...$`
	- `` `...` ``
- Closer skip-over behavior:
	- typing a closer on an existing closer moves cursor right instead of inserting.
- Enter-based multiline autocomplete:
	- `$$` becomes:
		- opening `$$`
		- empty line
		- closing `$$`
	- lines starting with three backticks (for example, backtick-fence plus language tag like `python`) auto-insert a closing fence.
	- `\\begin{env}` auto-inserts matching `\\end{env}`.
- Enter line continuation:
	- list continuation for ordered/unordered/task items
	- blockquote continuation with preserved indentation and quote depth
- Horizontal-rule safety:
	- `***`, `- - -`, and `* * *` are not treated as lists on Enter.

Complete behavior details are documented in [spec.md](spec.md).

## Project Structure

- [src/editor](src/editor): editor widgets, keyboard behaviors, tab/session/search management.
- [src/highlight](src/highlight): `MdHighlighter` token-to-format rendering.
- [src/parser](src/parser): block parser, inline parser, LaTeX parser, context stack.
- [src/config](src/config): settings/theme loading.
- [themes](themes): TOML theme files.
- [tests](tests): parser/editor/highlighter regression tests.
- [scripts](scripts): platform packaging and local update scripts.

## Build And Run

### Linux

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

```bash
brew install cmake qt
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j$(sysctl -n hw.ncpu)
open build/miter.app
```

### Windows (Developer PowerShell)

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64"
cmake --build build --config Release
.\build\Release\miter.exe
```

## Test

Run full test suite:

```bash
cmake --build build -j$(nproc)
cd build
ctest --output-on-failure
```

Markdown-focused tests:

```bash
cmake --build build -j$(nproc) --target test_block_parser test_inline_parser test_latex_parser test_md_editor test_highlighter
cd build
ctest --output-on-failure -R "test_block_parser|test_inline_parser|test_latex_parser|test_md_editor|test_highlighter"
```

## Packaging

Run packaging on the target OS.

### macOS package

```bash
./scripts/package-macos.sh
```

Output: `.dmg` in the macOS build directory (default `build-macos/`).

### Windows package

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-windows.ps1
```

Output: `.zip` in the Windows build directory (default `build-win\`).

## Local Install Or Update (Linux)

```bash
./scripts/update-local.sh
```

Default install target: `~/.local`.

## Keyboard Shortcuts

- `Ctrl+O`: open/import file
- `Ctrl+S`: save
- `Ctrl+Shift+S`: save as
- `Ctrl+T`: new tab
- `Ctrl+W`: close tab
- `Ctrl+Tab`: next tab
- `Ctrl+Shift+Tab`: previous tab
- `Ctrl+F`: search
- `Ctrl++` / `Ctrl+-` / `Ctrl+0`: zoom in/out/reset
- `Ctrl+L`: toggle line numbers
- `Ctrl+Shift+W`: toggle word wrap
- `F11`: focus mode
- `F12`: fullscreen

## Themes And Settings

- Theme files live in [themes](themes).
- Use the toolbar theme button to switch dark/white themes.
- Settings are stored in:
	- Linux: `~/.config/miter/config.toml`
	- macOS: `~/Library/Preferences/miter/config.toml`
	- Windows: `%LOCALAPPDATA%\\miter\\config.toml`

Example:

```toml
[editor]
tab_size = 2
```

`tab_size` controls inserted spaces for `Tab` and list indentation logic.

## Acknowledgements

See [NOTICE](NOTICE) for third-party references and notices.

## License

GPL-3.0
