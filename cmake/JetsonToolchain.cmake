# Toolchain for NVIDIA Jetson Orin Nano (aarch64-linux-gnu)
# 
# Usage:
#   cmake -S . -B build-jetson -DCMAKE_TOOLCHAIN_FILE=cmake/JetsonToolchain.cmake
#
# Prerequisites on Jetson:
#   sudo apt update && sudo apt install -y build-essential cmake libsdl2-dev libasound2-dev
#
# For native build on Jetson (recommended for development):
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
#   (Skip the toolchain file for native builds)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Use native compiler if building on Jetson
if(NOT DEFINED ENV{TOOLCHAIN_PREFIX})
  set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
  set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
endif()

# Jetson Orin Nano uses Cortex-A78AE cores
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8.2-a -mtune=cortex-a78ae")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -march=armv8.2-a -mtune=cortex-a78ae")

# Jetson-specific audio backend defaults
set(CHIMERA_USE_JACK     OFF CACHE BOOL "Disable JACK on Jetson")
set(CHIMERA_USE_PIPEWIRE OFF CACHE BOOL "Disable PipeWire on Jetson")

# Touchscreen-optimized defaults
set(CHIMERA_TOUCH_MODE ON CACHE BOOL "Enable touchscreen optimizations")

# Modularity: keep binary small for embedded eMMC/SD
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
