include(${CMAKE_SOURCE_DIR}/cmake/ChimeraConfig.cmake)

function(add_chimera_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE chimera_audio_engine)
  chimera_set_realtime_flags(${name})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_chimera_test(test_audio_graph test_audio_graph.cpp)
add_chimera_test(test_ring_buffer test_ring_buffer.cpp)
add_chimera_test(test_session test_session.cpp)
add_chimera_test(test_wav_loader test_wav_loader.cpp)
add_executable(test_plugin_abi test_plugin_abi.cpp)
target_include_directories(test_plugin_abi PRIVATE ${CMAKE_SOURCE_DIR}/sdk/include)
target_link_libraries(test_plugin_abi PRIVATE chimera_audio_engine ${CMAKE_DL_LIBS})
chimera_set_realtime_flags(test_plugin_abi)
add_test(NAME test_plugin_abi COMMAND test_plugin_abi)
set_tests_properties(test_plugin_abi PROPERTIES
  ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/sdk/examples:${CMAKE_BINARY_DIR}/software/audio-engine"
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)
add_executable(test_nodes test_nodes.cpp)
target_link_libraries(test_nodes PRIVATE chimera_audio_engine)
chimera_set_realtime_flags(test_nodes)
add_test(NAME test_nodes COMMAND test_nodes)

add_executable(test_plugin_host test_plugin_host.cpp)
target_include_directories(test_plugin_host PRIVATE ${CMAKE_SOURCE_DIR}/sdk/include)
target_link_libraries(test_plugin_host PRIVATE chimera_audio_engine ${CMAKE_DL_LIBS})
chimera_set_realtime_flags(test_plugin_host)
add_test(NAME test_plugin_host COMMAND test_plugin_host)
set_tests_properties(test_plugin_host PROPERTIES
  ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/sdk/examples:${CMAKE_BINARY_DIR}/software/audio-engine"
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)
