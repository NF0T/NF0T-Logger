set(SOURCES
    main.cpp
    app/MainWindow.cpp
    app/settings/SecureSettings.cpp
    app/settings/Settings.cpp
    app/settings/SettingsDialog.cpp
    app/settings/pages/StationPage.cpp
    app/settings/pages/EquipmentPage.cpp
    app/settings/pages/DatabasePage.cpp
    app/settings/pages/RadioPage.cpp
    app/settings/pages/QslPage.cpp
    core/logbook/QsoTableModel.cpp
    core/adif/AdifParser.cpp
    core/adif/AdifWriter.cpp
    core/BandPlan.cpp
    ui/entrypanel/QsoEntryPanel.cpp
    radio/HamlibBackend.cpp
    radio/TciBackend.cpp
    qsl/LoTwService.cpp
    qsl/EqslService.cpp
    qsl/QrzService.cpp
    qsl/ClubLogService.cpp
    database/SqlBackendBase.cpp
    database/SqliteBackend.cpp
    database/MariaDbBackend.cpp
)

set(HEADERS
    app/MainWindow.h
    app/settings/SecureSettings.h
    app/settings/Settings.h
    app/settings/SettingsDialog.h
    app/settings/pages/SettingsPage.h
    app/settings/pages/StationPage.h
    app/settings/pages/EquipmentPage.h
    app/settings/pages/DatabasePage.h
    app/settings/pages/RadioPage.h
    app/settings/pages/QslPage.h
    core/logbook/Qso.h
    core/logbook/QsoFilter.h
    core/logbook/QsoTableModel.h
    core/adif/AdifParser.h
    core/adif/AdifWriter.h
    core/BandPlan.h
    ui/entrypanel/QsoEntryPanel.h
    radio/HamlibBackend.h
    radio/TciBackend.h
    qsl/QslService.h
    qsl/LoTwService.h
    qsl/EqslService.h
    qsl/QrzService.h
    qsl/ClubLogService.h
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
    qt6keychain
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
