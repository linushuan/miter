// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 linushuan

#pragma once

enum class TokenType {
    // Block
    HeadingH1, HeadingH2, HeadingH3, HeadingH4, HeadingH5, HeadingH6,
    HeadingMarker,
    SetextMarker,
    CodeFenceMark,
    CodeFenceLang,
    CodeFenceBody,
    BlockquoteMark,
    BlockquoteBody,
    ListBullet,
    ListBody,
    TablePipe,
    TableHeader,
    TableSeparator,
    TableCell,
    HR,

    // Inline
    Bold, BoldMarker,
    Italic, ItalicMarker,
    BoldItalic, BoldItalicMarker,
    Strikethrough, StrikeMarker,
    InlineCode, InlineCodeMark,
    LinkText, LinkUrl, LinkBracket,
    ImageAlt, ImageUrl, ImageBracket,
    HardBreakSpace,
    HardBreakBackslash,
    Escape,

    // LaTeX
    LatexDelimiter,
    LatexCommand,
    LatexBrace,
    LatexMathBody,
    LatexEnvName,
    LatexBeginEnd,

    // Meta
    Normal,
    Marker,
};
