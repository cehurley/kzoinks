target_sources(Synth PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/ReverbModule.cpp
)
target_include_directories(Synth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

juce_add_binary_data(ReverbAssets NAMESPACE ReverbAssets HEADER_NAME ReverbData.h SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/reverbknob.png
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/LABELLOGO.png)
target_link_libraries(Synth PRIVATE ReverbAssets)
target_include_directories(Synth PRIVATE $<TARGET_PROPERTY:ReverbAssets,INTERFACE_INCLUDE_DIRECTORIES>)
