# - Try to find VLC library (modernized for cmake 3.x)
#
#  VLC_FOUND - system has VLC
#  VLC_INCLUDE_DIR - The VLC include directory
#  VLC_LIBRARIES - The libraries needed to use VLC

if(VLC_INCLUDE_DIR AND VLC_LIBRARIES)
   set(VLC_FIND_QUIETLY TRUE)
endif()

if(NOT WIN32)
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(VLC_PKG QUIET libvlc>=1.0.0)
  endif()
endif()

find_path(VLC_INCLUDE_DIR
  NAMES vlc.h
  HINTS ${VLC_PKG_INCLUDE_DIRS} ${VLC_INCLUDE_DIRS}
  PATH_SUFFIXES vlc)

find_library(VLC_LIBRARIES
  NAMES vlc
  HINTS ${VLC_PKG_LIBRARY_DIRS} ${VLC_LIBRARY_DIRS})

if(VLC_INCLUDE_DIR AND VLC_LIBRARIES)
  set(VLC_FOUND TRUE)
  if(NOT VLC_FIND_QUIETLY)
    message(STATUS "Found VLC: ${VLC_LIBRARIES}")
  endif()
else()
  if(VLC_FIND_REQUIRED)
    message(FATAL_ERROR "VLC library not found")
  endif()
endif()

mark_as_advanced(VLC_INCLUDE_DIR VLC_LIBRARIES)
