cmake_minimum_required(VERSION 3.20)

project(NF0T-Logger
    VERSION 0.3.0
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
