find_package(Qt6 REQUIRED COMPONENTS Test)

# nf0t_core_add_test(<name> <sources...>)
# One Qt Test executable per component, linked against the library only.
function(nf0t_core_add_test name)
    add_executable(${name} ${ARGN})
    set_target_properties(${name} PROPERTIES
        WIN32_EXECUTABLE FALSE
        AUTOMOC ON
    )
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${name} PRIVATE nf0t-core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

nf0t_core_add_test(test_callsign       test_callsign.cpp)
nf0t_core_add_test(test_maidenhead     test_maidenhead.cpp)
nf0t_core_add_test(test_bandplan       test_bandplan.cpp)
nf0t_core_add_test(test_adif           test_adif.cpp QsoFixtures.h)
nf0t_core_add_test(test_sqlite_backend test_sqlite_backend.cpp QsoFixtures.h)
nf0t_core_add_test(test_ctydat         test_ctydat.cpp)
nf0t_core_add_test(test_qrzxml         test_qrzxml.cpp)
