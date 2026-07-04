include(${CMAKE_SOURCE_DIR}/cmake/ChimeraConfig.cmake)

if(NOT TARGET chimera_audio_engine)
    add_subdirectory(${CMAKE_SOURCE_DIR}/software/audio-engine ${CMAKE_BINARY_DIR}/software/audio-engine)
endif()

add_library(chimera_ui STATIC
  src/display.cpp
  src/screen.cpp
  src/midi_handler.cpp
  src/screens/synth_screen.cpp
  src/screens/drum_screen.cpp
  src/screens/sequencer_screen.cpp
  src/screens/mixer_screen.cpp
  src/screens/browser_screen.cpp
)

target_include_directories(chimera_ui PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/chimera/ui>
)

chimera_set_realtime_flags(chimera_ui)
chimera_set_jetson_flags(chimera_ui)
chimera_no_rtti(chimera_ui)

# SDL2 - search in extracted dev package first, then system
if(NOT CHIMERA_JETSON)
  set(SDL2_INCLUDE_DIR ${CMAKE_BINARY_DIR}/sdl2-dev/usr/include)
  if(NOT EXISTS ${SDL2_INCLUDE_DIR}/SDL2/SDL.h)
    find_path(SDL2_INCLUDE_DIR SDL.h PATH_SUFFIXES SDL2)
  endif()

  set(SDL2_ARCH_INCLUDE ${CMAKE_BINARY_DIR}/sdl2-dev/usr/include/x86_64-linux-gnu)

  if(EXISTS ${CMAKE_BINARY_DIR}/libSDL2.so)
    set(SDL2_LIBRARY ${CMAKE_BINARY_DIR}/libSDL2.so)
  else()
    find_library(SDL2_LIBRARY NAMES SDL2-2.0 SDL2
      HINTS /usr/lib/x86_64-linux-gnu /usr/local/lib /usr/lib)
  endif()

  if(NOT SDL2_LIBRARY)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(PC_SDL2 QUIET sdl2)
      if(PC_SDL2_FOUND)
        set(SDL2_LIBRARY ${PC_SDL2_LINK_LIBRARIES})
        set(SDL2_INCLUDE_DIR ${PC_SDL2_INCLUDE_DIRS})
      endif()
    endif()
  endif()
else()
  find_path(SDL2_INCLUDE_DIR SDL2/SDL.h
    HINTS /usr/include /usr/local/include
    PATH_SUFFIXES SDL2)
  find_library(SDL2_LIBRARY NAMES SDL2
    HINTS /usr/lib/aarch64-linux-gnu /usr/local/lib /usr/lib)
endif()

if(SDL2_LIBRARY AND SDL2_INCLUDE_DIR)
  message(STATUS "UI: SDL2 at ${SDL2_LIBRARY}")
  target_link_libraries(chimera_ui PUBLIC ${SDL2_LIBRARY} m dl pthread)
  target_include_directories(chimera_ui PUBLIC ${SDL2_INCLUDE_DIR} ${SDL2_INCLUDE_DIR}/SDL2)
  if(NOT CHIMERA_JETSON)
    target_include_directories(chimera_ui PRIVATE ${SDL2_ARCH_INCLUDE})
  endif()
else()
  message(WARNING "UI: SDL2 not found")
endif()

# ALSA (for MIDI)
find_path(ALSA_INCLUDE_DIR alsa/asoundlib.h
  HINTS
    ${CMAKE_BINARY_DIR}/deps/alsa/usr/include
    /usr/include
)
if(ALSA_INCLUDE_DIR)
  target_include_directories(chimera_ui PRIVATE ${ALSA_INCLUDE_DIR})
endif()

find_library(DL_LIB dl)
if(DL_LIB)
  target_link_libraries(chimera_ui PRIVATE ${DL_LIB})
endif()

find_library(ASOUND_LIB asound)
if(ASOUND_LIB)
  target_link_libraries(chimera_ui PRIVATE ${ASOUND_LIB})
endif()

target_link_libraries(chimera_ui PRIVATE chimera_audio_engine)
