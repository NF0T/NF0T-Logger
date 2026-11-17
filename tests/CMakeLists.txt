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

# The nf0t-core tests live with the library, in libs/nf0t-core/tests/.
