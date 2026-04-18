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
    app/settings/pages/WsjtxPage.cpp
    app/settings/pages/DisplayPage.cpp
    core/logbook/QsoTableModel.cpp
    core/adif/AdifParser.cpp
    core/adif/AdifWriter.cpp
    core/BandPlan.cpp
    core/Maidenhead.cpp
    ui/RadioPanel.cpp
    ui/entrypanel/QsoEntryPanel.cpp
    ui/QsoEditDialog.cpp
    ui/QslColumns.cpp
    ui/QslDownloadDialog.cpp
    ui/QslUploadDialog.cpp
    radio/HamlibBackend.cpp
    radio/TciBackend.cpp
    wsjtx/WsjtxService.cpp
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
    app/settings/pages/WsjtxPage.h
    app/settings/pages/DisplayPage.h
    core/logbook/Qso.h
    core/logbook/QsoFilter.h
    core/logbook/QsoTableModel.h
    core/adif/AdifParser.h
    core/adif/AdifWriter.h
    core/BandPlan.h
    core/Maidenhead.h
    ui/RadioPanel.h
    ui/entrypanel/QsoEntryPanel.h
    ui/QsoEditDialog.h
    ui/QslColumns.h
    ui/QslDownloadDialog.h
    ui/QslUploadDialog.h
    radio/DigitalListenerService.h
    radio/HamlibBackend.h
    radio/RadioBackend.h
    radio/TciBackend.h
    wsjtx/WsjtxService.h
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

qt_add_resources(NF0T-Logger "app_resources"
    PREFIX "/"
    FILES
        resources/icons/nf0t-logger.svg
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
    target_link_directories(NF0T-Logger PRIVATE ${HAMLIB_LIBRARY_DIRS})
    target_link_libraries(NF0T-Logger PRIVATE ${HAMLIB_LINK_LIBRARIES})
endif()

# Install rules
include(GNUInstallDirs)
install(TARGETS NF0T-Logger
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
