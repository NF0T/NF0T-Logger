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
    app/settings/pages/CallsignLookupPage.cpp
    ui/QsoTableModel.cpp
    ui/RadioPanel.cpp
    ui/entrypanel/QsoQuickEntryPanel.cpp
    ui/LogFilterBar.cpp
    ui/QsoFullEntryDialog.cpp
    ui/NewLogDialog.cpp
    ui/WhatsNewDialog.cpp
    ui/QslColumns.cpp
    ui/QslDownloadDialog.cpp
    ui/QslUploadDialog.cpp
    ui/MigrateDatabaseDialog.cpp
    radio/HamlibBackend.cpp
    radio/HamlibWorker.cpp
    radio/TciBackend.cpp
    wsjtx/WsjtxService.cpp
    qsl/LoTwService.cpp
    qsl/EqslService.cpp
    qsl/QrzService.cpp
    qsl/ClubLogService.cpp
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
    app/settings/pages/CallsignLookupPage.h
    ui/QsoTableModel.h
    ui/RadioPanel.h
    ui/entrypanel/QsoQuickEntryPanel.h
    ui/LogFilterBar.h
    ui/QsoFullEntryDialog.h
    ui/NewLogDialog.h
    ui/WhatsNewDialog.h
    ui/QslColumns.h
    ui/QslDownloadDialog.h
    ui/QslUploadDialog.h
    ui/MigrateDatabaseDialog.h
    radio/DigitalListenerService.h
    radio/HamlibBackend.h
    radio/HamlibWorker.h
    radio/RadioBackend.h
    radio/TciBackend.h
    wsjtx/WsjtxService.h
    qsl/QslService.h
    qsl/LoTwService.h
    qsl/EqslService.h
    qsl/QrzService.h
    qsl/ClubLogService.h
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

qt_add_resources(NF0T-Logger "data_resources"
    PREFIX "/data"
    BASE "${CMAKE_SOURCE_DIR}/data"
    FILES
        "${CMAKE_SOURCE_DIR}/data/cty.dat"
)

target_include_directories(NF0T-Logger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(NF0T-Logger PRIVATE
    nf0t-core
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::WebSockets
    Qt6::Concurrent
    qt6keychain
)

if(HAMLIB_FOUND)
    target_include_directories(NF0T-Logger PRIVATE ${HAMLIB_INCLUDE_DIRS})
    target_link_directories(NF0T-Logger PRIVATE ${HAMLIB_LIBRARY_DIRS})
    target_link_libraries(NF0T-Logger PRIVATE ${HAMLIB_LINK_LIBRARIES})
endif()

if(WIN32)
    target_sources(NF0T-Logger PRIVATE
        "${CMAKE_SOURCE_DIR}/packaging/windows/nf0t-logger.rc"
    )
endif()

# Install rules
include(GNUInstallDirs)
install(TARGETS NF0T-Logger
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(UNIX AND NOT APPLE)
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/linux/nf0t-logger.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES "${CMAKE_SOURCE_DIR}/packaging/linux/nf0t-logger.png"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps")
endif()

# Windows runtime deployment (windeployqt + dependent DLLs) is in the top-level
# CMakeLists.txt, so that it runs after every executable has been installed.
