find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
  pkg_check_modules(PC_ALSA QUIET alsa)
endif()

find_path(ALSA_INCLUDE_DIR
  NAMES alsa/asoundlib.h
  HINTS
    ${PC_ALSA_INCLUDEDIR}
    ${PC_ALSA_INCLUDE_DIRS}
    ${CMAKE_BINARY_DIR}/deps/alsa/usr/include
  NO_DEFAULT_PATH
)

find_path(ALSA_INCLUDE_DIR
  NAMES alsa/asoundlib.h
  HINTS
    ${PC_ALSA_INCLUDEDIR}
    ${PC_ALSA_INCLUDE_DIRS}
    ${CMAKE_BINARY_DIR}/deps/alsa/usr/include
)

find_library(ALSA_LIBRARY
  NAMES asound
  HINTS
    ${PC_ALSA_LIBDIR}
    ${PC_ALSA_LIBRARY_DIRS}
    ${CMAKE_BINARY_DIR}/deps/alsa/usr/lib/x86_64-linux-gnu
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ALSA
  REQUIRED_VARS ALSA_LIBRARY ALSA_INCLUDE_DIR
)

if(ALSA_FOUND AND NOT TARGET ALSA::ALSA)
  add_library(ALSA::ALSA UNKNOWN IMPORTED)
  set_target_properties(ALSA::ALSA PROPERTIES
    IMPORTED_LOCATION "${ALSA_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${ALSA_INCLUDE_DIR}"
  )
  message(STATUS "ALSA found: ${ALSA_INCLUDE_DIR}")
endif()

mark_as_advanced(ALSA_INCLUDE_DIR ALSA_LIBRARY)
