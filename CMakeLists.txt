cmake_minimum_required(VERSION 3.22)
project(VocalClarity VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.8
  GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(VocalClarity
  COMPANY_NAME "DutchAudio"
  PLUGIN_MANUFACTURER_CODE Dutc
  PLUGIN_CODE Vclr
  FORMATS VST3 Standalone
  PRODUCT_NAME "Vocal Clarity"
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx Dynamics)

juce_generate_juce_header(VocalClarity)

target_sources(VocalClarity PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginProcessor.h
  Source/PluginEditor.cpp
  Source/PluginEditor.h
  Source/DSP.h)

target_compile_definitions(VocalClarity PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(VocalClarity
  PRIVATE
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
