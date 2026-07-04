include(CheckCXXCompilerFlag)

function(chimera_set_realtime_flags target)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic
      -Wno-unused-parameter
      -fno-exceptions
      -fno-rtti
      -ffast-math
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
      target_compile_options(${target} PRIVATE -O3 -flto)
      target_link_options(${target} PRIVATE -flto)
    endif()
  elseif(MSVC)
    target_compile_options(${target} PRIVATE /W4 /wd4100)
  endif()
endfunction()

function(chimera_set_jetson_flags target)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_options(${target} PRIVATE
      -march=armv8.2-a
      -mtune=cortex-a78ae
    )
    target_link_options(${target} PRIVATE
      -march=armv8.2-a
      -mtune=cortex-a78ae
    )
  endif()
endfunction()

function(chimera_no_exceptions target)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE -fno-exceptions)
  endif()
endfunction()

function(chimera_no_rtti target)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE -fno-rtti)
  endif()
endfunction()

function(chimera_warn_stack_protector target)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target} PRIVATE -fstack-protector-strong)
  endif()
endfunction()
