# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

#[=======================================================================[.rst:
FindOpenCL
----------

.. versionadded:: 3.1

Finds Open Computing Language (OpenCL)

.. versionadded:: 3.10
  Detection of OpenCL 2.1 and 2.2.

IMPORTED Targets
^^^^^^^^^^^^^^^^

.. versionadded:: 3.7

This module defines :prop_tgt:`IMPORTED` target ``OpenCL::OpenCL``, if
OpenCL has been found.

.. versionadded:: 3.27

This module supports detecting various parts of common OpenCL SDKs as
`COMPONENTS`. Some components may only ship with the Khronos hosted SDK.
The following components define :prop_tgt:`IMPORTED` targets with
matching name in the `OpenCL::` namespace::

  OpenCL     - the ICD loader (depends on Headers)
  Headers    - the canonical C API headers
  HeadersCpp - the canonical C++ wrapper
  Utils      - Khronos-hosted utility lib for C
  UtilsCpp   - Khronos-hosted utility lib for C++

Result Variables
^^^^^^^^^^^^^^^^

This module defines the following variables::

  OpenCL_FOUND          - True if OpenCL was found
  OpenCL_INCLUDE_DIRS   - include directories for OpenCL
  OpenCL_LIBRARIES      - link against this library to use OpenCL
  OpenCL_VERSION_STRING - Highest supported OpenCL version (eg. 1.2)
  OpenCL_VERSION_MAJOR  - The major version of the OpenCL implementation
  OpenCL_VERSION_MINOR  - The minor version of the OpenCL implementation

The module will also define two cache variables::

  OpenCL_INCLUDE_DIR    - the OpenCL include directory
  OpenCL_LIBRARY        - the path to the OpenCL library

#]=======================================================================]

# Deduplicate PATHS and PATH_SUFFIXES
set(_OPENCL_x86 "(x86)")
set(_VENDOR_SDK_ENVS
  ENV AMDAPPSDKROOT
  ENV INTELOCLSDKROOT
  ENV NVSDKCOMPUTE_ROOT
  ENV CUDA_PATH
  ENV ATISTREAMSDKROOT
  ENV OCL_ROOT)
set(_LINUX_VENDOR_SDK_PATHS
  /usr/local/cuda
  /opt/cuda)
set(_INCLUDE_PATH_SUFFIXES
  include
  OpenCL/common/inc
  "AMD APP/include")
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  set(_LINUX_LIB_PATH_SUFFIXES
    lib/x86
    lib)
  set(_WIN_LIB_PATH_SUFFIXES
    "AMD APP/lib/x86"
    lib/x86
    lib/Win32
    OpenCL/common/lib/Win32)
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(_LINUX_LIB_PATH_SUFFIXES
    lib/x86_64
    lib/x64
    lib
    lib64)
  set(_WIN_LIB_PATH_SUFFIXES
    "AMD APP/lib/x86_64"
    lib/x86_64
    lib/x64
    lib
    OpenCL/common/lib/x64)
else()
  message(DEBUG "Unexpected pointer width. No lib suffixes set.")
endif()

function(_FIND_OPENCL_VERSION)
  include(CheckSymbolExists)
  include(CMakePushCheckState)
  set(CMAKE_REQUIRED_QUIET ${OpenCL_FIND_QUIETLY})

  CMAKE_PUSH_CHECK_STATE()

  foreach(VERSION "3_0" "2_2" "2_1" "2_0" "1_2" "1_1" "1_0")
    set(CMAKE_REQUIRED_INCLUDES "${OpenCL_INCLUDE_DIR}")

    if(EXISTS ${OpenCL_INCLUDE_DIR}/Headers/cl.h)
      CHECK_SYMBOL_EXISTS(
        CL_VERSION_${VERSION}
        "${OpenCL_INCLUDE_DIR}/Headers/cl.h"
        OPENCL_VERSION_${VERSION})
    else()
      CHECK_SYMBOL_EXISTS(
        CL_VERSION_${VERSION}
        "${OpenCL_INCLUDE_DIR}/CL/cl.h"
        OPENCL_VERSION_${VERSION})
    endif()

    if(OPENCL_VERSION_${VERSION})
      string(REPLACE "_" "." VERSION "${VERSION}")
      set(OpenCL_VERSION_STRING ${VERSION} PARENT_SCOPE)
      string(REGEX MATCHALL "[0-9]+" version_components "${VERSION}")
      list(GET version_components 0 major_version)
      list(GET version_components 1 minor_version)
      set(OpenCL_VERSION_MAJOR ${major_version} PARENT_SCOPE)
      set(OpenCL_VERSION_MINOR ${minor_version} PARENT_SCOPE)
      break()
    endif()
  endforeach()

  CMAKE_POP_CHECK_STATE()
