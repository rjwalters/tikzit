cmake_minimum_required(VERSION 3.16)

project(tikzit LANGUAGES CXX VERSION 2.2)

set(TIKZIT_VERSION "${PROJECT_VERSION}")

# Get git commit hash for the about dialog
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "unknown")
endif()
add_compile_definitions(TIKZIT_GIT_HASH="${GIT_COMMIT_HASH}")

if (NOT QT_VER)
    set (QT_VER "Qt6")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
#set(CMAKE_AUTOUIC ON)

find_package(${QT_VER} REQUIRED COMPONENTS Core Gui Widgets Pdf)
find_package(BISON)
find_package(FLEX)


include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/data
    ${PROJECT_SOURCE_DIR}/src/gui
    ${PROJECT_SOURCE_DIR}/src/layout
)


BISON_TARGET(PARSER src/data/tikzparser.y ${CMAKE_CURRENT_BINARY_DIR}/tikzparser.parser.cpp)
FLEX_TARGET(LEXER src/data/tikzlexer.l ${CMAKE_CURRENT_BINARY_DIR}/tikzlexer.lexer.cpp)
ADD_FLEX_BISON_DEPENDENCY(LEXER PARSER)


#cmake_policy(SET CMP0071 NEW) # run AUTOXXX on generated files ('NEW' default behaviour)

set(SOURCES
    src/data/delimitedstringvalidator.cpp
    src/data/edge.cpp
    src/data/path.cpp
    src/data/graph.cpp
    src/data/graphelementdata.cpp
    src/data/graphelementproperty.cpp
    src/data/node.cpp
    src/data/pdfdocument.cpp
    src/data/style.cpp
    src/data/stylelist.cpp
    src/data/tikzassembler.cpp
    src/data/tikzdocument.cpp
    src/data/tikzstyles.cpp
    src/gui/commands.cpp
    src/gui/delimitedstringitemdelegate.cpp
    src/gui/edgeitem.cpp
    src/gui/pathitem.cpp
    src/gui/exportdialog.cpp
    src/gui/latexprocess.cpp
    src/gui/mainmenu.cpp
    src/gui/mainwindow.cpp
    src/gui/nodeitem.cpp
    src/gui/preferencedialog.cpp
    src/gui/previewpanel.cpp
    src/gui/previewwindow.cpp
    src/gui/propertypalette.cpp
    src/gui/styleeditor.cpp
    src/gui/stylepalette.cpp
    src/gui/tikzscene.cpp
    src/gui/tikzview.cpp
    src/gui/toolpalette.cpp
    src/gui/undocommands.cpp
    src/layout/autolayout.cpp
    src/layout/edgerouter.cpp
    src/layout/layeredplacement.cpp
    src/layout/layouteval.cpp
    src/main.cpp
    src/tikzit.cpp
    src/util.cpp
)

set(HEADERS
    src/data/edge.h
    src/data/path.h
    src/data/graph.h
    src/data/graphelementdata.h
    src/data/graphelementproperty.h
    src/data/node.h
    src/data/pdfdocument.h
    src/data/style.h
    src/data/stylelist.h
    src/data/tikzassembler.h
    src/data/tikzdocument.h
    src/data/tikzparserdefs.h
    src/data/tikzstyles.h
    src/gui/commands.h
    src/gui/delimitedstringitemdelegate.h
    src/gui/edgeitem.h
    src/gui/pathitem.h
    src/gui/exportdialog.h
    src/gui/latexprocess.h
    src/gui/mainmenu.h
    src/gui/mainwindow.h
    src/gui/nodeitem.h
    src/gui/preferencedialog.h
    src/gui/previewpanel.h
    src/gui/previewwindow.h
    src/gui/propertypalette.h
    src/gui/styleeditor.h
    src/gui/stylepalette.h
    src/gui/tikzscene.h
    src/gui/tikzview.h
    src/gui/toolpalette.h
    src/gui/undocommands.h
    src/layout/autolayout.h
    src/layout/edgerouter.h
    src/layout/layeredplacement.h
    src/layout/layoutconfig.h
    src/layout/layouteval.h
    src/tikzit.h
    src/util.h
)

set(FORMS
    src/gui/exportdialog.ui
    src/gui/mainmenu.ui
    src/gui/mainwindow.ui
    src/gui/preferencedialog.ui
    src/gui/previewwindow.ui
    src/gui/propertypalette.ui
    src/gui/styleeditor.ui
    src/gui/stylepalette.ui
)

