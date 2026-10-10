cmake_minimum_required(VERSION 3.22)
project(ZapoAI VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
include(FetchContent)
if(DEFINED JUCE_SOURCE_DIR)
  add_subdirectory("${JUCE_SOURCE_DIR}" JUCE)
else()
  FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 51a8a6d7aeae7326956d747737ccf1575e61e209 GIT_SHALLOW FALSE)
  FetchContent_MakeAvailable(JUCE)
endif()
juce_add_plugin(ZapoAI COMPANY_NAME "Zapo" PLUGIN_MANUFACTURER_CODE Zapo PLUGIN_CODE Zpai
  FORMATS VST3 PRODUCT_NAME "ZapoAI" IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE VST3_CATEGORIES Fx Tools)
juce_add_binary_data(ZapoFaces SOURCES assets/scared.jpg assets/disgusted.jpg assets/feeling.jpg
  assets/coach.jpg assets/unimpressed.jpg assets/teacher.jpg assets/stank.jpg assets/happy.jpg)
juce_generate_juce_header(ZapoAI)
target_sources(ZapoAI PRIVATE Source/Plugin.cpp Source/Capture.h)
target_compile_definitions(ZapoAI PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(ZapoAI PRIVATE ZapoFaces juce::juce_audio_utils juce::juce_audio_processors
  PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
include(CTest)
if(BUILD_TESTING)
  find_package(Threads REQUIRED)
  add_executable(CaptureTests tests/capture_test.cpp)
  target_link_libraries(CaptureTests PRIVATE Threads::Threads)
  add_test(NAME capture COMMAND CaptureTests)
endif()