endfunction()

function(_GET_PROGRAMFILES_PATHS INSTALL_FOLDER OUT_VAR)
  set(${OUT_VAR}
    ENV "PROGRAMFILES(X86)"
    ENV "PROGRAMFILES"
    $ENV{PROGRAMFILES${_OPENCL_x86}}/${INSTALL_FOLDER}
    $ENV{PROGRAMFILES}/${INSTALL_FOLDER}
    PARENT_SCOPE
  )
endfunction()

_GET_PROGRAMFILES_PATHS(OpenCLHeaders _OPENCLHEADERS_PROGRAMFILES)
_GET_PROGRAMFILES_PATHS(OpenCLHeadersCpp _OPENCLHEADERSCPP_PROGRAMFILES)
_GET_PROGRAMFILES_PATHS(OpenCL-ICD-Loader _OPENCLICDLOADER_PROGRAMFILES)
_GET_PROGRAMFILES_PATHS(OpenCL-SDK _OPENCLSDK_PROGRAMFILES)

# Find C API header
find_path(OpenCL_INCLUDE_DIR
  NAMES
    CL/cl.h
    OpenCL/cl.h
  PATHS
    ${_OPENCLHEADERS_PROGRAMFILES}
    ${_VENDOR_SDK_ENVS}
    ${_LINUX_VENDOR_SDK_PATHS}
  PATH_SUFFIXES
    ${_INCLUDE_PATH_SUFFIXES})

# Check version of header
_FIND_OPENCL_VERSION()

# Find C++ API header and utility headers
find_path(OpenCL_HeadersCpp_INCLUDE_DIR
  NAMES CL/opencl.hpp
  PATHS
    ${_OPENCLHEADERSCPP_PROGRAMFILES}
    ${_VENDOR_SDK_ENVS}
    ${_LINUX_VENDOR_SDK_PATHS}
  PATH_SUFFIXES
    ${_INCLUDE_PATH_SUFFIXES})

find_path(OpenCL_Utils_INCLUDE_DIR
  NAMES CL/Utils/Utils.h
  PATHS
    ${_OPENCLSDK_PROGRAMFILES}
    ${_VENDOR_SDK_ENVS}
    ${_LINUX_VENDOR_SDK_PATHS}
  PATH_SUFFIXES
    ${_INCLUDE_PATH_SUFFIXES})

find_path(OpenCL_UtilsCpp_INCLUDE_DIR
  NAMES CL/Utils/Utils.hpp
  PATHS
    ${_OPENCLSDK_PROGRAMFILES}
    ${_VENDOR_SDK_ENVS}
    ${_LINUX_VENDOR_SDK_PATHS}
  PATH_SUFFIXES
    ${_INCLUDE_PATH_SUFFIXES})

# Find libraries
if(WIN32)
  find_library(OpenCL_LIBRARY
    NAMES OpenCL
    PATHS
      ${_OPENCLICDLOADER_PROGRAMFILES}
      ${_VENDOR_SDK_ENVS}
    PATH_SUFFIXES
      ${_WIN_LIB_PATH_SUFFIXES})

  foreach(UtilLibName Utils UtilsCpp)
    find_library(OpenCL_${UtilLibName}_LIBRARY
      NAMES OpenCL${UtilLibName}
      PATHS
        ${_OPENCLSDK_PROGRAMFILES}
        ${_VENDOR_SDK_ENVS}
      PATH_SUFFIXES
        ${_WIN_LIB_PATH_SUFFIXES})
    find_library(OpenCL_${UtilLibName}_DEBUG_LIBRARY
      NAMES OpenCL${UtilLibName}d
      PATHS
        ${_OPENCLSDK_PROGRAMFILES}
        ${_VENDOR_SDK_ENVS}
      PATH_SUFFIXES
        ${_WIN_LIB_PATH_SUFFIXES})
  endforeach()