if (${QT_VER} STREQUAL "Qt6")
    qt6_wrap_ui(FORM_HEADERS ${FORMS})
    qt6_add_resources(QT_RESOURCES tikzit.qrc)
else()
    qt5_wrap_ui(FORM_HEADERS ${FORMS})
    qt5_add_resources(QT_RESOURCES tikzit.qrc)
endif()



# macOS app bundle resources
if(APPLE)
    set(MACOSX_ICON "${CMAKE_CURRENT_SOURCE_DIR}/images/tikzit.icns")
    set_source_files_properties(${MACOSX_ICON} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
endif()

add_executable(tikzit
    ${SOURCES}
    ${HEADERS}
    ${FORM_HEADERS}
    ${FLEX_LEXER_OUTPUTS}
    ${BISON_PARSER_OUTPUTS}
    ${QT_RESOURCES}
    $<$<PLATFORM_ID:Darwin>:${MACOSX_ICON}>)

# macOS app bundle configuration
if(APPLE)
    set_target_properties(tikzit PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "TikZiT"
        MACOSX_BUNDLE_BUNDLE_VERSION "${TIKZIT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${TIKZIT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.tikzit"
        MACOSX_BUNDLE_ICON_FILE "tikzit.icns"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    )
endif()

target_link_libraries(${PROJECT_NAME}
    "${QT_VER}::Core"
    "${QT_VER}::Gui"
    "${QT_VER}::Widgets"
    "${QT_VER}::Pdf")

# Install
if(APPLE)
    install(TARGETS tikzit BUNDLE DESTINATION ".")

    # Remove qt.conf that points to bundled plugins (breaks non-bundled installs)
    # and strip any stale frameworks left by a previous macdeployqt run
    install(CODE "
        file(REMOVE \"\${CMAKE_INSTALL_PREFIX}/tikzit.app/Contents/Resources/qt.conf\")
        file(REMOVE_RECURSE \"\${CMAKE_INSTALL_PREFIX}/tikzit.app/Contents/Frameworks\")
        file(REMOVE_RECURSE \"\${CMAKE_INSTALL_PREFIX}/tikzit.app/Contents/PlugIns\")
    ")

    # Ad-hoc codesign the installed bundle so macOS doesn't reject it
    install(CODE "
        execute_process(
            COMMAND codesign --force --deep --sign - \"\${CMAKE_INSTALL_PREFIX}/tikzit.app\"
        )
    ")

    # macdeployqt target: bundles Qt frameworks into the .app for redistribution.
    # Use 'cmake --build . --target bundle' then 'cmake --install' for a
    # self-contained app that doesn't require Homebrew Qt at runtime.
    find_program(MACDEPLOYQT macdeployqt HINTS "${${QT_VER}_DIR}/../../../bin")
    if(MACDEPLOYQT)
        add_custom_target(bundle
            COMMAND "${MACDEPLOYQT}" "$<TARGET_BUNDLE_DIR:tikzit>"
            COMMENT "Running macdeployqt on tikzit.app..."
            DEPENDS tikzit
        )
    else()
        message(STATUS "macdeployqt not found — 'bundle' target will not be available")
    endif()
else()
    install(TARGETS tikzit DESTINATION bin)
endif()


# Tests
enable_testing()
find_package(${QT_VER} REQUIRED COMPONENTS Test)

set(TEST_SOURCES
    src/test/testmain.cpp
    src/test/testtest.cpp
    src/test/testparser.cpp
    src/test/testtikzoutput.cpp
)

set(TEST_HEADERS
    src/test/testtest.h
    src/test/testparser.h
    src/test/testtikzoutput.h
)

# Use the same app sources minus main.cpp (data layer is coupled to GUI)
set(APP_SOURCES ${SOURCES})
list(FILTER APP_SOURCES EXCLUDE REGEX "src/main\\.cpp$")

add_executable(tikzit-tests
    ${TEST_SOURCES}
    ${TEST_HEADERS}
    ${APP_SOURCES}
    ${HEADERS}
    ${FORM_HEADERS}
    ${FLEX_LEXER_OUTPUTS}
    ${BISON_PARSER_OUTPUTS}
    ${QT_RESOURCES}
)

target_link_libraries(tikzit-tests
    "${QT_VER}::Core"
    "${QT_VER}::Gui"
    "${QT_VER}::Widgets"
    "${QT_VER}::Pdf"
    "${QT_VER}::Test"
)

add_test(NAME tikzit-tests COMMAND tikzit-tests)
