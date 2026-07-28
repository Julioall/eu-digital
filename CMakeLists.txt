cmake_minimum_required(VERSION 3.25)

project(eu_digital_runtime VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(eu_digital_runtime cpp/app/main.cpp)
target_compile_features(eu_digital_runtime PRIVATE cxx_std_23)

add_executable(event_bus_test cpp/tests/event_bus_test.cpp)
target_include_directories(event_bus_test PRIVATE cpp)
target_compile_features(event_bus_test PRIVATE cxx_std_23)

add_executable(capability_runtime_test cpp/tests/capability_runtime_test.cpp)
target_include_directories(capability_runtime_test PRIVATE cpp)
target_compile_features(capability_runtime_test PRIVATE cxx_std_23)

add_executable(promotion_fixture_runner cpp/app/promotion_fixture_runner.cpp)
target_include_directories(promotion_fixture_runner PRIVATE cpp)
target_compile_features(promotion_fixture_runner PRIVATE cxx_std_23)

add_executable(promotion_registry_test cpp/tests/promotion_registry_test.cpp)
target_include_directories(promotion_registry_test PRIVATE cpp)
target_compile_features(promotion_registry_test PRIVATE cxx_std_23)

add_executable(system_activity_sensor_test cpp/tests/system_activity_sensor_test.cpp)
target_include_directories(system_activity_sensor_test PRIVATE cpp)
target_compile_features(system_activity_sensor_test PRIVATE cxx_std_23)

add_executable(input_interaction_sensor_test cpp/tests/input_interaction_sensor_test.cpp)
target_include_directories(input_interaction_sensor_test PRIVATE cpp)
target_compile_features(input_interaction_sensor_test PRIVATE cxx_std_23)

add_executable(screen_ocr_sensor_test cpp/tests/screen_ocr_sensor_test.cpp)
target_include_directories(screen_ocr_sensor_test PRIVATE cpp)
target_compile_features(screen_ocr_sensor_test PRIVATE cxx_std_23)

set(EU_DIGITAL_SQLITE3_SEARCH_PATHS /lib/x86_64-linux-gnu /usr/lib/x86_64-linux-gnu)
if(DEFINED ENV{VCPKG_INSTALLATION_ROOT})
    list(APPEND EU_DIGITAL_SQLITE3_SEARCH_PATHS
        "$ENV{VCPKG_INSTALLATION_ROOT}/installed/x64-windows/lib")
endif()
find_library(EU_DIGITAL_SQLITE3_LIBRARY
    NAMES sqlite3 sqlite3.lib libsqlite3.so.0
    PATHS ${EU_DIGITAL_SQLITE3_SEARCH_PATHS})
if(NOT EU_DIGITAL_SQLITE3_LIBRARY)
    message(FATAL_ERROR "SQLite3 runtime library is required for SPEC-006 timeline tests")
endif()
add_executable(timeline_store_test cpp/tests/timeline_store_test.cpp)
target_include_directories(timeline_store_test PRIVATE cpp)
target_compile_features(timeline_store_test PRIVATE cxx_std_23)
target_link_libraries(timeline_store_test PRIVATE "${EU_DIGITAL_SQLITE3_LIBRARY}")

enable_testing()
add_test(
    NAME canonical_event_fixture
    COMMAND eu_digital_runtime "${CMAKE_SOURCE_DIR}/contracts/fixtures/canonical_event.json"
)
add_test(NAME event_bus COMMAND event_bus_test)
add_test(NAME capability_runtime COMMAND capability_runtime_test)
add_test(NAME promotion_registry COMMAND promotion_registry_test)
add_test(NAME system_activity_sensor COMMAND system_activity_sensor_test)
add_test(NAME input_interaction_sensor COMMAND input_interaction_sensor_test)
add_test(NAME screen_ocr_sensor COMMAND screen_ocr_sensor_test)
add_test(NAME timeline_store COMMAND timeline_store_test)

include(GNUInstallDirs)
install(TARGETS eu_digital_runtime RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

set(CPACK_PACKAGE_NAME "eu-digital-runtime")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "ZIP")
include(CPack)
