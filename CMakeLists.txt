cmake_minimum_required(VERSION 3.22)
project(SpaceTrace VERSION 1.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SPACETRACE_BUILD_TESTS "Build SpaceTrace core tests" ON)
option(SPACETRACE_BUILD_TOOLS "Build SpaceTrace command-line tools" ON)
option(SPACETRACE_BUILD_PLUGIN "Build JUCE VST3/Standalone plugin" OFF)
option(SPACETRACE_BUILD_CLAP "Build CLAP from the same JUCE target via pinned clap-juce-extensions" ON)
option(SPACETRACE_BUILD_VISUAL_SNAPSHOTS "Build developer-only real-editor visual snapshot harness" OFF)

set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/JUCE" CACHE PATH "Path to pinned JUCE 8.0.12 source tree")
set(CLAP_JUCE_EXTENSIONS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/clap-juce-extensions" CACHE PATH "Path to clap-juce-extensions source tree")

add_library(spacetrace_core
    core/src/HRTFSet.cpp
    core/src/Compensation.cpp
    core/src/FIRRenderer.cpp
    core/src/NativePackage.cpp
    core/src/DatasetCatalog.cpp
    core/src/Resample.cpp
    core/src/RealtimeFIRRenderer.cpp
    core/src/PapaPanRenderer.cpp
    core/src/DistanceModel.cpp
)

target_include_directories(spacetrace_core PUBLIC core/include)

target_compile_options(spacetrace_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

if(SPACETRACE_BUILD_TOOLS)
    add_executable(spacetrace_probe tools/spacetrace_probe.cpp)
    target_link_libraries(spacetrace_probe PRIVATE spacetrace_core)

    add_executable(spacetrace_package_info tools/spacetrace_package_info.cpp)
    target_link_libraries(spacetrace_package_info PRIVATE spacetrace_core)

    add_executable(spacetrace_sweep_measure tools/spacetrace_sweep_measure.cpp)
    target_link_libraries(spacetrace_sweep_measure PRIVATE spacetrace_core)
endif()

if(SPACETRACE_BUILD_TESTS)
    add_executable(spacetrace_core_tests tests/test_core.cpp)
    target_link_libraries(spacetrace_core_tests PRIVATE spacetrace_core)
    enable_testing()
    add_test(NAME spacetrace_core_tests COMMAND spacetrace_core_tests)

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME spacetrace_source_contract
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/source_contract.py)
        add_test(NAME spacetrace_head_packages
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/validate_heads.py)
        add_test(NAME spacetrace_head_level_match
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_head_level_match.py)
        add_test(NAME spacetrace_head_tool_tests
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_head_scripts.py)
        add_test(NAME spacetrace_correction_composition
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_correction_composition.py)
        add_test(NAME spacetrace_release_staging
                 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_release_staging.py)
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/datasets/IRC_1050_R_44100.sthrtf" AND
       EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/datasets/mit_kemar_normal_pinna.sthrtf")
        add_executable(spacetrace_dataset_tests tests/test_datasets.cpp)
        target_link_libraries(spacetrace_dataset_tests PRIVATE spacetrace_core)
        target_compile_definitions(spacetrace_dataset_tests PRIVATE
            SPACETRACE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME spacetrace_dataset_tests COMMAND spacetrace_dataset_tests)
    endif()
endif()

if(SPACETRACE_BUILD_PLUGIN)
    if(EXISTS "${JUCE_DIR}/CMakeLists.txt")
        add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
    else()
        find_package(JUCE 8.0.12 CONFIG REQUIRED)
    endif()

    # SpaceTrace 1.0 heads are external portable resources under Heads/.
    # The plug-in binary intentionally embeds no HRTF datasets.

    juce_add_plugin(SpaceTrace
        COMPANY_NAME "LanesAudio"
        BUNDLE_ID "com.lanesaudio.spacetrace"
        PLUGIN_MANUFACTURER_CODE LnAu
        PLUGIN_CODE SpTr
        FORMATS VST3 Standalone
        PRODUCT_NAME "SpaceTrace"
        DESCRIPTION "Accessible binaural HRTF spatializer"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
    )

    target_sources(SpaceTrace PRIVATE
        plugin/PluginProcessor.cpp
        plugin/PluginProcessor.h
        plugin/HeadRepository.cpp
        plugin/HeadRepository.h
        plugin/PluginEditor.cpp
        plugin/PluginEditor.h
        plugin/SpatialDisplay.cpp
        plugin/SpatialDisplay.h
        plugin/AccessibleParameterSlider.cpp
        plugin/AccessibleParameterSlider.h
        plugin/AccessibleComboBox.cpp
        plugin/AccessibleComboBox.h
    )

    target_link_libraries(SpaceTrace PRIVATE
        spacetrace_core
        juce::juce_audio_utils
        juce::juce_cryptography
        juce::juce_dsp
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )

    target_compile_definitions(SpaceTrace PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )

    if(SPACETRACE_BUILD_TESTS)
        add_executable(spacetrace_plugin_tests tests/test_plugin_processor.cpp)
        target_link_libraries(spacetrace_plugin_tests PRIVATE
            SpaceTrace
            spacetrace_core
            juce::juce_audio_utils
            juce::juce_dsp)
        target_include_directories(spacetrace_plugin_tests PRIVATE plugin core/include)
        add_test(NAME spacetrace_plugin_tests COMMAND spacetrace_plugin_tests)
        set_tests_properties(spacetrace_plugin_tests PROPERTIES
            ENVIRONMENT "SPACETRACE_HEADS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/Heads")
        add_executable(spacetrace_mono_level_tests tests/test_mono_level.cpp)
        target_link_libraries(spacetrace_mono_level_tests PRIVATE SpaceTrace spacetrace_core juce::juce_audio_utils juce::juce_dsp)
        target_include_directories(spacetrace_mono_level_tests PRIVATE plugin core/include)
        add_test(NAME spacetrace_mono_level_tests COMMAND spacetrace_mono_level_tests)
        set_tests_properties(spacetrace_mono_level_tests PROPERTIES
            ENVIRONMENT "SPACETRACE_HEADS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/Heads")
    endif()

    if(SPACETRACE_BUILD_VISUAL_SNAPSHOTS)
        add_executable(spacetrace_visual_snapshots tests/visual_snapshots.cpp)
        target_link_libraries(spacetrace_visual_snapshots PRIVATE
            SpaceTrace
            spacetrace_core
            juce::juce_audio_utils
            juce::juce_dsp)
        target_include_directories(spacetrace_visual_snapshots PRIVATE plugin core/include)
    endif()

    if(SPACETRACE_BUILD_CLAP AND EXISTS "${CLAP_JUCE_EXTENSIONS_DIR}/CMakeLists.txt")
        set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        add_subdirectory("${CLAP_JUCE_EXTENSIONS_DIR}" "${CMAKE_BINARY_DIR}/clap-juce-extensions" EXCLUDE_FROM_ALL)
        clap_juce_extensions_plugin(TARGET SpaceTrace
            CLAP_ID "com.lanesaudio.spacetrace"
            CLAP_FEATURES audio-effect stereo
            CLAP_USE_JUCE_PARAMETER_RANGES ALL
            CLAP_CHECKING_LEVEL Minimal)
        if(SPACETRACE_BUILD_TESTS AND WIN32)
            add_executable(spacetrace_clap_tests tests/test_clap_binary.cpp)
            target_include_directories(spacetrace_clap_tests PRIVATE "${CLAP_JUCE_EXTENSIONS_DIR}/clap-libs/clap/include")
            target_compile_definitions(spacetrace_clap_tests PRIVATE
                SPACETRACE_EXPECTED_VERSION=\"${PROJECT_VERSION}\")
            add_dependencies(spacetrace_clap_tests SpaceTrace_CLAP)
            add_test(NAME spacetrace_clap_tests COMMAND spacetrace_clap_tests $<TARGET_FILE:SpaceTrace_CLAP>)
            set_tests_properties(spacetrace_clap_tests PROPERTIES
                ENVIRONMENT "SPACETRACE_HEADS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/Heads"
                TIMEOUT 120)
        endif()
    elseif(SPACETRACE_BUILD_CLAP)
        message(FATAL_ERROR "CLAP requested but clap-juce-extensions is absent. Run FETCH_CLAP_WINDOWS.ps1 or set CLAP_JUCE_EXTENSIONS_DIR.")
    endif()
endif()