else()
  find_library(OpenCL_LIBRARY
    NAMES OpenCL
    PATHS
      ${_VENDOR_SDK_ENVS}
      ${_LINUX_VENDOR_SDK_PATHS}
    PATH_SUFFIXES
      ${_LINUX_LIB_PATH_SUFFIXES})

  foreach(UtilLibName Utils UtilsCpp)
    find_library(OpenCL_${UtilLibName}_LIBRARY
      NAMES OpenCL${UtilLibName}
      PATHS
        ${_VENDOR_SDK_ENVS}
        ${_LINUX_VENDOR_SDK_PATHS}
      PATH_SUFFIXES
        ${_LINUX_LIB_PATH_SUFFIXES})
    find_library(OpenCL_${UtilLibName}_DEBUG_LIBRARY
      NAMES OpenCL${UtilLibName}d
      PATHS
        ${_VENDOR_SDK_ENVS}
        ${_LINUX_VENDOR_SDK_PATHS}
      PATH_SUFFIXES
        ${_LINUX_LIB_PATH_SUFFIXES})
  endforeach()
endif()

unset(_OPENCL_x86)

# If user specified components, both restrict to searching for only those
# components and enforce finding all of them.
if(OpenCL_FIND_COMPONENTS)
  foreach(component ${OpenCL_FIND_COMPONENTS})
    if(component STREQUAL "Headers")
      set(OPENCL_USE_HEADERS 1)
    elseif(component STREQUAL "HeadersCpp")
      set(OPENCL_USE_HEADERSCPP 1)
      set(OPENCL_USE_HEADERS 1)
    elseif(component STREQUAL "OpenCL")
      set(OPENCL_USE_HEADERS 1)
      set(OPENCL_USE_ICD_LOADER 1)
    elseif(component STREQUAL "Utils")
      set(OPENCL_USE_UTILS 1)
      set(OPENCL_USE_HEADERS 1)
    elseif(component STREQUAL "UtilsCpp")
      set(OPENCL_USE_UTILSCPP 1)
      set(OPENCL_USE_UTILS 1)
      set(OPENCL_USE_HEADERSCPP 1)
      set(OPENCL_USE_HEADERS 1)
    endif()
  endforeach()

  if(OPENCL_USE_HEADERS)
    list(APPEND _OpenCL_REQUIRED_VARS OpenCL_INCLUDE_DIR)
    list(APPEND _OpenCL_ADVANCED_VARS OpenCL_INCLUDE_DIR)
    list(APPEND OpenCL_INCLUDE_DIRS "${OpenCL_INCLUDE_DIR}")
  endif()

  if(OPENCL_USE_HEADERSCPP)
    list(APPEND _OpenCL_REQUIRED_VARS OpenCL_HeadersCpp_INCLUDE_DIR)
    list(APPEND _OpenCL_ADVANCED_VARS OpenCL_HeadersCpp_INCLUDE_DIR)
    list(APPEND OpenCL_INCLUDE_DIRS "${OpenCL_HeadersCpp_INCLUDE_DIR}")
  endif()

  if(OPENCL_USE_ICD_LOADER)
    list(APPEND _OpenCL_REQUIRED_VARS OpenCL_LIBRARY)
    list(APPEND _OpenCL_ADVANCED_VARS OpenCL_LIBRARY)
    list(APPEND OpenCL_LIBRARIES "${OpenCL_LIBRARY}")
  endif()

  if(OPENCL_USE_UTILS)
    list(APPEND _OpenCL_REQUIRED_VARS OpenCL_Utils_INCLUDE_DIR)
    list(APPEND _OpenCL_ADVANCED_VARS OpenCL_Utils_INCLUDE_DIR)
    list(APPEND OpenCL_INCLUDE_DIRS "${OpenCL_Utils_INCLUDE_DIR}")
    if(WIN32)
      list(APPEND _OpenCL_REQUIRED_VARS OpenCL_Utils_DEBUG_LIBRARY)
      list(APPEND _OpenCL_ADVANCED_VARS OpenCL_Utils_DEBUG_LIBRARY)
      list(APPEND OpenCL_LIBRARIES_DEBUG "${OpenCL_Utils_DEBUG_LIBRARY}")
    endif()
    list(APPEND _OpenCL_REQUIRED_VARS OpenCL_Utils_LIBRARY)
    list(APPEND _OpenCL_ADVANCED_VARS OpenCL_Utils_LIBRARY)
    list(APPEND OpenCL_LIBRARIES "${OpenCL_Utils_LIBRARY}")
    
  endif()

  if(OPENCL_USE_UTILSCPP)
    list(APPEND _OpenCL_REQUIRED_VARS OpenCL_UtilsCpp_INCLUDE_DIR)
    list(APPEND _OpenCL_ADVANCED_VARS OpenCL_UtilsCpp_INCLUDE_DIR)
    list(APPEND OpenCL_INCLUDE_DIRS "${OpenCL_UtilsCpp_INCLUDE_DIR}")
    if(WIN32)
      list(APPEND _OpenCL_REQUIRED_VARS OpenCL_UtilsCpp_DEBUG_LIBRARY)
      list(APPEND _OpenCL_ADVANCED_VARS OpenCL_UtilsCpp_DEBUG_LIBRARY)
      list(APPEND OpenCL_LIBRARIES_DEBUG "${OpenCL_UtilsCpp_DEBUG_LIBRARY}")
    endif()
    list(APPEND _OpenCL_REQUIRED_VARS OpenCL_UtilsCpp_LIBRARY)
    list(APPEND _OpenCL_ADVANCED_VARS OpenCL_UtilsCpp_LIBRARY)
    list(APPEND OpenCL_LIBRARIES "${OpenCL_UtilsCpp_LIBRARY}")
    
  endif()
