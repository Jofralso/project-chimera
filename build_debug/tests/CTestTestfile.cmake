# CMake generated Testfile for 
# Source directory: /home/j3tson/project_chimera/tests
# Build directory: /home/j3tson/project_chimera/build_debug/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[test_audio_graph]=] "/home/j3tson/project_chimera/build_debug/tests/test_audio_graph")
set_tests_properties([=[test_audio_graph]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/j3tson/project_chimera/tests/CMakeLists.txt;7;add_test;/home/j3tson/project_chimera/tests/CMakeLists.txt;10;add_chimera_test;/home/j3tson/project_chimera/tests/CMakeLists.txt;0;")
add_test([=[test_ring_buffer]=] "/home/j3tson/project_chimera/build_debug/tests/test_ring_buffer")
set_tests_properties([=[test_ring_buffer]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/j3tson/project_chimera/tests/CMakeLists.txt;7;add_test;/home/j3tson/project_chimera/tests/CMakeLists.txt;11;add_chimera_test;/home/j3tson/project_chimera/tests/CMakeLists.txt;0;")
add_test([=[test_session]=] "/home/j3tson/project_chimera/build_debug/tests/test_session")
set_tests_properties([=[test_session]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/j3tson/project_chimera/tests/CMakeLists.txt;7;add_test;/home/j3tson/project_chimera/tests/CMakeLists.txt;12;add_chimera_test;/home/j3tson/project_chimera/tests/CMakeLists.txt;0;")
add_test([=[test_wav_loader]=] "/home/j3tson/project_chimera/build_debug/tests/test_wav_loader")
set_tests_properties([=[test_wav_loader]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/j3tson/project_chimera/tests/CMakeLists.txt;7;add_test;/home/j3tson/project_chimera/tests/CMakeLists.txt;13;add_chimera_test;/home/j3tson/project_chimera/tests/CMakeLists.txt;0;")
add_test([=[test_plugin_abi]=] "/home/j3tson/project_chimera/build_debug/tests/test_plugin_abi")
set_tests_properties([=[test_plugin_abi]=] PROPERTIES  ENVIRONMENT "LD_LIBRARY_PATH=/home/j3tson/project_chimera/build_debug/sdk/examples:/home/j3tson/project_chimera/build_debug/software/audio-engine" WORKING_DIRECTORY "/home/j3tson/project_chimera/build_debug/tests" _BACKTRACE_TRIPLES "/home/j3tson/project_chimera/tests/CMakeLists.txt;18;add_test;/home/j3tson/project_chimera/tests/CMakeLists.txt;0;")
add_test([=[test_nodes]=] "/home/j3tson/project_chimera/build_debug/tests/test_nodes")
set_tests_properties([=[test_nodes]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/j3tson/project_chimera/tests/CMakeLists.txt;26;add_test;/home/j3tson/project_chimera/tests/CMakeLists.txt;0;")
add_test([=[test_plugin_host]=] "/home/j3tson/project_chimera/build_debug/tests/test_plugin_host")
set_tests_properties([=[test_plugin_host]=] PROPERTIES  ENVIRONMENT "LD_LIBRARY_PATH=/home/j3tson/project_chimera/build_debug/sdk/examples:/home/j3tson/project_chimera/build_debug/software/audio-engine" WORKING_DIRECTORY "/home/j3tson/project_chimera/build_debug/tests" _BACKTRACE_TRIPLES "/home/j3tson/project_chimera/tests/CMakeLists.txt;32;add_test;/home/j3tson/project_chimera/tests/CMakeLists.txt;0;")
