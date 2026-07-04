include(${CMAKE_SOURCE_DIR}/cmake/ChimeraConfig.cmake)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

set(CHIMERA_AUDIO_ENGINE_SOURCES
  src/audio_buffer.cpp
  src/audio_graph.cpp
  src/audio_node.cpp
  src/engine.cpp
  src/logger.cpp
  src/plugin_host.cpp
  src/port.cpp
  src/session.cpp
  src/wav_loader.cpp
  src/backends/dummy_backend.cpp
  src/nodes/audio_io.cpp
  src/nodes/drum_node.cpp
  src/nodes/gain_node.cpp
  src/nodes/master_output.cpp
  src/nodes/sampler_node.cpp
  src/nodes/step_sequencer.cpp
  src/nodes/synth_node.cpp
  src/nodes/test_tone.cpp
)

add_library(chimera_audio_engine STATIC
  ${CHIMERA_AUDIO_ENGINE_SOURCES}
)

target_include_directories(chimera_audio_engine PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/chimera>
)

target_include_directories(chimera_audio_engine PRIVATE
  ${CMAKE_SOURCE_DIR}/sdk/include
)

chimera_set_realtime_flags(chimera_audio_engine)
chimera_no_rtti(chimera_audio_engine)

find_library(DL_LIB dl)
if(DL_LIB)
  target_link_libraries(chimera_audio_engine PRIVATE ${DL_LIB})
endif()

option(CHIMERA_USE_ALSA "Use ALSA audio backend" ON)

if(CHIMERA_USE_ALSA)
  find_package(ALSA QUIET)
  if(ALSA_FOUND)
    list(APPEND CHIMERA_AUDIO_ENGINE_SOURCES src/backends/alsa_backend.cpp)
    target_sources(chimera_audio_engine PRIVATE src/backends/alsa_backend.cpp)
    target_compile_definitions(chimera_audio_engine PRIVATE CHIMERA_HAS_ALSA)
    target_include_directories(chimera_audio_engine PRIVATE ${ALSA_INCLUDE_DIR})
    target_link_libraries(chimera_audio_engine PRIVATE ALSA::ALSA)
    message(STATUS "Audio engine: ALSA backend enabled")
  endif()
endif()

if(CHIMERA_USE_JACK)
  find_package(Jack QUIET)
  if(JACK_FOUND)
    target_compile_definitions(chimera_audio_engine PRIVATE CHIMERA_HAS_JACK)
    target_link_libraries(chimera_audio_engine PRIVATE Jack::jack)
    message(STATUS "Audio engine: JACK backend enabled")
  endif()
endif()

if(CHIMERA_USE_PIPEWIRE)
  find_package(PipeWire QUIET)
  if(PipeWire_FOUND)
    target_compile_definitions(chimera_audio_engine PRIVATE CHIMERA_HAS_PIPEWIRE)
    target_link_libraries(chimera_audio_engine PRIVATE PipeWire::pipewire)
    message(STATUS "Audio engine: PipeWire backend enabled")
  endif()
endif()

if(NOT ALSA_FOUND AND NOT JACK_FOUND AND NOT PipeWire_FOUND)
  message(STATUS "Audio engine: no audio backend found, using dummy backend")
  target_compile_definitions(chimera_audio_engine PRIVATE CHIMERA_USE_DUMMY)
endif()
