cmake_minimum_required(VERSION 3.20)

project(NF0T-Logger
    VERSION 26.6.2
    DESCRIPTION "Amateur Radio Contact Logger"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for IDE/tooling support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Sql WebSockets)
qt_standard_project_setup()

# Place all executables in <build>/bin so artifact paths are predictable
# across platforms regardless of what qt_standard_project_setup() does.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# qtkeychain — secure credential storage (KWallet / libsecret / macOS Keychain)
find_package(Qt6Keychain REQUIRED)
message(STATUS "Qt6Keychain found — secure credential storage enabled")

# Build-time version info — passed as compile definitions so the About dialog
# can show the exact commit the binary was built from.
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_COMMIT_HASH OR GIT_COMMIT_HASH STREQUAL "")
    set(GIT_COMMIT_HASH "unknown")
endif()
message(STATUS "Build commit: ${GIT_COMMIT_HASH}")
add_compile_definitions(
    APP_VERSION="${PROJECT_VERSION}"
    GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
)

# Optional: Hamlib for CAT radio control
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(HAMLIB hamlib)
endif()

if(HAMLIB_FOUND)
    message(STATUS "Hamlib found: ${HAMLIB_VERSION} — CAT control enabled")
    add_compile_definitions(HAVE_HAMLIB)
else()
    message(STATUS "Hamlib not found — CAT control will be disabled")
endif()

add_subdirectory(src)
add_subdirectory(tools/log4om-migrate)

# ── Packaging ────────────────────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME              "NF0T-Logger")
set(CPACK_PACKAGE_VENDOR            "Ryan Butler (NF0T)")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION           "${PROJECT_VERSION}")
set(CPACK_PACKAGE_HOMEPAGE_URL      "https://github.com/NF0T/NF0T-Logger")
set(CPACK_RESOURCE_FILE_LICENSE     "${CMAKE_SOURCE_DIR}/LICENSE")

if(WIN32)
    set(CPACK_GENERATOR             "NSIS")
    set(CPACK_PACKAGE_FILE_NAME     "NF0T-Logger-${PROJECT_VERSION}-win64")
    set(CPACK_NSIS_PACKAGE_NAME     "NF0T Logger")
    set(CPACK_NSIS_DISPLAY_NAME     "NF0T Logger ${PROJECT_VERSION}")
    set(CPACK_NSIS_INSTALL_ROOT     "$PROGRAMFILES64")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MUI_ICON
        "${CMAKE_SOURCE_DIR}/packaging/windows/nf0t-logger.ico")
    set(CPACK_NSIS_MUI_UNIICON
        "${CMAKE_SOURCE_DIR}/packaging/windows/nf0t-logger.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\NF0T-Logger.exe")
    set(CPACK_NSIS_URL_INFO_ABOUT   "https://github.com/NF0T/NF0T-Logger")
    set(CPACK_NSIS_HELP_LINK        "https://github.com/NF0T/NF0T-Logger/issues")

    # Start Menu shortcut (always created)
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\NF0T Logger.lnk' '$INSTDIR\\\\bin\\\\NF0T-Logger.exe'\n  StrCmp \$INSTALL_DESKTOP 1 0 +2\n    CreateShortCut '$DESKTOP\\\\NF0T Logger.lnk' '$INSTDIR\\\\bin\\\\NF0T-Logger.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\NF0T Logger.lnk'\n  StrCmp \$INSTALL_DESKTOP 1 0 +2\n    Delete '$DESKTOP\\\\NF0T Logger.lnk'")

    # Enables the InstallOptions page (PATH + optional Desktop shortcut checkbox, Field 5)
    set(CPACK_NSIS_MODIFY_PATH ON)
endif()

include(CPack)
