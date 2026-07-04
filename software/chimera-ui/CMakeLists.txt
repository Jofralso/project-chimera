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
chimera_no_rtti(chimera_ui)

# SDL2 - search in extracted dev package first, then system
# Find SDL2 headers from extracted dev package
set(SDL2_INCLUDE_DIR ${CMAKE_BINARY_DIR}/sdl2-dev/usr/include)
if(NOT EXISTS ${SDL2_INCLUDE_DIR}/SDL2/SDL.h)
  find_path(SDL2_INCLUDE_DIR SDL.h PATH_SUFFIXES SDL2)
endif()

# Add SDL2 arch-specific headers (Debian)
set(SDL2_ARCH_INCLUDE ${CMAKE_BINARY_DIR}/sdl2-dev/usr/include/x86_64-linux-gnu)

# SDL2 library - prefer runtime .so.0 via symlink, then system, then dev package
if(EXISTS ${CMAKE_BINARY_DIR}/libSDL2.so)
  set(SDL2_LIBRARY ${CMAKE_BINARY_DIR}/libSDL2.so)
else()
  find_library(SDL2_LIBRARY NAMES SDL2-2.0 SDL2
    HINTS /usr/lib/x86_64-linux-gnu /usr/local/lib /usr/lib)
endif()

if(SDL2_LIBRARY AND SDL2_INCLUDE_DIR)
  message(STATUS "UI: SDL2 at ${SDL2_LIBRARY}")
  target_link_libraries(chimera_ui PUBLIC ${SDL2_LIBRARY} m dl pthread)
  target_include_directories(chimera_ui PUBLIC ${SDL2_INCLUDE_DIR} ${SDL2_INCLUDE_DIR}/SDL2)
  target_include_directories(chimera_ui PRIVATE ${SDL2_ARCH_INCLUDE})
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
