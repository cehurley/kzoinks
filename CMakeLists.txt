cmake_minimum_required(VERSION 3.22)
project(Synth VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(Synth
    PRODUCT_NAME  "Synth"
    COMPANY_NAME  "OpenSynth"
    BUNDLE_ID     "com.opensynth.synth"
)

juce_generate_juce_header(Synth)

target_sources(Synth PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/ChainStrip.cpp
    Source/ModuleRegistry.cpp
    Source/SynthVoice.cpp
)

# Source/ is on the include path so modules can include IModule.h, SynthParameters.h, etc.
target_include_directories(Synth PRIVATE Source)

target_compile_definitions(Synth PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Synth,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Synth,JUCE_VERSION>"
)

target_link_libraries(Synth PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Auto-discover modules: any subdirectory of modules/ with a CMakeLists.txt is included.
file(GLOB MODULE_DIRS "modules/*/CMakeLists.txt")
foreach(module_cmake IN LISTS MODULE_DIRS)
    get_filename_component(module_dir "${module_cmake}" DIRECTORY)
    message(STATUS "Loading module: ${module_dir}")
    add_subdirectory("${module_dir}")
endforeach()
