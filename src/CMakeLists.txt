set(SOURCES
    main.cpp
    app/MainWindow.cpp
    core/logbook/QsoTableModel.cpp
    database/SqlBackendBase.cpp
    database/SqliteBackend.cpp
    database/MariaDbBackend.cpp
)

set(HEADERS
    app/MainWindow.h
    core/logbook/Qso.h
    core/logbook/QsoFilter.h
    core/logbook/QsoTableModel.h
    database/DatabaseInterface.h
    database/SqlBackendBase.h
    database/SqliteBackend.h
    database/MariaDbBackend.h
)

qt_add_executable(NF0T-Logger
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(NF0T-Logger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(NF0T-Logger PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::WebSockets
)

if(HAMLIB_FOUND)
    target_include_directories(NF0T-Logger PRIVATE ${HAMLIB_INCLUDE_DIRS})
    target_link_libraries(NF0T-Logger PRIVATE ${HAMLIB_LIBRARIES})
endif()

# Install rules
include(GNUInstallDirs)
install(TARGETS NF0T-Logger
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
