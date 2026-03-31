cmake_minimum_required(VERSION 3.20)
project(miter VERSION 0.2.0 LANGUAGES CXX)

include(GNUInstallDirs)

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

if(APPLE)
    add_executable(miter MACOSX_BUNDLE ${SOURCES})
elseif(WIN32)
    add_executable(miter WIN32 ${SOURCES})
else()
    add_executable(miter ${SOURCES})
endif()

qt_add_resources(miter "miter_themes"
    PREFIX "/themes"
    BASE themes
    FILES
        themes/dark.toml
        themes/white.toml
)

qt_add_resources(miter "miter_icons"
    PREFIX "/icons"
    BASE assets/icons
    FILES
        assets/icons/miter.svg
)

target_include_directories(miter PRIVATE src)

target_link_libraries(miter PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
)

if(MSVC)
    target_compile_options(miter PRIVATE
        /W4
        /permissive-
        /utf-8
        $<$<CONFIG:Release>:/O2>
    )
else()
    target_compile_options(miter PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Release>:-O2>
    )
endif()

# Tests
option(BUILD_TESTS "Build test suite" ON)
if(BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

# Install
install(TARGETS miter
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY themes DESTINATION ${CMAKE_INSTALL_DATADIR}/miter)

if((APPLE OR WIN32) AND COMMAND qt_generate_deploy_app_script)
    qt_generate_deploy_app_script(
        TARGET miter
        OUTPUT_SCRIPT miter_deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${miter_deploy_script})
endif()

if(UNIX AND NOT APPLE)
    set(MITER_DESKTOP_EXEC "${CMAKE_INSTALL_FULL_BINDIR}/miter")
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/miter.desktop.in
        ${CMAKE_CURRENT_BINARY_DIR}/miter.desktop
        @ONLY
    )

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/miter.desktop DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
    install(FILES assets/icons/miter.svg DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
endif()

set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VENDOR "linushuan")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Minimal Markdown Editor")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_MONOLITHIC_INSTALL ON)

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop;ZIP")
    set(CPACK_DMG_VOLUME_NAME "miter")
elseif(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
