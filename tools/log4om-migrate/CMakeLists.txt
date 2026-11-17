find_package(Qt6 REQUIRED COMPONENTS Core Sql)

qt_add_executable(log4om-migrate
    main.cpp
    Log4OmMigrator.h
    Log4OmMigrator.cpp
)

target_include_directories(log4om-migrate PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(log4om-migrate PRIVATE
    nf0t-core
    Qt6::Core
    Qt6::Sql
)

set_target_properties(log4om-migrate PROPERTIES
    WIN32_EXECUTABLE FALSE          # force console subsystem on Windows
)

install(TARGETS log4om-migrate
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
