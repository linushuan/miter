# miter Markdown Editing and Highlight Spec

## 1. Scope

This spec defines markdown typing behavior and syntax highlighting in the editor.
It covers:

- Block-level parsing (`src/parser/BlockParser.cpp`)
- Inline parsing (`src/parser/InlineParser.cpp`)
- Rendering and style mapping (`src/highlight/MdHighlighter.cpp`)
- Typing-time autocomplete (`src/editor/MdEditor.cpp`)
- Regression and edge-case tests (`tests/`)

## 2. Parsing Pipeline

1. Each block is classified by `BlockParser` into one of:
   - heading, list, blockquote, table, horizontal rule, code fence, LaTeX display/env, HTML comment, normal text
2. Block tokens are emitted for structural markers.
3. Inline parsing runs only when the block is not code-fence body/start/end and not HTML comment.
4. Inline token precedence is:
   - escape
   - inline code
   - HTML comment
   - inline LaTeX
   - angle autolink
   - linked image link (`[![alt](img)](url)`)
   - image
   - link
   - underline (`++text++`)
   - highlight (`==text==`)
   - bold/italic
   - superscript/subscript
   - strikethrough
   - hard break

## 3. Supported Syntax

### 3.1 Block Syntax

- ATX headings: `#` to `######`
- Setext headings:
  - H1 underline: `===`
  - H2 underline: `---`
  - Leading spaces (0-3) are allowed on underline lines
- Fenced code blocks:
  - Backtick and tilde fences with optional language tag
- Blockquotes:
  - Nested markers and indentation are preserved
- Lists:
  - ordered (`1.`, `1)`)
  - unordered (`-`, `*`, `+`)
  - task checkboxes (`[ ]`, `[x]`, `[X]`)
- Tables: pipe-based rows
- Horizontal rules:
  - `***`, `---`, `___`
  - spaced forms like `- - -`, `* * *`
- LaTeX display fence: `$$ ... $$`
- LaTeX environments: `\begin{env} ... \end{env}`
- HTML comment blocks: `<!-- ... -->`

### 3.2 Inline Syntax

- Inline code with variable backtick length
- Links: `[text](url)`
- Images: `![alt](url)`
- Linked images: `[![alt](img-url)](link-url)`
- Angle autolinks: `<https://example.com>`, `<mailto:a@b.com>`, `<www.example.com>`
- Bold/italic/bold-italic using `*`/`_`
- Strikethrough: `~~text~~`
- Underline extension: `++text++`
- Highlight extension: `==text==`
- Superscript extension: `^text^`
- Subscript extension: `~text~`
- Hard breaks via trailing 2+ spaces or trailing backslash
- Escape sequences (`\*`, `\_`, etc.)

## 4. Autocomplete Rules

### 4.1 Pair Autocomplete

Pairs supported:

- `()` `[]` `{}` `<>` `$$` ```` ``

Rules:

- Autocomplete only triggers when there is no non-space text after the cursor in the current line.
- Autocomplete does not trigger when text is selected.
- If the typed closer already exists at cursor position, only move cursor right (do not insert duplicate).
- Existing closer skip logic applies to `) ] } > $ ``.

### 4.2 Multiline Autocomplete on Enter

At end-of-line (without Shift+Enter):

- `$$` opens a display block and inserts closing `$$`
- ````` (with or without language, e.g. ` ```python`) opens a fenced code block and inserts closing `````
- `\begin{env}` inserts matching `\end{env}`
- Existing immediate auto-closed block is detected to avoid duplicate fence/env insertion

### 4.3 Line Continuation

- Ordered and unordered lists continue on Enter.
- Blockquote continuation preserves current indentation and marker depth.
- Horizontal rule lines (`***`, `- - -`, `* * *`, etc.) are not treated as list items on Enter.

### 4.4 Explicit Non-goals for Autocomplete

No autocomplete is performed for:

- superscript/subscript markers
- underline markers
- highlight markers

## 5. Highlighting Rules

- Blockquote markers and body receive a gray translucent background.
- Strikethrough content uses strike-out font style.
- `==highlight==` applies background highlight color to content.
- `++underline++` applies underline font style to content.
- Superscript/subscript content uses vertical alignment and reduced size.
- Checkbox markers are highlighted with list bullet color.
- Angle autolinks use link bracket/url colors.
- Linked-image syntax colors image alt/url and outer link url distinctly.

## 6. Setext Consistency Rule

Setext headings depend on the next line, which is a backward dependency.
To keep rendering correct during live typing:

- the highlighter queues a safe full refresh when a normal-context line may affect setext state (`-`, `=`, or empty lines)
- refresh is queued (not reentrant) to avoid crashes

## 7. Test Coverage Requirements

Edge-case coverage is required in:

- `tests/test_md_editor.cpp`
- `tests/test_block_parser.cpp`
- `tests/test_inline_parser.cpp`
- `tests/test_highlighter.cpp`

Minimum verification includes:

- pair autocomplete and closer-skip logic
- multiline fence/env autocomplete behavior
- blockquote continuation depth/indent
- HR/list ambiguity on Enter
- setext heading correctness after incremental edits
- nested image link parsing/highlighting
- checkbox coloring
- strikethrough style and highlight background styling

## 8. Validation Command

```bash
cmake --build build -j$(nproc)
cd build
ctest --output-on-failure
```
