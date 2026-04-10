find_package(Qt6 REQUIRED COMPONENTS Core Sql)

qt_add_executable(log4om-migrate
    main.cpp
    Log4OmMigrator.h
    Log4OmMigrator.cpp
    # Reuse database backend directly (no separate library target in src/)
    ${CMAKE_SOURCE_DIR}/src/database/SqlBackendBase.cpp
    ${CMAKE_SOURCE_DIR}/src/database/SqliteBackend.cpp
)

target_include_directories(log4om-migrate PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src        # reuse src/core and src/database headers
)

target_link_libraries(log4om-migrate PRIVATE
    Qt6::Core
    Qt6::Sql
)

set_target_properties(log4om-migrate PROPERTIES
    WIN32_EXECUTABLE FALSE          # force console subsystem on Windows
)

install(TARGETS log4om-migrate
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
