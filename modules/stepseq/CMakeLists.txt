target_sources(Synth PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/StepSequencerModule.cpp
)
target_include_directories(Synth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

juce_add_binary_data(StepSeqAssets NAMESPACE StepSeqAssets HEADER_NAME StepSeqData.h SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/yellowknob.png)
target_link_libraries(Synth PRIVATE StepSeqAssets)
target_include_directories(Synth PRIVATE $<TARGET_PROPERTY:StepSeqAssets,INTERFACE_INCLUDE_DIRECTORIES>)
