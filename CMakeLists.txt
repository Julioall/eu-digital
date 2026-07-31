cmake_minimum_required(VERSION 3.25)

project(eu_digital_runtime VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EU_DIGITAL_BUILD_DESKTOP_INTERFACE_SPIKE
    "Build the optional SDL2/Dear ImGui desktop interface spike"
    OFF)

if(EU_DIGITAL_BUILD_DESKTOP_INTERFACE_SPIKE)
    find_package(SDL2 CONFIG REQUIRED)
    find_package(imgui CONFIG REQUIRED)
    add_executable(desktop_interface_spike cpp/app/desktop_interface_spike.cpp)
    target_compile_features(desktop_interface_spike PRIVATE cxx_std_23)
    target_link_libraries(desktop_interface_spike PRIVATE imgui::imgui)
    if(TARGET SDL2::SDL2-static)
        target_link_libraries(desktop_interface_spike PRIVATE SDL2::SDL2-static)
    else()
        target_link_libraries(desktop_interface_spike PRIVATE SDL2::SDL2)
    endif()
endif()

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

add_executable(world_model_test cpp/tests/world_model_test.cpp)
target_include_directories(world_model_test PRIVATE cpp)
target_compile_features(world_model_test PRIVATE cxx_std_23)

add_executable(global_workspace_test cpp/tests/global_workspace_test.cpp)
target_include_directories(global_workspace_test PRIVATE cpp)
target_compile_features(global_workspace_test PRIVATE cxx_std_23)

add_executable(functional_self_model_test cpp/tests/functional_self_model_test.cpp)
target_include_directories(functional_self_model_test PRIVATE cpp)
target_compile_features(functional_self_model_test PRIVATE cxx_std_23)

add_executable(metacognition_curiosity_test cpp/tests/metacognition_curiosity_test.cpp)
target_include_directories(metacognition_curiosity_test PRIVATE cpp)
target_compile_features(metacognition_curiosity_test PRIVATE cxx_std_23)

add_executable(procedural_avatar_test cpp/tests/procedural_avatar_test.cpp)
target_include_directories(procedural_avatar_test PRIVATE cpp)
target_compile_features(procedural_avatar_test PRIVATE cxx_std_23)

add_executable(procedural_avatar_probe cpp/app/procedural_avatar_probe.cpp)
target_include_directories(procedural_avatar_probe PRIVATE cpp)
target_compile_features(procedural_avatar_probe PRIVATE cxx_std_23)

add_executable(local_model_gateway_test cpp/tests/local_model_gateway_test.cpp)
target_include_directories(local_model_gateway_test PRIVATE cpp)
target_compile_features(local_model_gateway_test PRIVATE cxx_std_23)

add_executable(suggestion_orchestrator_test cpp/tests/suggestion_orchestrator_test.cpp)
target_include_directories(suggestion_orchestrator_test PRIVATE cpp)
target_compile_features(suggestion_orchestrator_test PRIVATE cxx_std_23)

add_executable(packaging_test cpp/tests/packaging_test.cpp)
target_include_directories(packaging_test PRIVATE cpp)
target_compile_features(packaging_test PRIVATE cxx_std_23)

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

add_executable(supervised_actions_feedback_test cpp/tests/supervised_actions_feedback_test.cpp)
target_include_directories(supervised_actions_feedback_test PRIVATE cpp)
target_compile_features(supervised_actions_feedback_test PRIVATE cxx_std_23)

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
if(NOT WIN32)
    if(NOT EU_DIGITAL_SQLITE3_LIBRARY)
        message(FATAL_ERROR "SQLite3 runtime library is required for SPEC-006 timeline tests")
    endif()
endif()
add_executable(timeline_store_test cpp/tests/timeline_store_test.cpp)
target_include_directories(timeline_store_test PRIVATE cpp)
target_compile_features(timeline_store_test PRIVATE cxx_std_23)
if(NOT WIN32)
    target_link_libraries(timeline_store_test PRIVATE "${EU_DIGITAL_SQLITE3_LIBRARY}")
endif()

add_executable(cognitive_snapshot_test cpp/tests/cognitive_snapshot_test.cpp)
target_include_directories(cognitive_snapshot_test PRIVATE cpp)
target_compile_features(cognitive_snapshot_test PRIVATE cxx_std_23)
if(NOT WIN32)
    target_link_libraries(cognitive_snapshot_test PRIVATE "${EU_DIGITAL_SQLITE3_LIBRARY}")
endif()
if(WIN32)
    target_link_libraries(cognitive_snapshot_test PRIVATE Crypt32)
endif()

