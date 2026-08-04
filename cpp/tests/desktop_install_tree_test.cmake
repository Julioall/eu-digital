if(NOT DEFINED BUILD_DIR OR NOT DEFINED STAGING_DIR)
    message(FATAL_ERROR "BUILD_DIR and STAGING_DIR are required")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${STAGING_DIR}"
        --component desktop
    RESULT_VARIABLE install_result
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "desktop installation failed")
endif()

foreach(required_file IN ITEMS
        "bin/eu_digital_desktop.exe"
        "bin/runtime_manifest.json"
        "bin/Qt6Core.dll"
        "bin/Qt6Gui.dll"
        "bin/Qt6Widgets.dll"
        "bin/libc++.dll"
        "bin/libunwind.dll"
        "plugins/platforms/qwindows.dll")
    if(NOT EXISTS "${STAGING_DIR}/${required_file}")
        message(FATAL_ERROR "desktop installation is missing: ${required_file}")
    endif()
endforeach()

file(READ "${STAGING_DIR}/bin/runtime_manifest.json" manifest)
string(JSON schema_version GET "${manifest}" schema_version)
string(JSON runtime_id GET "${manifest}" runtime_id)
string(JSON python_dependency GET "${manifest}" build python_runtime_dependency)
if(NOT schema_version STREQUAL "1.0" OR
   NOT runtime_id STREQUAL "eu-digital-desktop" OR
   python_dependency)
    message(FATAL_ERROR "installed desktop manifest is invalid")
endif()

file(GLOB_RECURSE installed_files RELATIVE "${STAGING_DIR}" "${STAGING_DIR}/*")
foreach(installed_file IN LISTS installed_files)
    if(installed_file MATCHES "\\.(py|pyc|ipynb)$" OR
       installed_file MATCHES "(^|/)(venv|\\.venv|datasets)(/|$)")
        message(FATAL_ERROR
            "installed desktop contains prohibited laboratory file: ${installed_file}")
    endif()
endforeach()
