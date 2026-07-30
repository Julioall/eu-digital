cmake_minimum_required(VERSION 3.25)

project(eu_digital_runtime VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(eu_digital_runtime cpp/app/main.cpp)
target_include_directories(eu_digital_runtime PRIVATE cpp)
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

add_executable(episode_segmentation_test cpp/tests/episode_segmentation_test.cpp)
target_include_directories(episode_segmentation_test PRIVATE cpp)
target_compile_features(episode_segmentation_test PRIVATE cxx_std_23)

add_executable(episodic_memory_test cpp/tests/episodic_memory_test.cpp)
target_include_directories(episodic_memory_test PRIVATE cpp)
target_compile_features(episodic_memory_test PRIVATE cxx_std_23)

add_executable(pattern_learning_test cpp/tests/pattern_learning_test.cpp)
target_include_directories(pattern_learning_test PRIVATE cpp)
target_compile_features(pattern_learning_test PRIVATE cxx_std_23)

add_executable(system_activity_sensor_test cpp/tests/system_activity_sensor_test.cpp)
target_include_directories(system_activity_sensor_test PRIVATE cpp)
target_compile_features(system_activity_sensor_test PRIVATE cxx_std_23)

add_executable(input_interaction_sensor_test cpp/tests/input_interaction_sensor_test.cpp)
target_include_directories(input_interaction_sensor_test PRIVATE cpp)
target_compile_features(input_interaction_sensor_test PRIVATE cxx_std_23)

add_executable(screen_ocr_sensor_test cpp/tests/screen_ocr_sensor_test.cpp)
target_include_directories(screen_ocr_sensor_test PRIVATE cpp)
target_compile_features(screen_ocr_sensor_test PRIVATE cxx_std_23)

add_executable(audio_sensor_test cpp/tests/audio_sensor_test.cpp)
target_include_directories(audio_sensor_test PRIVATE cpp)
target_compile_features(audio_sensor_test PRIVATE cxx_std_23)

add_executable(supervised_actions_test cpp/tests/supervised_actions_test.cpp)
target_include_directories(supervised_actions_test PRIVATE cpp)
target_compile_features(supervised_actions_test PRIVATE cxx_std_23)

add_executable(privacy_storage_test cpp/tests/privacy_storage_test.cpp)
target_include_directories(privacy_storage_test PRIVATE cpp)
target_compile_features(privacy_storage_test PRIVATE cxx_std_23)
if(WIN32)
    target_link_libraries(privacy_storage_test PRIVATE Crypt32)
    target_link_libraries(eu_digital_runtime PRIVATE Crypt32)
endif()

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

add_executable(runtime_host_test cpp/tests/runtime_host_test.cpp)
target_include_directories(runtime_host_test PRIVATE cpp)
target_compile_features(runtime_host_test PRIVATE cxx_std_23)
target_compile_definitions(runtime_host_test PRIVATE EU_DIGITAL_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_link_libraries(runtime_host_test PRIVATE "${EU_DIGITAL_SQLITE3_LIBRARY}")
target_link_libraries(eu_digital_runtime PRIVATE "${EU_DIGITAL_SQLITE3_LIBRARY}")

enable_testing()
add_test(
    NAME canonical_event_fixture
    COMMAND eu_digital_runtime "${CMAKE_SOURCE_DIR}/contracts/fixtures/canonical_event.json"
)
add_test(NAME event_bus COMMAND event_bus_test)
add_test(NAME capability_runtime COMMAND capability_runtime_test)
add_test(NAME promotion_registry COMMAND promotion_registry_test)
add_test(NAME episode_segmentation COMMAND episode_segmentation_test)
add_test(NAME episodic_memory COMMAND episodic_memory_test)
add_test(NAME pattern_learning COMMAND pattern_learning_test)
add_test(NAME system_activity_sensor COMMAND system_activity_sensor_test)
add_test(NAME input_interaction_sensor COMMAND input_interaction_sensor_test)
add_test(NAME screen_ocr_sensor COMMAND screen_ocr_sensor_test)
add_test(NAME audio_sensor COMMAND audio_sensor_test)
add_test(NAME supervised_actions COMMAND supervised_actions_test)
add_test(NAME privacy_storage COMMAND privacy_storage_test)
add_test(NAME timeline_store COMMAND timeline_store_test)
add_test(NAME runtime_host COMMAND runtime_host_test)
add_test(
    NAME runtime_cli_required_dependency_failure
    COMMAND eu_digital_runtime --run
        "${CMAKE_SOURCE_DIR}/contracts/fixtures/runtime_manifest.json"
        "${CMAKE_BINARY_DIR}"
        "${CMAKE_SOURCE_DIR}/contracts/fixtures/canonical_event.json"
        runtime-cli-failure
        2026-07-29T12:00:00Z
)
set_tests_properties(runtime_cli_required_dependency_failure PROPERTIES WILL_FAIL TRUE)
add_test(
    NAME runtime_install_tree
    COMMAND "${CMAKE_COMMAND}"
        "-DBUILD_DIR=${CMAKE_BINARY_DIR}"
        "-DSTAGING_DIR=${CMAKE_BINARY_DIR}/runtime-install-test"
        -P "${CMAKE_SOURCE_DIR}/cpp/tests/runtime_install_tree_test.cmake"
)

include(GNUInstallDirs)
install(TARGETS eu_digital_runtime RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

set(CPACK_PACKAGE_NAME "eu-digital-runtime")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "ZIP")
include(CPack)
