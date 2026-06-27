target_sources(Synth PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/AutoWahModule.cpp
)
target_include_directories(Synth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

juce_add_binary_data(AutoWahAssets NAMESPACE AutoWahAssets HEADER_NAME AutoWahData.h SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/auto-wah-logo.png)
target_link_libraries(Synth PRIVATE AutoWahAssets)
target_include_directories(Synth PRIVATE $<TARGET_PROPERTY:AutoWahAssets,INTERFACE_INCLUDE_DIRECTORIES>)