# If the user did not specify components explicitly, in the spirit of backwards compat,
# find Headers and ICD-Loader minimally to succeed, but add HeadersCpp, Utils and
# UtilsCpp to the result variables if present.
else()
  list(APPEND _OpenCL_REQUIRED_VARS OpenCL_INCLUDE_DIR OpenCL_LIBRARY)
  set(OpenCL_LIBRARIES "${OpenCL_LIBRARY}")
  set(OpenCL_INCLUDE_DIRS "${OpenCL_INCLUDE_DIR}")
  list(APPEND _OpenCL_ADVANCED_VARS OpenCL_LIBRARY OpenCL_INCLUDE_DIR)
  if(OpenCL_HeadersCpp_INCLUDE_DIR)
    list(APPEND OpenCL_INCLUDE_DIRS "${OpenCL_HeadersCpp_INCLUDE_DIR}")
    list(APPEND _OpenCL_ADVANCED_VARS OpenCL_HeadersCpp_INCLUDE_DIR)
  endif()
  if(OpenCL_Utils_INCLUDE_DIR)
    list(APPEND OpenCL_INCLUDE_DIRS "${OpenCL_Utils_INCLUDE_DIR}")
    list(APPEND _OpenCL_ADVANCED_VARS OpenCL_Utils_INCLUDE_DIR)
  endif()
  if(OpenCL_Utils_LIBRARY)
    list(APPEND OpenCL_LIBRARIES "${OpenCL_Utils_LIBRARY}")
    list(APPEND _OpenCL_ADVANCED_VARS OpenCL_Utils_LIBRARY)
  endif()
  if(OpenCL_Utils_DEBUG_LIBRARY)
    list(APPEND OpenCL_LIBRARIES_DEBUG "${OpenCL_Utils_DEBUG_LIBRARY}")
    list(APPEND _OpenCL_ADVANCED_VARS OpenCL_Utils_DEBUG_LIBRARY)
  endif()
  if(OpenCL_UtilsCpp_INCLUDE_DIR)
    list(APPEND OpenCL_INCLUDE_DIRS "${OpenCL_UtilsCpp_INCLUDE_DIR}")
    list(APPEND _OpenCL_ADVANCED_VARS OpenCL_UtilsCpp_INCLUDE_DIR)
  endif()
  if(OpenCL_UtilsCpp_LIBRARY)
    list(APPEND OpenCL_LIBRARIES "${OpenCL_UtilsCpp_LIBRARY}")
    list(APPEND _OpenCL_ADVANCED_VARS OpenCL_UtilsCpp_LIBRARY)
  endif()
  if(OpenCL_UtilsCpp_DEBUG_LIBRARY)
    list(APPEND OpenCL_LIBRARIES_DEBUG "${OpenCL_UtilsCpp_DEBUG_LIBRARY}")
    list(APPEND _OpenCL_ADVANCED_VARS OpenCL_UtilsCpp_DEBUG_LIBRARY)
  endif()
