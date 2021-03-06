
## cmake build script for davix
project (davix)
cmake_minimum_required (VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules/")
option(SHARED_LIBRARY "generate shared library" TRUE)
option(STATIC_LIBRARY  "generate static library" FALSE)


# build type
if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, default to RelWithDebInfo")
    set(CMAKE_BUILD_TYPE "RelWithDebInfo")
endif(NOT CMAKE_BUILD_TYPE)

# load module
include(DefineInstallationPaths REQUIRED)
include(ReleaseDebugAutoFlags REQUIRED)
include(CMakeGeneratePkgConfig REQUIRED)
include(MacroAddDoxygen REQUIRED)
include(PortabilityGNUCheck REQUIRED)
include(CMakeCXX11Support REQUIRED)

#enable testing
INCLUDE(CTest)


set(OUTPUT_NAME_DAVIX "davix")
set(VERSION_MAJOR 0)
set(VERSION_MINOR 4)
set(VERSION_PATCH 1)
set(VERSION_STRING ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})
set(VERSION_TAG "")


# dependency mode
option(BOOST_EXTERNAL "Enable the usage of an external boost dependency" TRUE)
set (Boost_NO_BOOST_CMAKE ON)

# components
option(ENABLE_TOOLS "enable or disable tools " TRUE)

# features
option(ENABLE_IPV6 "enable or disable IPv6 support " TRUE)
option(ENABLE_TCP_NODELAY "enable or disable tcp_nodelay" TRUE)
option(ENABLE_THIRD_PARTY_COPY "enable or disable third party copy support" FALSE)

# tests
option(UNIT_TESTS "enable or disable the unit tests" FALSE)
option(FUNCTIONAL_TESTS "enable or disable the functional tests" FALSE)
option(BENCH_TESTS "enable or disable the bench tests" FALSE)

# libs checks
find_package(Threads)

# openssl
find_package(OpenSSL)
SET(LIBSSL_PKG_LIBRARIES "${OPENSSL_LIBRARIES}")
SET(CRYPTO_PKG "")
SET(LIBSSL_PKG_INCLUDE_DIRS "${OPENSSL_INCLUDE_DIR}")

find_package(LibXml2)
set(LIBXML2_INCLUDE_DIRS ${LIBXML2_INCLUDE_DIR})

if(BOOST_EXTERNAL)
find_package(Boost 1.41.0 COMPONENTS thread system)
endif(BOOST_EXTERNAL)

#const
set(HAVE_OPENSSL 1)
set(NE_HAVE_SSL 1)

add_subdirectory(deps)




## general defs
add_definitions( -D_GNU_SOURCE)              # GNU source, import LFS, etc...
add_definitions( -D__DAVIX_INSIDE__)         # protection flag
add_definitions( -D__STDC_LIMIT_MACROS )     # C99 limit macros


set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CXX11_FLAG_ENABLE}")

include_directories( ${Boost_INCLUDE_DIRS})
include_directories( ${HTTPLIB_PKG_INCLUDE_DIRS} ${GLIB2_PKG_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/deps ${CMAKE_CURRENT_SOURCE_DIR}/deps/libneon/src/ )
include_directories( ${CMAKE_SOURCE_DIR}/include/davix )
include_directories( ${CMAKE_SOURCE_DIR}/src/libs/)

add_subdirectory (src)
add_subdirectory (doc)
add_subdirectory (dist)
add_subdirectory (test)
add_subdirectory (packaging)


#############################################################################
## headers

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/davix/
                DESTINATION ${INCLUDE_INSTALL_DIR}/davix)

##############################################################################
## Doc file
install(FILES RELEASE-NOTES LICENSE
                DESTINATION ${DOC_INSTALL_DIR}/) # install release notes


#######################################################
# Configure an 'uninstall' target
CONFIGURE_FILE(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY)

ADD_CUSTOM_TARGET(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")

