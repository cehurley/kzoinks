target_sources(Synth PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/RingModModule.cpp
)
target_include_directories(Synth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
juce_add_binary_data(RingModAssets NAMESPACE RingModAssets HEADER_NAME RingModData.h SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/lightblue.png
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/faceplaterm.png
)
target_link_libraries(Synth PRIVATE RingModAssets)
target_include_directories(Synth PRIVATE
    $<TARGET_PROPERTY:RingModAssets,INTERFACE_INCLUDE_DIRECTORIES>
)