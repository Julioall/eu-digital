if(NOT DEFINED BUILD_DIR OR NOT DEFINED STAGING_DIR)
    message(FATAL_ERROR "BUILD_DIR and STAGING_DIR are required")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --prefix "${STAGING_DIR}"
        --component runtime
    RESULT_VARIABLE install_result
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "runtime installation failed")
endif()

file(GLOB_RECURSE installed_files RELATIVE "${STAGING_DIR}" "${STAGING_DIR}/*")
foreach(installed_file IN LISTS installed_files)
    if(IS_DIRECTORY "${STAGING_DIR}/${installed_file}")
        continue()
    endif()
    if(installed_file MATCHES "\\.(py|pyc|ipynb)$" OR installed_file MATCHES "(^|/)(venv|\\.venv|datasets)(/|$)")
        message(FATAL_ERROR "installed runtime contains prohibited non-native file: ${installed_file}")
    endif()
    if(NOT installed_file MATCHES "^bin/eu_digital_runtime(\\.exe)?$")
        message(FATAL_ERROR "installed runtime contains unexpected file: ${installed_file}")
    endif()
endforeach()
