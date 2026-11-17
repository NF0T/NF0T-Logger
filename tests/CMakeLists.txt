find_package(Qt6 REQUIRED COMPONENTS Test)

set(NF0T_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../src)

# nf0t_add_test(<name> SOURCES <files...> [LIBS <targets...>])
# One Qt Test executable per component, registered with ctest.
function(nf0t_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE FALSE)
    target_include_directories(${name} PRIVATE ${NF0T_SRC} ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE Qt6::Core Qt6::Test ${ARG_LIBS})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

nf0t_add_test(test_tci
    SOURCES
        test_tci.cpp
        ${NF0T_SRC}/radio/TciBackend.cpp
        ${NF0T_SRC}/radio/TciBackend.h
        ${NF0T_SRC}/radio/RadioBackend.h
        ${NF0T_SRC}/app/settings/Settings.cpp
        ${NF0T_SRC}/app/settings/Settings.h
        ${NF0T_SRC}/app/settings/SecureSettings.cpp
        ${NF0T_SRC}/app/settings/SecureSettings.h
    LIBS Qt6::Network Qt6::WebSockets qt6keychain
)

# --- nf0t-core characterization tests (docs/nf0t-core-plan.md, step A1) ----
# These compile the files that will move into nf0t-core. They move with the
# library to libs/nf0t-core/tests/ in step A3.

nf0t_add_test(test_callsign
    SOURCES test_callsign.cpp ${NF0T_SRC}/core/Callsign.cpp
)

nf0t_add_test(test_maidenhead
    SOURCES test_maidenhead.cpp ${NF0T_SRC}/core/Maidenhead.cpp
)

nf0t_add_test(test_bandplan
    SOURCES test_bandplan.cpp ${NF0T_SRC}/core/BandPlan.cpp
)

nf0t_add_test(test_adif
    SOURCES
        test_adif.cpp
        QsoFixtures.h
        ${NF0T_SRC}/core/adif/AdifParser.cpp
        ${NF0T_SRC}/core/adif/AdifWriter.cpp
)

nf0t_add_test(test_sqlite_backend
    SOURCES
        test_sqlite_backend.cpp
        QsoFixtures.h
        ${NF0T_SRC}/database/SqlBackendBase.cpp
        ${NF0T_SRC}/database/SqliteBackend.cpp
    LIBS Qt6::Sql
)

nf0t_add_test(test_ctydat
    SOURCES
        test_ctydat.cpp
        ${NF0T_SRC}/lookup/CtyDatLookupProvider.cpp
        ${NF0T_SRC}/lookup/CtyDatLookupProvider.h
        ${NF0T_SRC}/lookup/CallsignLookupProvider.h
        ${NF0T_SRC}/lookup/CallsignLookupResult.h
        ${NF0T_SRC}/core/Callsign.cpp
)

nf0t_add_test(test_qrzxml
    SOURCES
        test_qrzxml.cpp
        ${NF0T_SRC}/lookup/QrzXmlLookupProvider.cpp
        ${NF0T_SRC}/lookup/QrzXmlLookupProvider.h
        ${NF0T_SRC}/lookup/CallsignLookupProvider.h
        ${NF0T_SRC}/lookup/CallsignLookupResult.h
    LIBS Qt6::Network
)
