# eq module — 8-band graphic EQ
target_sources(Synth PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/EQModule.cpp
)

target_include_directories(Synth PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
