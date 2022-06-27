# Fetch GoogleTest and setup the unit tests if option is enabled
option(BACI_WITH_GOOGLETEST "Use GoogleTest for unit testing" ON)
if(BACI_WITH_GOOGLETEST)
  message(STATUS "Unit tests with GoogleTest: enabled")

  # Define a convenience target for all unit tests and add it to 'full'
  # All unit test executables should add themselves as a dependency to 'unittests'
  add_custom_target(unittests)
  add_dependencies(full unittests)

  if(TARGET gtest)
    message(
      FATAL_ERROR "A target <gtest> has already been included by a TPL." "This is not supported."
      )
  endif()

  include(FetchContent)
  fetchcontent_declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.11.0
    )
  fetchcontent_makeavailable(googletest)

  add_subdirectory(unittests)
else()
  message(STATUS "Unit tests with GoogleTest: disabled")
endif()
