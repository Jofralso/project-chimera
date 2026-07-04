find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
  pkg_check_modules(PC_JACK QUIET jack)
endif()

find_path(JACK_INCLUDE_DIR
  NAMES jack/jack.h
  HINTS ${PC_JACK_INCLUDEDIR} ${PC_JACK_INCLUDE_DIRS}
  PATH_SUFFIXES jack
)

find_library(JACK_LIBRARY
  NAMES jack
  HINTS ${PC_JACK_LIBDIR} ${PC_JACK_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Jack
  REQUIRED_VARS JACK_LIBRARY JACK_INCLUDE_DIR
)

if(JACK_FOUND AND NOT TARGET Jack::jack)
  add_library(Jack::jack UNKNOWN IMPORTED)
  set_target_properties(Jack::jack PROPERTIES
    IMPORTED_LOCATION "${JACK_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${JACK_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(JACK_INCLUDE_DIR JACK_LIBRARY)
