function(add_parser_test target_name source_file)
    add_executable(${target_name}
        ${source_file}
        ../src/parser/BlockParser.cpp
        ../src/parser/InlineParser.cpp
        ../src/parser/LatexParser.cpp
        ../src/parser/ContextStack.cpp
        ../src/util/CjkUtil.cpp
    )
    target_include_directories(${target_name} PRIVATE ../src)
    target_link_libraries(${target_name} Qt6::Test Qt6::Core)
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

add_parser_test(test_block_parser test_block_parser.cpp)
add_parser_test(test_inline_parser test_inline_parser.cpp)
add_parser_test(test_latex_parser test_latex_parser.cpp)
add_parser_test(test_context_stack test_context_stack.cpp)

add_executable(test_editor
    test_tab_manager.cpp
    ../src/editor/TabManager.cpp
    ../src/editor/EditorWidget.cpp
    ../src/editor/MdEditor.cpp
    ../src/editor/LineNumberArea.cpp
    ../src/editor/SearchBar.cpp
    ../src/editor/SessionManager.cpp
    ../src/highlight/MdHighlighter.cpp
    ../src/parser/BlockParser.cpp
    ../src/parser/InlineParser.cpp
    ../src/parser/LatexParser.cpp
    ../src/parser/ContextStack.cpp
    ../src/config/Theme.cpp
    ../src/config/Settings.cpp
    ../src/util/CjkUtil.cpp
    ../src/util/TomlParser.cpp
)
target_include_directories(test_editor PRIVATE ../src)
target_link_libraries(test_editor Qt6::Test Qt6::Widgets Qt6::Core Qt6::Gui)
add_test(NAME editor_tests COMMAND test_editor)

add_executable(test_md_editor
    test_md_editor.cpp
    ../src/editor/MdEditor.cpp
    ../src/editor/LineNumberArea.cpp
    ../src/highlight/MdHighlighter.cpp
    ../src/parser/BlockParser.cpp
    ../src/parser/InlineParser.cpp
    ../src/parser/LatexParser.cpp
    ../src/parser/ContextStack.cpp
    ../src/config/Theme.cpp
    ../src/config/Settings.cpp
    ../src/util/CjkUtil.cpp
    ../src/util/TomlParser.cpp
)
target_include_directories(test_md_editor PRIVATE ../src)
target_link_libraries(test_md_editor Qt6::Test Qt6::Widgets Qt6::Core Qt6::Gui)
add_test(NAME test_md_editor COMMAND test_md_editor)

add_executable(test_highlighter
    test_highlighter.cpp
    ../src/highlight/MdHighlighter.cpp
    ../src/parser/BlockParser.cpp
    ../src/parser/InlineParser.cpp
    ../src/parser/LatexParser.cpp
    ../src/parser/ContextStack.cpp
    ../src/config/Theme.cpp
    ../src/util/CjkUtil.cpp
    ../src/util/TomlParser.cpp
)
target_include_directories(test_highlighter PRIVATE ../src)
target_link_libraries(test_highlighter Qt6::Test Qt6::Core Qt6::Gui)
add_test(NAME test_highlighter COMMAND test_highlighter)
