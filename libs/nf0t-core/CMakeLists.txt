# nf0t-core: application-independent logbook code shared by NF0T Logger and
# CC Companion. See docs/nf0t-core-plan.md in NF0T Logger.
#
# Builds as part of a consuming project (add_subdirectory) or on its own.
# It may only include its own sources: there is deliberately no include path
# to any consumer, so a dependency on application code is a compile error.

# PROJECT_IS_TOP_LEVEL needs CMake 3.21; the consumer's floor is 3.20.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.20)
    project(nf0t-core LANGUAGES CXX)
    find_package(Qt6 REQUIRED COMPONENTS Core Sql Network)
    enable_testing()
endif()

option(NF0T_CORE_BUILD_TESTS "Build the nf0t-core unit tests" ON)

add_library(nf0t-core STATIC
    src/core/BandPlan.cpp
    src/core/BandPlan.h
    src/core/Callsign.cpp
    src/core/Callsign.h
    src/core/Maidenhead.cpp
    src/core/Maidenhead.h
    src/core/adif/AdifParser.cpp
    src/core/adif/AdifParser.h
    src/core/adif/AdifWriter.cpp
    src/core/adif/AdifWriter.h
    src/core/logbook/Qso.h
    src/core/logbook/QsoFilter.h
    src/database/DatabaseInterface.h
    src/database/MariaDbBackend.cpp
    src/database/MariaDbBackend.h
    src/database/SqlBackendBase.cpp
    src/database/SqlBackendBase.h
    src/database/SqliteBackend.cpp
    src/database/SqliteBackend.h
    src/lookup/CallsignLookupProvider.h
    src/lookup/CallsignLookupResult.h
    src/lookup/CtyDatLookupProvider.cpp
    src/lookup/CtyDatLookupProvider.h
    src/lookup/QrzXmlLookupProvider.cpp
    src/lookup/QrzXmlLookupProvider.h
)

target_include_directories(nf0t-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(nf0t-core PUBLIC Qt6::Core Qt6::Sql Qt6::Network)

# Set on the target rather than inherited: a consumer's
# qt_standard_project_setup() does not exist when this builds on its own.
set_target_properties(nf0t-core PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    AUTOMOC ON
)
target_compile_features(nf0t-core PUBLIC cxx_std_23)

if(NF0T_CORE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
