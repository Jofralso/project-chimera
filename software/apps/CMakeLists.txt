add_executable(chimera-play chimera-play.cpp)
target_link_libraries(chimera-play PRIVATE chimera_audio_engine)
target_include_directories(chimera-play PRIVATE
  ${CMAKE_SOURCE_DIR}/software/audio-engine/include
  ${CMAKE_SOURCE_DIR}/sdk/include
)
