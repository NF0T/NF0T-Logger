find_package(Qt6 REQUIRED COMPONENTS Test)

add_executable(test_tci
    test_tci.cpp
    ../src/radio/TciBackend.cpp
    ../src/radio/TciBackend.h
    ../src/radio/RadioBackend.h
    ../src/app/settings/Settings.cpp
    ../src/app/settings/Settings.h
    ../src/app/settings/SecureSettings.cpp
    ../src/app/settings/SecureSettings.h
)

set_target_properties(test_tci PROPERTIES WIN32_EXECUTABLE FALSE)

target_include_directories(test_tci PRIVATE ../src)

target_link_libraries(test_tci PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::WebSockets
    Qt6::Test
    qt6keychain
)

add_test(NAME test_tci COMMAND test_tci)
