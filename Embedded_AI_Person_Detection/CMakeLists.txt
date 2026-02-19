cmake_minimum_required(VERSION 3.13...3.27)

# initialize pico-sdk from submodule
# note: this must happen before project()
# include(third_party/pico-sdk/pico_sdk_init.cmake)
include(pico_sdk_import.cmake) # Alternative: import pico-sdk from system-wide installation

project(assignment6 C CXX ASM)

# RP2040
set(PICO_BOARD pico)

add_compile_options(-Wall -Winline)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# Add the path to the third-party libraries besides pico-sdk
add_subdirectory(third_party/pico-tflmicro)

# Add the path to the source files
include_directories(src)

# Add the executable. This will build the source files *.c/*.cpp in the src directory
file(GLOB_RECURSE ASSIGNMENT6_SOURCES
    src/*.c
    src/*.cpp
)

add_executable(assignment6 ${ASSIGNMENT6_SOURCES})

pico_enable_stdio_usb(assignment6 1)
pico_enable_stdio_uart(assignment6 0)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(assignment6 pico_stdlib)
target_link_libraries(assignment6 pico-tflmicro)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(assignment6)

# Add the path to the source files for tests
enable_testing()
# add_subdirectory(tests)