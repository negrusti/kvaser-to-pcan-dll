cmake_minimum_required(VERSION 3.16)
project(canlib32 VERSION 1.0 LANGUAGES CXX)

# Must be 32-bit — target application is x86
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR
        "Build must target 32-bit (x86).\n"
        "Re-run CMake with:  cmake -A Win32 <source_dir>")
endif()

# Path to the PCAN-Basic SDK.
# Default matches the typical PEAK installer location.
# Override with:  cmake -DPCAN_SDK_DIR="C:/your/path" ...
set(PCAN_SDK_DIR "C:/Program Files/PEAK-System/PCAN-Basic API"
    CACHE PATH "Root directory of the PCAN-Basic SDK")

add_library(canlib32 SHARED
    src/dllmain.cpp
    src/canlib32.cpp
    canlib32.def
)

target_include_directories(canlib32 PRIVATE
    include                              # PCANBasic types/constants
)

# PCANBasic.dll is loaded at runtime via LoadLibrary — no import lib needed.

if(MSVC)
    target_compile_options(canlib32 PRIVATE
        /W4
        /wd4100   # unreferenced formal parameter (stubs)
        /wd4996   # _snprintf_s etc.
    )
endif()

# Output as canlib32.dll with no lib prefix
set_target_properties(canlib32 PROPERTIES
    OUTPUT_NAME                       "canlib32"
    PREFIX                            ""
    RUNTIME_OUTPUT_DIRECTORY_RELEASE  "${CMAKE_SOURCE_DIR}/dist"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG    "${CMAKE_SOURCE_DIR}/dist"
)
