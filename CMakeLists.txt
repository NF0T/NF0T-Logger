cmake_minimum_required(VERSION 3.20)

project(NF0T-Logger
    VERSION 0.1.0
    DESCRIPTION "Amateur Radio Contact Logger"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate compile_commands.json for IDE/tooling support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Sql WebSockets)
qt_standard_project_setup()

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
