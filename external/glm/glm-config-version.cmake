set(PACKAGE_VERSION "0.9.9")

if(PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION)
  set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()

  if("0.9.9" MATCHES "^([0-9]+)\\.")
    set(CVF_VERSION_MAJOR "${CMAKE_MATCH_1}")
  else()
    set(CVF_VERSION_MAJOR "0.9.9")
  endif()

  if(PACKAGE_FIND_VERSION_MAJOR STREQUAL CVF_VERSION_MAJOR)
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
  else()
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
  endif()

  if(PACKAGE_FIND_VERSION STREQUAL PACKAGE_VERSION)
      set(PACKAGE_VERSION_EXACT TRUE)
  endif()
endif()


# if the installed or the using project don't have CMAKE_SIZEOF_VOID_P set, ignore it:
if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "" OR "" STREQUAL "")
   return()
endif()

# check that the installed version has the same 32/64bit-ness as the one which is currently searching:
if(NOT CMAKE_SIZEOF_VOID_P STREQUAL "")
  math(EXPR installedBits " * 8")
  set(PACKAGE_VERSION "${PACKAGE_VERSION} (${installedBits}bit)")
  set(PACKAGE_VERSION_UNSUITABLE TRUE)
endif()
