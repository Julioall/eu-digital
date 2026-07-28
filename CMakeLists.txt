cmake_minimum_required(VERSION 3.25)

project(eu_digital_runtime VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(eu_digital_runtime cpp/app/main.cpp)
target_compile_features(eu_digital_runtime PRIVATE cxx_std_23)

enable_testing()
add_test(
    NAME canonical_event_fixture
    COMMAND eu_digital_runtime "${CMAKE_SOURCE_DIR}/contracts/fixtures/canonical_event.json"
)

include(GNUInstallDirs)
install(TARGETS eu_digital_runtime RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

set(CPACK_PACKAGE_NAME "eu-digital-runtime")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "ZIP")
include(CPack)
