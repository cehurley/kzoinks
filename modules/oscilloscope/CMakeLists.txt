# oscilloscope module — real-time waveform display
target_sources(Synth PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/OscilloscopeModule.cpp
)

target_include_directories(Synth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
