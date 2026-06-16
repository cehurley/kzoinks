# synth-base module — self-contained subtractive synth voice controls
target_sources(Synth PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/SynthBaseModule.cpp
)

target_include_directories(Synth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

juce_add_binary_data(SynthBaseAssets NAMESPACE SynthBaseAssets HEADER_NAME SynthBaseData.h SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/knob.png
)
target_link_libraries(Synth PRIVATE SynthBaseAssets)
target_include_directories(Synth PRIVATE
    $<TARGET_PROPERTY:SynthBaseAssets,INTERFACE_INCLUDE_DIRECTORIES>
)
