cmake_minimum_required(VERSION 3.20)
project(mded VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/editor/MdEditor.cpp
    src/editor/EditorWidget.cpp
    src/editor/LineNumberArea.cpp
    src/editor/SearchBar.cpp
    src/editor/TabManager.cpp
    src/editor/SessionManager.cpp
    src/parser/ContextStack.cpp
    src/parser/BlockParser.cpp
    src/parser/InlineParser.cpp
    src/parser/LatexParser.cpp
    src/highlight/MdHighlighter.cpp
    src/config/Theme.cpp
    src/config/Settings.cpp
    src/util/CjkUtil.cpp
    src/util/TomlParser.cpp
)

add_executable(mded ${SOURCES})

target_include_directories(mded PRIVATE src)

target_link_libraries(mded PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)

target_compile_options(mded PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Release>:-O2>
)

# Tests
option(BUILD_TESTS "Build test suite" ON)
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

# Install
install(TARGETS mded RUNTIME DESTINATION bin)
install(DIRECTORY themes DESTINATION share/mded)
install(FILES mded.desktop DESTINATION share/applications)
