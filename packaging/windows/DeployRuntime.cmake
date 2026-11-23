# Install-time script (Windows): make the installed bin/ directory self-contained.
#
# windeployqt copies Qt's own DLLs and plugins, but not the non-Qt libraries
# they depend on (ICU, zlib, zstd, pcre2, freetype, harfbuzz, ...), not
# qtkeychain, and — with --no-compiler-runtime — not the GCC runtime. Without
# those, the installed app cannot start on a machine that has no MSYS2.
# GET_RUNTIME_DEPENDENCIES follows the import tables of everything in bin/
# recursively and copies each non-system DLL in from NF0T_DEPLOY_DIRS.
#
# Expects NF0T_DEPLOY_DIRS (toolchain bin, Qt bin, qtkeychain bin), set by the
# install(CODE) that precedes this script in the top-level CMakeLists.txt.

set(_bin "${CMAKE_INSTALL_PREFIX}/bin")

find_program(_WDQT NAMES windeployqt6 windeployqt HINTS ${NF0T_DEPLOY_DIRS} REQUIRED)
execute_process(
    COMMAND "${_WDQT}"
        --no-translations
        --no-system-d3d-compiler
        --no-compiler-runtime
        # SQL drivers for databases NF0T Logger does not support; they only
        # drag in client libraries (Firebird, ODBC, PostgreSQL, Mimer, Oracle).
        --exclude-plugins qsqlibase,qsqlodbc,qsqlpsql,qsqlmimer,qsqloci
        "${_bin}/NF0T-Logger.exe"
    RESULT_VARIABLE _r
)
if(NOT _r EQUAL 0)
    message(FATAL_ERROR "windeployqt failed: ${_r}")
endif()

file(GLOB_RECURSE _libs "${_bin}/*.dll")
file(GET_RUNTIME_DEPENDENCIES
    EXECUTABLES "${_bin}/NF0T-Logger.exe" "${_bin}/log4om-migrate.exe"
    LIBRARIES ${_libs}
    DIRECTORIES ${NF0T_DEPLOY_DIRS}
    PRE_EXCLUDE_REGEXES "^api-ms-" "^ext-ms-"
    POST_EXCLUDE_REGEXES "^[A-Za-z]:[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\]"
    RESOLVED_DEPENDENCIES_VAR _deps
    UNRESOLVED_DEPENDENCIES_VAR _unresolved
    # A DLL windeployqt already copied into bin/ also resolves from its source
    # directory; those are the same file, not a real conflict.
    CONFLICTING_DEPENDENCIES_PREFIX _conflict
)

foreach(_name IN LISTS _conflict_FILENAMES)
    if(NOT EXISTS "${_bin}/${_name}")
        message(FATAL_ERROR "Ambiguous runtime dependency ${_name}: ${_conflict_${_name}}")
    endif()
endforeach()

foreach(_dep IN LISTS _deps)
    get_filename_component(_name "${_dep}" NAME)
    if(NOT EXISTS "${_bin}/${_name}")
        message(STATUS "Deploying runtime dependency: ${_name}")
        file(COPY "${_dep}" DESTINATION "${_bin}")
    endif()
endforeach()

if(_unresolved)
    # Reported, not fatal: an unresolved DLL behind an optional plugin (e.g. a
    # database client library) should not block packaging. The CI smoke test
    # launches the installed programs with a system-only PATH and fails if
    # anything they actually load is missing.
    message(WARNING "Unresolved runtime dependencies: ${_unresolved}")
endif()
