cmake_minimum_required(VERSION 3.22)
project(GMajorReborn VERSION 0.9.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (MSVC)
    add_compile_definitions(_USE_MATH_DEFINES NOMINMAX)
endif()
if (MINGW)
    add_compile_definitions(_USE_MATH_DEFINES)
endif()

option(GMR_BUILD_PLUGIN "Build the JUCE VST3 + Standalone (needs JUCE)" ON)
option(GMR_BUILD_TESTS  "Build the headless test suite"                ON)
option(GMR_BUILD_TOOLS  "Build the offline render / compare tools"     ON)

add_subdirectory(core)

if (GMR_BUILD_TESTS)
    enable_testing()
    add_executable(gmr_tests tests/TestMain.cpp)
    target_link_libraries(gmr_tests PRIVATE gmr_core)
    add_test(NAME gmr_tests COMMAND gmr_tests)
endif()

if (GMR_BUILD_TOOLS)
    add_executable(gmr_render tools/Render.cpp)
    target_link_libraries(gmr_render PRIVATE gmr_core)
endif()

if (GMR_BUILD_PLUGIN)
    add_subdirectory(plugin)
endif()
