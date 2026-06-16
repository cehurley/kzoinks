target_sources(Synth PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/DistortionModule.cpp
)
target_include_directories(Synth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

juce_add_binary_data(DistortionAssets NAMESPACE DistortionAssets HEADER_NAME DistortionData.h SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/knob.png
)
target_link_libraries(Synth PRIVATE DistortionAssets)
target_include_directories(Synth PRIVATE
    $<TARGET_PROPERTY:DistortionAssets,INTERFACE_INCLUDE_DIRECTORIES>
)
