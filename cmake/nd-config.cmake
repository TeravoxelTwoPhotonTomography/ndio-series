#
# LIBND
#
# TODO: Can't figure out how to install libnd and plugins to the parent projects
#       install root

# set ND_PLUGIN_PATH before find_package(ND) call to override the default

include(ExternalProject)
include(FindPackageHandleStandardArgs)

if(NOT ND_LIBRARY)
  set(ND_GIT_REPOSITORY ssh://git@bitbucket.org/nclack/nd.git CACHE STRING "Location of the git repository for libnd.")
  ExternalProject_Add(libnd
    GIT_REPOSITORY ${ND_GIT_REPOSITORY}
    GIT_TAG spin-off-plugins
    # UPDATE_COMMAND ""
    # UPDATE_COMMAND ${GIT_EXECUTABLE} pull origin master
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
               -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>#${CMAKE_INSTALL_PREFIX}
    )
  ExternalProject_Get_Property(libnd SOURCE_DIR)
  ExternalProject_Get_Property(libnd INSTALL_DIR)
  set(ND_INCLUDE_DIR ${SOURCE_DIR} CACHE PATH "Path to nd.h")

  add_library(nd STATIC IMPORTED)
  set_target_properties(nd PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    IMPORTED_LOCATION "${INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}nd${CMAKE_STATIC_LIBRARY_SUFFIX}"
    )
  add_dependencies(nd libnd)

  add_executable(libnd-tests IMPORTED)
  set_target_properties(libnd-tests PROPERTIES
    IMPORTED_LOCATION "${INSTALL_DIR}/bin/libnd-tests${CMAKE_EXECUTABLE_SUFFIX}"
    )
  add_dependencies(libnd-tests libnd)

  find_package(CUDA 4.0)

  set(ND_LIBRARY nd)
  set(ND_INCLUDE_DIRS  ${ND_INCLUDE_DIR} ${CUDA_INCLUDE_DIRS})
  set(ND_LIBRARIES     ${ND_LIBRARY} ${CUDA_LIBRARIES})
endif()

find_package_handle_standard_args(ND DEFAULT_MSG
  ND_LIBRARY
  ND_INCLUDE_DIR
)

### macro for copying plugins to a target's build Location
macro(nd_copy_plugins_to_target _target)
    #show(_target)
    add_custom_command(TARGET ${_target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${ND_ROOT_DIR}/bin/plugins
          $<TARGET_FILE_DIR:${_target}>/plugins
          COMMENT "Copying libnd plugins to build path for ${_target}."
          )  
    add_dependencies(${_target} libnd)
endmacro()
