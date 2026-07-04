cmake_minimum_required(VERSION 3.25)
project(Chimera VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(CHIMERA_BUILD_TESTS "Build test suite" ON)
option(CHIMERA_BUILD_SDK_EXAMPLES "Build SDK examples" ON)
option(CHIMERA_USE_JACK "Use JACK audio backend" ON)
option(CHIMERA_USE_PIPEWIRE "Use PipeWire audio backend" OFF)

add_subdirectory(software/audio-engine)

if(CHIMERA_BUILD_SDK_EXAMPLES)
  add_subdirectory(sdk)
endif()

option(CHIMERA_BUILD_UI "Build UI library and desktop app" ON)
if(CHIMERA_BUILD_UI)
  add_subdirectory(software/chimera-ui)
endif()

add_subdirectory(software/apps)

if(CHIMERA_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
