# Would be nice if the source code was downloadable from an
# archive.  Otherwise, this requires hg to be installed.
#
# May have to look for a msvc compatible source tree elsewhere
# see: https://bitbucket.org/root_op/re2-msvc
include(ExternalProject)
include(FindPackageHandleStandardArgs)

set(FLAGS -fPIC)
if(WIN32)
set(FLAGS -DWIN32)
endif()

if(NOT TARGET re2)
  ExternalProject_Add(re2
    URL     http://dl.dropbox.com/u/782372/Software/re2.zip
    URL_MD5 02ef3dc07e72033ca14fc3d59259182b
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
               -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
               -DCMAKE_CXX_FLAGS:STRING=${FLAGS}
    )
endif()
get_target_property(RE2_SRC_DIR re2 _EP_SOURCE_DIR)
get_target_property(RE2_ROOT_DIR re2 _EP_BINARY_DIR)

add_library(libre2 IMPORTED STATIC)
add_dependencies(libre2 re2)
set_property(TARGET libre2 PROPERTY IMPORTED_LOCATION ${RE2_ROOT_DIR}/${CMAKE_CFG_INTDIR}/${CMAKE_STATIC_LIBRARY_PREFIX}re2${CMAKE_STATIC_LIBRARY_SUFFIX})

set(RE2_LIBRARY libre2)
set(RE2_INCLUDE_DIRS ${RE2_SRC_DIR})

find_package_handle_standard_args(RE2 DEFAULT_MSG
	RE2_LIBRARY
	RE2_INCLUDE_DIRS
)
