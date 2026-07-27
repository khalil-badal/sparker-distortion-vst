cmake_minimum_required(VERSION 3.22)

project(SuhrRiot VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE location — change this if you cloned JUCE somewhere other than C:/JUCE.
# You can also override it on the cmake command line:
#   cmake -B build -DJUCE_PATH="D:/libs/JUCE"
# ---------------------------------------------------------------------------
set(JUCE_PATH "C:/JUCE" CACHE PATH "Path to the JUCE source directory")
add_subdirectory(${JUCE_PATH} juce_build)

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------
juce_add_plugin(SuhrRiot
    COMPANY_NAME                "Khyle Audio"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    BUNDLE_ID                   "com.KhyleAudio.SparkerDistortion"
    PLUGIN_MANUFACTURER_CODE    KhAu   # 4-char code, unique per company
    PLUGIN_CODE                 SRot   # 4-char code, unique per plugin
    FORMATS                     VST3 Standalone
    PRODUCT_NAME                "Sparker Distortion"
)

juce_generate_juce_header(SuhrRiot)

# ---------------------------------------------------------------------------
# Embedded fonts (Inter + Barlow Condensed)
# ---------------------------------------------------------------------------
juce_add_binary_data(SparkerFonts
    SOURCES
        Assets/Fonts/Inter-Regular.ttf
        Assets/Fonts/Inter-Medium.ttf
        Assets/Fonts/Inter-SemiBold.ttf
        Assets/Fonts/Inter-Bold.ttf
        Assets/Fonts/BarlowCondensed-ExtraBold.ttf
        Assets/Fonts/BarlowCondensed-Regular.ttf
        Assets/Fonts/BarlowCondensed-Light.ttf
)

# ---------------------------------------------------------------------------
# Source files
# ---------------------------------------------------------------------------
target_sources(SuhrRiot
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/dsp/InputStage.cpp
        Source/dsp/DriveStage.cpp
        Source/dsp/ClippingStage.cpp
        Source/dsp/ToneStack.cpp
        Source/dsp/OutputStage.cpp
        Source/gui/PedalLookAndFeel.cpp
        Source/gui/ModeSwitch.cpp
        Source/gui/OsSwitch.cpp
        Source/dsp/TrimStage.cpp
        Source/dsp/TightFilter.cpp
        Source/dsp/NoiseGate.cpp
        Source/dsp/EmberCircuit.cpp
        Source/dsp/BurnerCircuit.cpp
        Source/dsp/BlazerCircuit.cpp
        Source/gui/CircuitSwitch.cpp
        Source/gui/AggressionSwitch.cpp
        Source/gui/BurnerLookAndFeel.cpp
        Source/gui/BlazerLookAndFeel.cpp
)

# ---------------------------------------------------------------------------
# Compile-time JUCE feature flags
# ---------------------------------------------------------------------------
target_compile_definitions(SuhrRiot
    PUBLIC
        JUCE_WEB_BROWSER=0          # not using the JUCE browser component
        JUCE_USE_CURL=0             # not using CURL
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# ---------------------------------------------------------------------------
# Link against the JUCE modules we need
# ---------------------------------------------------------------------------
target_link_libraries(SuhrRiot
    PRIVATE
        SparkerFonts
        juce::juce_audio_utils  # AudioProcessor, AudioProcessorEditor, APVTS, etc.
        juce::juce_dsp          # IIR filters, Oversampling, SmoothedValue, etc.
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ---------------------------------------------------------------------------
# Offline integration test harness (Phase 5) — exercises EmberCircuit /
# BurnerCircuit / BlazerCircuit directly, independent of the plugin/GUI
# target above. Not shipped with the plugin.
# ---------------------------------------------------------------------------
juce_add_console_app(SuhrRiotOfflineTests
    PRODUCT_NAME "SuhrRiot Offline Tests")

juce_generate_juce_header(SuhrRiotOfflineTests)

target_sources(SuhrRiotOfflineTests
    PRIVATE
        Tests/OfflineTests.cpp
        Source/dsp/EmberCircuit.cpp
        Source/dsp/BurnerCircuit.cpp
        Source/dsp/BlazerCircuit.cpp
        Source/dsp/InputStage.cpp
        Source/dsp/DriveStage.cpp
        Source/dsp/ClippingStage.cpp
        Source/dsp/ToneStack.cpp
        Source/dsp/OutputStage.cpp
        Source/dsp/TrimStage.cpp
        Source/dsp/TightFilter.cpp
        Source/dsp/NoiseGate.cpp
)

target_include_directories(SuhrRiotOfflineTests PRIVATE Source)

target_compile_definitions(SuhrRiotOfflineTests
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(SuhrRiotOfflineTests
    PRIVATE
        juce::juce_dsp
        juce::juce_audio_basics
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