add_executable(runtime_host_test cpp/tests/runtime_host_test.cpp)
target_include_directories(runtime_host_test PRIVATE cpp)
target_compile_features(runtime_host_test PRIVATE cxx_std_23)
target_compile_definitions(runtime_host_test PRIVATE EU_DIGITAL_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
if(NOT WIN32)
    target_link_libraries(runtime_host_test PRIVATE "${EU_DIGITAL_SQLITE3_LIBRARY}")
endif()
if(NOT WIN32)
    target_link_libraries(eu_digital_runtime PRIVATE "${EU_DIGITAL_SQLITE3_LIBRARY}")
endif()
if(WIN32)
    target_link_libraries(runtime_host_test PRIVATE Crypt32)
endif()

add_executable(cognitive_coordinator_test cpp/tests/cognitive_coordinator_test.cpp)
target_include_directories(cognitive_coordinator_test PRIVATE cpp)
target_compile_features(cognitive_coordinator_test PRIVATE cxx_std_23)

add_executable(ports_mock_test cpp/tests/ports_mock_test.cpp)
target_include_directories(ports_mock_test PRIVATE cpp)
target_compile_features(ports_mock_test PRIVATE cxx_std_23)

add_executable(world_model_adapter_test cpp/tests/world_model_adapter_test.cpp)
target_include_directories(world_model_adapter_test PRIVATE cpp)
target_compile_features(world_model_adapter_test PRIVATE cxx_std_23)

add_executable(episodic_memory_adapter_test cpp/tests/episodic_memory_adapter_test.cpp)
target_include_directories(episodic_memory_adapter_test PRIVATE cpp)
target_compile_features(episodic_memory_adapter_test PRIVATE cxx_std_23)

add_executable(episode_segmenter_adapter_test cpp/tests/episode_segmenter_adapter_test.cpp)
target_include_directories(episode_segmenter_adapter_test PRIVATE cpp)
target_compile_features(episode_segmenter_adapter_test PRIVATE cxx_std_23)

add_executable(global_workspace_adapter_test cpp/tests/global_workspace_adapter_test.cpp)
target_include_directories(global_workspace_adapter_test PRIVATE cpp)
target_compile_features(global_workspace_adapter_test PRIVATE cxx_std_23)

add_executable(functional_self_model_adapter_test cpp/tests/functional_self_model_adapter_test.cpp)
target_include_directories(functional_self_model_adapter_test PRIVATE cpp)
target_compile_features(functional_self_model_adapter_test PRIVATE cxx_std_23)

add_executable(metacognition_curiosity_adapter_test cpp/tests/metacognition_curiosity_adapter_test.cpp)
target_include_directories(metacognition_curiosity_adapter_test PRIVATE cpp)
target_compile_features(metacognition_curiosity_adapter_test PRIVATE cxx_std_23)

add_executable(cognitive_decision_adapter_test cpp/tests/cognitive_decision_adapter_test.cpp)
target_include_directories(cognitive_decision_adapter_test PRIVATE cpp)
target_compile_features(cognitive_decision_adapter_test PRIVATE cxx_std_23)

add_executable(local_language_renderer_test cpp/tests/local_language_renderer_test.cpp)
target_include_directories(local_language_renderer_test PRIVATE cpp)
target_compile_features(local_language_renderer_test PRIVATE cxx_std_23)

add_executable(cognitive_decision_policy_test cpp/tests/cognitive_decision_policy_test.cpp)
target_include_directories(cognitive_decision_policy_test PRIVATE cpp)
target_compile_features(cognitive_decision_policy_test PRIVATE cxx_std_23)

add_executable(cognitive_port_factory_test cpp/tests/cognitive_port_factory_test.cpp)
target_include_directories(cognitive_port_factory_test PRIVATE cpp)
target_compile_features(cognitive_port_factory_test PRIVATE cxx_std_23)

enable_testing()
add_test(
    NAME canonical_event_fixture
    COMMAND eu_digital_runtime "${CMAKE_SOURCE_DIR}/contracts/fixtures/canonical_event.json"
)

add_executable(ollama_model_backend_test cpp/tests/ollama_model_backend_test.cpp cpp/core/adapters/ollama_model_backend.cpp)
target_include_directories(ollama_model_backend_test PRIVATE cpp)
target_compile_features(ollama_model_backend_test PRIVATE cxx_std_23)
if(WIN32)
    target_link_libraries(ollama_model_backend_test PRIVATE winhttp)
endif()
add_test(NAME ollama_model_backend COMMAND ollama_model_backend_test)

add_test(NAME event_bus COMMAND event_bus_test)
add_test(NAME cognitive_coordinator COMMAND cognitive_coordinator_test)
add_test(NAME capability_runtime COMMAND capability_runtime_test)
add_test(NAME promotion_registry COMMAND promotion_registry_test)
add_test(NAME episode_segmentation COMMAND episode_segmentation_test)
add_test(NAME episodic_memory COMMAND episodic_memory_test)
add_test(NAME pattern_learning COMMAND pattern_learning_test)
add_test(NAME world_model COMMAND world_model_test)
add_test(NAME global_workspace COMMAND global_workspace_test)
add_test(NAME functional_self_model COMMAND functional_self_model_test)
add_test(NAME metacognition_curiosity COMMAND metacognition_curiosity_test)
add_test(NAME procedural_avatar COMMAND procedural_avatar_test)
add_test(NAME procedural_avatar_probe COMMAND procedural_avatar_probe)
add_test(NAME local_model_gateway COMMAND local_model_gateway_test)
add_test(NAME suggestion_orchestrator COMMAND suggestion_orchestrator_test)
add_test(NAME packaging COMMAND packaging_test)
if(EU_DIGITAL_BUILD_DESKTOP_INTERFACE_SPIKE)
    add_test(NAME desktop_interface_spike COMMAND desktop_interface_spike)
endif()

option(EU_DIGITAL_BUILD_QT_SHELL "Build the Qt 6 QML desktop shell adapter" OFF)
if(EU_DIGITAL_BUILD_QT_SHELL)
    find_package(Qt6 COMPONENTS Core Gui Quick Widgets Concurrent REQUIRED)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC ON)

    add_executable(qt_avatar_shell_test cpp/tests/qt_avatar_shell_test.cpp cpp/shell/qt_avatar_window.cpp)
    target_include_directories(qt_avatar_shell_test PRIVATE cpp)
    target_compile_features(qt_avatar_shell_test PRIVATE cxx_std_23)
    target_link_libraries(qt_avatar_shell_test PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::Widgets)
    add_test(NAME qt_avatar_shell COMMAND qt_avatar_shell_test)

    add_executable(eu_digital_desktop WIN32
        cpp/app/eu_digital_desktop.cpp
        cpp/shell/desktop_controller.cpp
        cpp/shell/desktop_controller.hpp
        cpp/shell/qt_tray_adapter.hpp
        cpp/shell/tray_widget.cpp
        cpp/shell/tray_widget.hpp
        cpp/shell/tray_state_machine.hpp
        cpp/shell/tray_state_machine.cpp
        cpp/shell/quick_panel_widget.hpp
        cpp/shell/quick_panel_widget.cpp
        cpp/shell/settings_window.hpp
        cpp/shell/settings_window.cpp
        cpp/shell/toast_notification.hpp
        cpp/shell/toast_notification.cpp
        cpp/shell/log_manager.hpp
        cpp/shell/log_manager.cpp
        cpp/shell/ollama_dialogue_service.hpp
        cpp/core/adapters/ollama_model_backend.cpp
    )
    target_include_directories(eu_digital_desktop PRIVATE cpp)
    target_compile_features(eu_digital_desktop PRIVATE cxx_std_23)
    target_link_libraries(eu_digital_desktop PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::Widgets Qt6::Concurrent)
    if(NOT WIN32)
        target_link_libraries(eu_digital_desktop PRIVATE "${EU_DIGITAL_SQLITE3_LIBRARY}")
    endif()
    if(WIN32)
        target_link_libraries(eu_digital_desktop PRIVATE Crypt32)
    endif()

    add_executable(desktop_integration_test
        cpp/tests/desktop_integration_test.cpp
        cpp/shell/desktop_controller.cpp
        cpp/shell/desktop_controller.hpp
        cpp/shell/qt_tray_adapter.hpp
        cpp/shell/tray_widget.cpp
        cpp/shell/tray_widget.hpp
        cpp/shell/tray_state_machine.hpp
        cpp/shell/tray_state_machine.cpp
        cpp/shell/quick_panel_widget.hpp
        cpp/shell/quick_panel_widget.cpp
        cpp/shell/settings_window.hpp
        cpp/shell/settings_window.cpp
        cpp/shell/toast_notification.hpp
        cpp/shell/toast_notification.cpp
        cpp/shell/log_manager.hpp
        cpp/shell/log_manager.cpp
        cpp/shell/ollama_dialogue_service.hpp
        cpp/core/adapters/ollama_model_backend.cpp
    )
    target_include_directories(desktop_integration_test PRIVATE cpp)
    target_compile_features(desktop_integration_test PRIVATE cxx_std_23)
    target_link_libraries(desktop_integration_test PRIVATE Qt6::Core Qt6::Gui Qt6::Quick Qt6::Widgets Qt6::Concurrent)
    if(NOT WIN32)
        target_link_libraries(desktop_integration_test PRIVATE "${EU_DIGITAL_SQLITE3_LIBRARY}")
    endif()
    if(WIN32)
        target_link_libraries(desktop_integration_test PRIVATE Crypt32)
    endif()
    add_test(NAME desktop_integration COMMAND desktop_integration_test)
endif()
add_test(NAME supervised_actions_feedback COMMAND supervised_actions_feedback_test)
add_test(NAME world_model_adapter COMMAND world_model_adapter_test)
add_test(NAME episodic_memory_adapter COMMAND episodic_memory_adapter_test)
add_test(NAME episode_segmenter_adapter COMMAND episode_segmenter_adapter_test)
add_test(NAME global_workspace_adapter COMMAND global_workspace_adapter_test)
add_test(NAME functional_self_model_adapter COMMAND functional_self_model_adapter_test)
add_test(NAME metacognition_curiosity_adapter COMMAND metacognition_curiosity_adapter_test)
add_test(NAME cognitive_decision_adapter COMMAND cognitive_decision_adapter_test)
add_test(NAME local_language_renderer COMMAND local_language_renderer_test)
add_test(NAME cognitive_decision_policy COMMAND cognitive_decision_policy_test)
add_test(NAME cognitive_port_factory COMMAND cognitive_port_factory_test)
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