endif()

# Decide whether find_package succeeds or fails
include(${CMAKE_CURRENT_LIST_DIR}/FindPackageHandleStandardArgs.cmake)
find_package_handle_standard_args(
  OpenCL
  FOUND_VAR OpenCL_FOUND
  REQUIRED_VARS ${_OpenCL_REQUIRED_VARS}
  VERSION_VAR OpenCL_VERSION_STRING)

mark_as_advanced(${_OpenCL_ADVANCED_VARS})

# Create IMPORTED targets upon success (and guard against multiple invocations)
if(OpenCL_FOUND AND NOT TARGET OpenCL::Headers)
  add_library(OpenCL::Headers INTERFACE IMPORTED)
  set_target_properties(OpenCL::Headers PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${OpenCL_INCLUDE_DIR}")
endif()

if(OpenCL_FOUND AND NOT TARGET OpenCL::HeadersCpp)
  add_library(OpenCL::HeadersCpp INTERFACE IMPORTED)
  set_target_properties(OpenCL::HeadersCpp PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES
      "${OpenCL_INCLUDE_DIR};${OpenCL_HeadersCpp_INCLUDE_DIR}")
endif()

if(OpenCL_FOUND AND NOT TARGET OpenCL::OpenCL)
  if(OpenCL_LIBRARY MATCHES "/([^/]+)\\.framework$")
    add_library(OpenCL::OpenCL INTERFACE IMPORTED)
    set_target_properties(OpenCL::OpenCL PROPERTIES
      INTERFACE_LINK_LIBRARIES "${OpenCL_LIBRARY}")
  else()
    add_library(OpenCL::OpenCL UNKNOWN IMPORTED)
    set_target_properties(OpenCL::OpenCL PROPERTIES
      IMPORTED_LOCATION "${OpenCL_LIBRARY}")
  endif()

  set_target_properties(OpenCL::OpenCL PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${OpenCL_INCLUDE_DIR}")
endif()

if(OpenCL_FOUND AND NOT TARGET OpenCL::Utils)
  add_library(OpenCL::Utils UNKNOWN IMPORTED)
  set_target_properties(OpenCL::Utils PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES
      "${OpenCL_INCLUDE_DIR};${OpenCL_Utils_INCLUDE_DIR}")
  if(OpenCL_Utils_LIBRARY)
  set_property(TARGET OpenCL::Utils APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
  set_target_properties(OpenCL::Utils PROPERTIES
    IMPORTED_LINK_INTERFACE_LANGUAGES "C"
    IMPORTED_LOCATION "${OpenCL_Utils_LIBRARY}")
  endif()
  if(OpenCL_Utils_DEBUG_LIBRARY)
    set_property(TARGET OpenCL::Utils APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
    set_target_properties(OpenCL::Utils PROPERTIES
      IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
      IMPORTED_LOCATION_DEBUG "${OpenCL_Utils_DEBUG_LIBRARY}")
  endif()
endif()

if(OpenCL_FOUND AND NOT TARGET OpenCL::UtilsCpp)
  add_library(OpenCL::UtilsCpp UNKNOWN IMPORTED)
  set_target_properties(OpenCL::UtilsCpp PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES
      "${OpenCL_INCLUDE_DIR};${OpenCL_Utils_INCLUDE_DIR};${OpenCL_UtilsCpp_INCLUDE_DIR}")
  if(OpenCL_UtilsCpp_LIBRARY)
    set_property(TARGET OpenCL::UtilsCpp APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
    set_target_properties(OpenCL::UtilsCpp PROPERTIES
      IMPORTED_LINK_INTERFACE_LANGUAGES "CXX"
      IMPORTED_LOCATION_RELEASE "${OpenCL_UtilsCpp_LIBRARY}")
  endif()
  if(OpenCL_UtilsCpp_DEBUG_LIBRARY)
    set_property(TARGET OpenCL::UtilsCpp APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
    set_target_properties(OpenCL::UtilsCpp PROPERTIES
      IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "CXX"
      IMPORTED_LOCATION_DEBUG "${OpenCL_UtilsCpp_DEBUG_LIBRARY}")
  endif()
endif()
