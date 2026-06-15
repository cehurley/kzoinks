# synth-base module — self-contained subtractive synth voice controls
target_sources(Synth PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/SynthBaseModule.cpp
)

target_include_directories(Synth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
