cmake_minimum_required(VERSION 2.8)
project(ndio-series-plugin)

macro(show v)
  message("${v} is ${${v}}")
endmacro()

################################################################################
# CONFIG
################################################################################
set(NDIO_SERIES_TEST_DATA_PATH ${PROJECT_SOURCE_DIR}/test/data)
configure_file(${PROJECT_SOURCE_DIR}/config.h.in ${PROJECT_BINARY_DIR}/config.h)
include_directories(${PROJECT_BINARY_DIR})

find_package(GTEST CONFIG PATHS cmake)

find_package(ND CONFIG PATHS cmake)
include_directories(${ND_INCLUDE_DIRS})

find_package(RE2 CONFIG PATHS cmake)
include_directories(${RE2_INCLUDE_DIRS})

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
target_link_libraries(ndio-series libre2 ${ND_LIBRARIES})
add_dependencies(ndio-series nd)

##############################################################################
#  Testing
##############################################################################

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
    ${ND_LIBRARIES}
    )
  add_dependencies(test-ndio-series gtest nd)
  gtest_copy_shared_libraries(test-ndio-series)
  add_test(TestNDIOSeries test-ndio-series)
  install(TARGETS test-ndio-series RUNTIME DESTINATION bin)
  gtest_copy_shared_libraries(test-ndio-series)
  install(FILES ${GTEST_BOTH_LIBRARIES} DESTINATION bin)
endif()

##############################################################################
#  Install
##############################################################################

foreach(_path ${NDIO_PLUGIN_PATH})
  install(TARGETS ndio-series LIBRARY DESTINATION ${_path})
endforeach()
install(TARGETS ndio-series LIBRARY DESTINATION bin/plugins)

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


