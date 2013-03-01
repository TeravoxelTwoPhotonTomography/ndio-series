cmake_minimum_required(VERSION 2.8)
project(ndio-series-plugin)

#set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR}/install CACHE PATH "Install path prefix, prepended onto install directories." FORCE)

set(CMAKE_MODULE_PATH  ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

macro(show v)
  message("${v} is ${${v}}")
endmacro()

################################################################################
# CONFIG
################################################################################
set(NDIO_SERIES_TEST_DATA_PATH ${PROJECT_SOURCE_DIR}/test/data)
configure_file(${PROJECT_SOURCE_DIR}/config.h.in ${PROJECT_BINARY_DIR}/config.h)
include_directories(${PROJECT_BINARY_DIR})

find_package(RE2)
if(RE2_FOUND)
  include_directories(${RE2_INCLUDE_DIRS})
  get_directory_property(ND_SOURCE_DIR PARENT_DIRECTORY)
  include_directories(${ND_SOURCE_DIR})

  ##############################################################################
  # SOURCE
  ##############################################################################

  file(GLOB SRCS src/*.c src/*.cc)
  file(GLOB HDRS src/*.h src/*.hh)

  ##############################################################################
  # TARGETS
  ##############################################################################

  add_definitions(-fPIC)
  add_library(ndio-series MODULE ${SRCS} ${HDRS})
  target_link_libraries(ndio-series nd libre2)

  ##############################################################################
  #  Testing
  ##############################################################################

  find_package(GTEST CONFIG PATHS cmake)
  file(GLOB TEST_SOURCES test/*.cc)
  if(GTEST_FOUND)
    find_package(Threads)
    enable_testing()
    include_directories(${PROJECT_SOURCE_DIR})
    include_directories(${GTEST_INCLUDE_DIR})
    add_executable(test-ndio-series ${TEST_SOURCES} config.h)
    target_link_libraries(test-ndio-series
      ${GTEST_BOTH_LIBRARIES}
      ${CMAKE_THREAD_LIBS_INIT}
      nd
      )
    add_dependencies(test-ndio-series gtest)
    gtest_copy_shared_libraries(test-ndio-series)
    add_test(TestNDIOSeries test-ndio-series)
    install(TARGETS test-ndio-series RUNTIME DESTINATION bin)
    gtest_copy_shared_libraries(test-ndio-series)
  endif()

  ##############################################################################
  #  Install
  ##############################################################################
  install(TARGETS ndio-series
    EXPORT ndio-series-targets
    LIBRARY DESTINATION bin/${NDIO_PLUGIN_PATH}
  )
  export(TARGETS ndio-series FILE ndio-series-targets.cmake)
  install(EXPORT ndio-series-targets DESTINATION cmake)

endif(RE2_FOUND)

################################################################################
#  Documentation
################################################################################

find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile.in
                 ${CMAKE_CURRENT_BINARY_DIR}/doc/Doxyfile
                 @ONLY
                 )
  add_custom_target(ndio-series-docs
    ${DOXYGEN_EXECUTABLE} ${PROJECT_BINARY_DIR}/doc/Doxyfile
    WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
    COMMENT           "Generating API Docs"
    VERBATIM
    )
endif()


