cmake_minimum_required(VERSION 2.8)
project(ndio-series-plugin)

include(cmake/show.cmake)

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

function(install_debug_symbols tgt loc)
	# pdb files - msvc debugger - windows specific
	if(MSVC)
		#get_target_property(pdb ${tgt} PDB_NAME_DEBUG ) # -- doesn't work
		SET(pdb "${PROJECT_BINARY_DIR}/Debug/${tgt}.pdb")
		if(pdb)
			install(FILES ${pdb} DESTINATION ${loc} CONFIGURATIONS debug)
		endif()  
	endif()
endfunction()

function(target_add_dependencies tgt)
  target_link_libraries(${tgt} ${ND_LIBRARIES})
  target_add_tre(${tgt})
  add_dependencies(${tgt} nd)
endfunction()

################################################################################
# CONFIG
################################################################################
set(RPATH \$ORIGIN)
if(APPLE)
  set(RPATH @rpath)
endif()
set(CMAKE_INSTALL_RPATH ${RPATH})

set(NDIO_SERIES_TEST_DATA_PATH ${PROJECT_SOURCE_DIR}/test/data)
configure_file(${PROJECT_SOURCE_DIR}/config.h.in ${PROJECT_BINARY_DIR}/config.h)
include_directories(${PROJECT_BINARY_DIR})

find_package(GTEST PATHS cmake)

find_package(ND PATHS cmake)
include_directories(${ND_INCLUDE_DIRS})

find_package(tre PATHS cmake)

find_package(Threads)

##############################################################################
# SOURCE
##############################################################################

file(GLOB SRCS src/*.c src/*.cc)
file(GLOB HDRS src/*.h src/*.hh)

##############################################################################
# TARGETS
##############################################################################

add_library(ndio-series MODULE ${SRCS} ${HDRS})
target_add_dependencies(ndio-series)
#target_link_libraries(ndio-series ${ND_LIBRARIES})
#target_add_tre(ndio-series)
#add_dependencies(ndio-series nd)



##############################################################################
#  Testing
##############################################################################

file(GLOB TEST_SOURCES test/*.cc)
if(GTEST_FOUND)
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
  install(TARGETS test-ndio-series EXPORT ndio-series-targets RUNTIME DESTINATION bin/test)
  install_debug_symbols(test-ndio-series bin/test)
  gtest_copy_shared_libraries(test-ndio-series)
endif()
add_subdirectory(test2)

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

##############################################################################
#  Install
##############################################################################

foreach(_path ${NDIO_PLUGIN_PATH})
  install(TARGETS ndio-series DESTINATION ${_path})
endforeach()
install(TARGETS ndio-series DESTINATION bin/plugins)
export(PACKAGE ndio-series)
install_debug_symbols(ndio-series bin/plugins)



