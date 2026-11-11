# Mini Moop - circuit-simulation synthesizer
# Copyright (C) 2026 Thierry Fraudet
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published
# by the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
# SPDX-License-Identifier: AGPL-3.0-or-later

cmake_minimum_required(VERSION 3.24)

# macOS: universal binaries (Apple Silicon + Intel), macOS 11 or later.
# Must be set before project(). Override with -DCMAKE_OSX_ARCHITECTURES=arm64
# for faster single-architecture development builds.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Target architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(MiniMoop VERSION 1.0.0 LANGUAGES C CXX)

string(TIMESTAMP MINIMOOP_BUILD_DATE "%Y-%m-%d")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(MINIMOOP_BUILD_PLUGIN "Build the JUCE plugin" ON)
option(MINIMOOP_BUILD_TESTS  "Build the unit tests" ON)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
# Use local clones in deps/ when present (avoids re-downloading on each fresh build dir).
if(NOT FETCHCONTENT_SOURCE_DIR_JUCE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/JUCE/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_JUCE "${CMAKE_CURRENT_SOURCE_DIR}/deps/JUCE")
endif()
if(NOT FETCHCONTENT_SOURCE_DIR_CATCH2 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/Catch2/CMakeLists.txt")
    set(FETCHCONTENT_SOURCE_DIR_CATCH2 "${CMAKE_CURRENT_SOURCE_DIR}/deps/Catch2")
endif()

# ---------------------------------------------------------------------------
# Circuit simulation + synth engine: pure C++, no JUCE dependency.
# ---------------------------------------------------------------------------
add_library(minimoop_dsp STATIC
    src/engine/Voice.cpp
    src/engine/SynthEngine.cpp
)
target_include_directories(minimoop_dsp PUBLIC src)
target_compile_options(minimoop_dsp PRIVATE -Wall -Wextra -Wno-unused-parameter)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(minimoop_dsp PRIVATE -O3 -ffp-contract=fast)
endif()
set_target_properties(minimoop_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
if(MINIMOOP_BUILD_PLUGIN)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.5
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)

    # Panel assets (wood texture) and the built-in Factory sound bank
    juce_add_binary_data(MiniMoopAssets
        HEADER_NAME MiniMoopAssets.h
        NAMESPACE MiniMoopAssets
        SOURCES resources/wood.jpg resources/leather.jpg resources/brushed.jpg presets/Factory.json)
    set_target_properties(MiniMoopAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

    juce_add_plugin(MiniMoop
        COMPANY_NAME              "Zazart"
        BUNDLE_ID                 "com.zazart.minimoop"
        PLUGIN_MANUFACTURER_CODE  Zzrt
        PLUGIN_CODE               MMop
        FORMATS                   VST3 AU Standalone
        PRODUCT_NAME              "Mini Moop"
        IS_SYNTH                  TRUE
        NEEDS_MIDI_INPUT          TRUE
        NEEDS_MIDI_OUTPUT         FALSE
        IS_MIDI_EFFECT            FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD   TRUE
        VST3_CATEGORIES           Instrument Synth
        AU_MAIN_TYPE              kAudioUnitType_MusicDevice)

    target_sources(MiniMoop PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/Parameters.cpp
        src/SoundBank.cpp
        src/ui/PanelLookAndFeel.cpp
        src/ui/Panel.cpp
        src/ui/AboutBox.cpp
        src/ui/BankManager.cpp)

    target_compile_definitions(MiniMoop PUBLIC
        MINIMOOP_VERSION_STRING="${PROJECT_VERSION}"
        MINIMOOP_BUILD_DATE="${MINIMOOP_BUILD_DATE}"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(MiniMoop
        PRIVATE
            minimoop_dsp
            MiniMoopAssets
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    # Headless UI snapshot tool (renders the editor to a PNG)
    juce_add_console_app(MiniMoopSnapshot PRODUCT_NAME "MiniMoopSnapshot")
    target_sources(MiniMoopSnapshot PRIVATE
        tools/snapshot/Snapshot.cpp
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        src/Parameters.cpp
        src/SoundBank.cpp
        src/ui/PanelLookAndFeel.cpp
        src/ui/Panel.cpp
        src/ui/AboutBox.cpp
        src/ui/BankManager.cpp)
    target_compile_definitions(MiniMoopSnapshot PRIVATE
        MINIMOOP_VERSION_STRING="${PROJECT_VERSION}"
        MINIMOOP_BUILD_DATE="${MINIMOOP_BUILD_DATE}"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(MiniMoopSnapshot PRIVATE
        minimoop_dsp MiniMoopAssets juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(MINIMOOP_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.16.1
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(Catch2)

    add_executable(minimoop_tests
        tests/test_solver.cpp
        tests/test_spice_validation.cpp
        tests/test_ladder.cpp
        tests/test_vco.cpp
        tests/test_voice.cpp
        tests/test_extpreamp.cpp
        tests/test_contour.cpp
        tests/test_vca.cpp
        tests/test_filter_output.cpp
        tests/test_reference_laws.cpp
        tests/test_noise_a440.cpp)
    target_link_libraries(minimoop_tests PRIVATE minimoop_dsp Catch2::Catch2WithMain)
    target_compile_definitions(minimoop_tests PRIVATE
        MINIMOOP_SPICE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tools/spice")
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_compile_options(minimoop_tests PRIVATE -O2)
    endif()

    enable_testing()
    include(CTest)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
    catch_discover_tests(minimoop_tests)

    # Sound bank model (needs JUCE core, so only with the plugin)
    if(MINIMOOP_BUILD_PLUGIN)
        juce_add_console_app(minimoop_bank_tests PRODUCT_NAME "minimoop_bank_tests")
        target_sources(minimoop_bank_tests PRIVATE tests/test_soundbank.cpp src/SoundBank.cpp)
        target_include_directories(minimoop_bank_tests PRIVATE src)
        target_compile_definitions(minimoop_bank_tests PRIVATE
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
        target_link_libraries(minimoop_bank_tests PRIVATE
            MiniMoopAssets juce::juce_core juce::juce_data_structures Catch2::Catch2WithMain
            juce::juce_recommended_config_flags)
        catch_discover_tests(minimoop_bank_tests)
    endif()
endif()
