target_sources(Synth PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/PhaserModule.cpp
)
target_include_directories(Synth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

juce_add_binary_data(PhaserAssets NAMESPACE PhaserAssets HEADER_NAME PhaserData.h SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/wh-knob.png)
target_link_libraries(Synth PRIVATE PhaserAssets)
target_include_directories(Synth PRIVATE $<TARGET_PROPERTY:PhaserAssets,INTERFACE_INCLUDE_DIRECTORIES>)
