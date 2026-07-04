include(${CMAKE_SOURCE_DIR}/cmake/ChimeraConfig.cmake)

add_executable(chimera-play chimera-play.cpp)
target_link_libraries(chimera-play PRIVATE chimera_audio_engine)
target_include_directories(chimera-play PRIVATE
  ${CMAKE_SOURCE_DIR}/software/audio-engine/include
  ${CMAKE_SOURCE_DIR}/sdk/include
)
chimera_set_realtime_flags(chimera-play)
chimera_set_jetson_flags(chimera-play)

if(TARGET chimera_ui)
  add_executable(chimera-desktop chimera-desktop.cpp)
  target_link_libraries(chimera-desktop PRIVATE chimera_ui chimera_audio_engine)
  target_include_directories(chimera-desktop PRIVATE
    ${CMAKE_SOURCE_DIR}/software/audio-engine/include
    ${CMAKE_SOURCE_DIR}/software/chimera-ui/include
    ${CMAKE_SOURCE_DIR}/sdk/include
  )
  chimera_set_realtime_flags(chimera-desktop)
  chimera_set_jetson_flags(chimera-desktop)
endif()
