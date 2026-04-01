# miter Markdown Behavior Spec

## 1. Purpose

This document is the source of truth for Markdown parsing, typing-time autocomplete,
and syntax highlighting behavior in miter.

Implementation mapping:

- Block parsing: `src/parser/BlockParser.cpp`
- Inline parsing: `src/parser/InlineParser.cpp`
- LaTeX inline/body tokenization: `src/parser/LatexParser.cpp`
- Stateful highlight rendering: `src/highlight/MdHighlighter.cpp`
- Keyboard and Enter autocomplete: `src/editor/MdEditor.cpp`
- Regression tests: `tests/`

## 2. Processing Model

For each line, highlighting is resolved in this order:

1. Restore parse context from previous block (`ContextStack`).
2. Classify current block (`BlockParser::classify`) and emit block tokens.
3. Apply block token formats.
4. Apply inline parsing where eligible.
5. Save updated context for the next block.

Inline parsing is skipped for code-fence bodies and HTML comment blocks.

## 3. Block Grammar

### 3.1 Supported block types

- ATX headings (`#` to `######`)
- Setext headings
  - H1 underline: `===` (3+)
  - H2 underline: `---` (3+)
  - leading indentation up to 3 spaces allowed on underline line
- Fenced code blocks
  - backtick and tilde fences
  - info strings accepted after opening fence (for example `c++`, `python`, `objective-c`)
- Blockquotes with nested depth
- Ordered and unordered lists
- Task list markers (`[ ]`, `[x]`, `[X]`)
- Table rows (pipe-based)
- Horizontal rules
  - compact: `***`, `---`, `___`
  - spaced: `* * *`, `- - -`
- LaTeX display blocks (`$$`)
- LaTeX environment blocks (`\begin{env}` / `\end{env}`)
- HTML comment blocks (`<!-- ... -->`)

### 3.2 Disambiguation rules

- Horizontal rules are not parsed as list items.
- `\begin{env}` starts a LaTeX environment block only when it is a standalone line.
- Setext underlines are only treated as setext markers when the previous line is non-empty and context is normal text.

## 4. Inline Grammar

Inline parser precedence (top to bottom):

1. escapes
2. inline code
3. inline HTML comments
4. inline LaTeX
5. angle autolinks (`<...>`)
6. linked images (`[![alt](img)](url)`)
7. images (`![alt](url)`)
8. links (`[text](url)`)
9. underline (`++text++`)
10. highlight (`==text==`)
11. emphasis (`*`, `_`, `**`, `***`)
12. superscript/subscript (`^text^`, `~text~`)
13. strikethrough (`~~text~~`)
14. hard breaks

### 4.1 Supported inline extensions

- Underline: `++text++`
- Highlight: `==text==`
- Superscript: `^text^`
- Subscript: `~text~`
- Angle autolink: `<https://...>`, `<user@example.com>`
- Linked-image link: `[![image](url)](url)`

## 5. Autocomplete Specification

### 5.1 Pair insertion

Pairs:

- `()`
- `[]`
- `{}`
- `<>`
- `$...$`
- `` `...` ``

Rules:

- Insert pair only if:
  - there is no selection
  - there is no non-space text after cursor on current line
- Typing a closer when the same closer is already at cursor position moves cursor right (skip-over).

### 5.2 Enter-based block completion

When cursor is at end-of-line and Enter is pressed (without Shift):

- On line `$$`, insert a closing `$$` and place cursor on the middle line.
- On lines starting with three backticks, insert a matching closing fence.
- On standalone `\begin{env}`, insert matching `\end{env}`.
- If the immediate closing block already exists, do not duplicate it.

### 5.3 Continuation behavior

- Lists auto-continue on Enter.
- Blockquotes auto-continue using current indentation and quote depth.
- HR lines (`***`, `- - -`, `* * *`) do not trigger list continuation.

### 5.4 Explicit non-autocomplete syntax

No pair-autocomplete is added for:

- superscript/subscript markers
- underline markers
- highlight markers

## 6. Highlight Mapping

Key visual rules:

- Blockquote marker/body: translucent gray background.
- Strikethrough content: strike-out font style.
- Underline content: font underline enabled.
- Highlight content: background color (`searchHighlightBg`).
- Superscript/subscript: vertical alignment plus reduced font size.
- Checkbox marker: list bullet color.
- Link brackets/text/url and image tokens are independently colorized.
- Linked-image syntax applies image colors internally and link colors externally.

## 7. Setext Refresh Safety

Setext headings require a lookahead to the next block.
To avoid stale formatting and crashes:

- Potential setext-affecting edits queue a full rehighlight.
- Rehighlight is queued through Qt event loop.
- Reentrant highlighting is explicitly prevented.

## 8. Edge Cases Required By This Spec

- `\begin{align*}` environment parsing and autocomplete.
- setext heading updates when underline is typed after heading text.
- horizontal-rule/list ambiguity with compact and spaced forms.
- quote continuation preserving indentation and depth.
- angle brackets both as pair autocomplete and as autolink coloring.
- nested linked-image syntax parsing/highlighting.
- checkbox marker colorization for lowercase/uppercase checked states.
- token format ranges staying in bounds for complex markdown samples.

## 9. Test Matrix

Primary files:

- `tests/test_md_editor.cpp`
- `tests/test_block_parser.cpp`
- `tests/test_inline_parser.cpp`
- `tests/test_highlighter.cpp`

Coverage includes:

- pair insertion and closer skip-over
- Enter-based multiline completion for `$$`, code fences, and `\begin{env}`
- blockquote/list continuation behavior
- block parser disambiguation (HR vs list, setext handling)
- inline extension tokenization and malformed syntax rejection
- highlighting correctness and format safety bounds

## 10. Validation Commands

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
cd build
ctest --output-on-failure
```
