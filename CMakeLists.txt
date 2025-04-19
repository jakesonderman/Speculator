cmake_minimum_required(VERSION 3.15)

project(MyJUCEPlugin VERSION 1.0.0)

# Set JUCE path
set(JUCE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/modules" CACHE STRING "JUCE modules path")
set(JUCE_EXAMPLES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/examples" CACHE STRING "JUCE examples path")
set(JUCE_EXTRAS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/extras" CACHE STRING "JUCE extras path")

# Add JUCE as a subdirectory
add_subdirectory(JUCE)

# Create the plugin target
juce_add_plugin(MyPlugin
    VERSION 1.0.0
    COMPANY_NAME "YourCompany"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE YcCo
    PLUGIN_CODE Yp01
    FORMATS VST3
    PRODUCT_NAME "MyPlugin")

# Add source files
target_sources(MyPlugin
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/SamplePlayer.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.h
        Source/SamplePlayer.h)

# Add include directories
target_include_directories(MyPlugin
    PRIVATE
        Source
        ${JUCE_MODULE_PATH})

# Link against JUCE modules
target_link_libraries(MyPlugin
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_core
        juce::juce_audio_formats
        juce::juce_audio_basics)