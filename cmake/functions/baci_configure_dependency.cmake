# A function to search for and configure an external dependency
function(baci_configure_dependency _package_name)
  set(options "")
  set(oneValueArgs DEFAULT)
  set(multiValueArgs "")
  cmake_parse_arguments(
    _parsed
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
    ${ARGN}
    )

  if(DEFINED _parsed_UNPARSED_ARGUMENTS)
    message(FATAL_ERROR "There are unparsed arguments: ${_parsed_UNPARSED_ARGUMENTS}")
  endif()

  string(TOUPPER ${_package_name} _package_name_UPPER)

  # Add a cache entry to turn the option ON or OFF.
  baci_process_global_option(
    BACI_WITH_${_package_name_UPPER} "Build BACI with ${_package_name}" ${_parsed_DEFAULT}
    )

  if(BACI_WITH_${_package_name_UPPER})
    if(${_package_name}_ROOT)
      message(
        WARNING
          "A variable '${_package_name}_ROOT' is set. Prefer setting it via 'BACI_${_package_name_UPPER}_ROOT'."
        )
    endif()

    if(BACI_${_package_name_UPPER}_ROOT)
      # Translate the ROOT variable into the case style that fits to the package name.
      # This variable is automatically understood by CMake's find_XXX functions.
      set(${_package_name}_ROOT ${BACI_${_package_name_UPPER}_ROOT})
    else()
      message(
        STATUS
          "No variable 'BACI_${_package_name_UPPER}_ROOT' set. Trying to find ${_package_name} in default locations."
        )
    endif()

    # Hand over the actual configuration to a separate script.
    include(cmake/configure/configure_${_package_name}.cmake)
  endif()

  # Store the flag that was generated from the package name
  set_property(
    GLOBAL APPEND PROPERTY BACI_FLAGS_EXTERNAL_DEPENDENCIES BACI_WITH_${_package_name_UPPER}
    )

  message(STATUS "Processed dependency ${_package_name}.\n")
endfunction()
