include(ExternalProject)
include(FindPackageHandleStandardArgs)

macro(gtest_copy_shared_libraries _target)  
  foreach(_lib libgtest libgtest-main)
    get_target_property(_name ${_lib}    IMPORTED_LOCATION)
    #show(_name)
    add_custom_command(TARGET ${_target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy
          ${_name}
          $<TARGET_FILE_DIR:${_target}>
          )  
  endforeach()
endmacro()

if(NOT TARGET gtest)
  # DOWNLOAD AND BUILD
  ExternalProject_Add(gtest
    SVN_REPOSITORY http://googletest.googlecode.com/svn/trunk/
    INSTALL_COMMAND "" #The gtest project  doesn't support install
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
               -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
               -DBUILD_SHARED_LIBS:BOOL=TRUE
    )
endif()

get_target_property(GTEST_SRC_DIR gtest _EP_SOURCE_DIR)
get_target_property(GTEST_ROOT_DIR gtest _EP_BINARY_DIR)

if(NOT TARGET libgtest)
  add_library(libgtest IMPORTED SHARED)
  add_library(libgtest-main IMPORTED SHARED)
endif()

set_target_properties(libgtest PROPERTIES  
  IMPORTED_IMPLIB   ${GTEST_ROOT_DIR}/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}gtest${CMAKE_STATIC_LIBRARY_SUFFIX}
  IMPORTED_LOCATION ${GTEST_ROOT_DIR}/${CMAKE_CFG_INTDIR}/${CMAKE_SHARED_LIBRARY_PREFIX}gtest${CMAKE_SHARED_LIBRARY_SUFFIX}
)
set_target_properties(libgtest-main PROPERTIES  
  IMPORTED_IMPLIB   ${GTEST_ROOT_DIR}/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}gtest_main${CMAKE_STATIC_LIBRARY_SUFFIX}
  IMPORTED_LOCATION ${GTEST_ROOT_DIR}/${CMAKE_CFG_INTDIR}/${CMAKE_SHARED_LIBRARY_PREFIX}gtest_main${CMAKE_SHARED_LIBRARY_SUFFIX}
)

set(GTEST_LIBRARY libgtest)
set(GTEST_MAIN_LIBRARY libgtest-main)
set(GTEST_BOTH_LIBRARIES libgtest libgtest-main)
set(GTEST_INCLUDE_DIR ${GTEST_SRC_DIR}/include)

find_package_handle_standard_args(GTEST DEFAULT_MSG
  GTEST_BOTH_LIBRARIES
  GTEST_INCLUDE_DIR
)

### INSTALL
if(NOT TARGET install-gtest)
  add_custom_target(install-gtest DEPENDS ${GTEST_BOTH_LIBRARIES})
endif()
install(CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\" --build --target install-gtest)")
foreach(lib ${GTEST_BOTH_LIBRARIES})
  get_target_property(loc ${lib} IMPORTED_LOCATION)  
  if(MSVC)
    string(REPLACE ${CMAKE_CFG_INTDIR} Debug   loc_debug   ${loc})
    string(REPLACE ${CMAKE_CFG_INTDIR} Release loc_release ${loc})
  else()
    set(loc_debug   ${loc})
    set(loc_release ${loc})
  endif()
  install(FILES ${loc_debug}   DESTINATION bin CONFIGURATIONS Debug)
  install(FILES ${loc_release} DESTINATION bin CONFIGURATIONS Release)
endforeach()