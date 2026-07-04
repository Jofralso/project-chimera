add_library(chimera_gain_plugin SHARED gain_plugin.cpp)

target_include_directories(chimera_gain_plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

chimera_no_exceptions(chimera_gain_plugin)
chimera_no_rtti(chimera_gain_plugin)

set_target_properties(chimera_gain_plugin PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  PREFIX ""
)
