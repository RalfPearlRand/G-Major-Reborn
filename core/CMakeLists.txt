add_library(gmr_core STATIC
    src/Tables.cpp src/Params.cpp src/Patch.cpp src/Engine.cpp src/FactoryPresets.cpp
    src/dsp/Dynamics.cpp src/dsp/FilterMod.cpp src/dsp/Pitch.cpp
    src/dsp/ChorusFlanger.cpp src/dsp/Delay.cpp src/dsp/Reverb.cpp)
target_include_directories(gmr_core PUBLIC include)
target_compile_features(gmr_core PUBLIC cxx_std_17)
set_target_properties(gmr_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if (MSVC)
    target_compile_options(gmr_core PRIVATE /W4 /fp:fast)
else()
    target_compile_options(gmr_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
