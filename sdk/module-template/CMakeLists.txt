# Replace every occurrence of "Template" with your module's name,
# then copy this folder into /modules/ — CMake will find and build it automatically.

target_sources(Synth PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/TemplateModule.cpp
)

target_include_directories(Synth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
