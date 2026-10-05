#pragma once

#include "spacetrace/DatasetCatalog.h"
#include "HeadRepository.h"
#include "spacetrace/RealtimeFIRRenderer.h"
#include "spacetrace/PapaPanRenderer.h"
#include "spacetrace/Resample.h"
#include "spacetrace/DistanceModel.h"
#include "StereoPairGeometry.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include <atomic>
#include <memory>
#include <vector>

namespace spacetrace::plugin {

enum class RendererMode {
    ModernHRTF = 0,
    PapaPan = 1,
};

enum class InputMode {
    MonoPointSource = 0,
    StereoPair = 1,
};

class SpaceTraceAudioProcessor final : public juce::AudioProcessor {
public:
    SpaceTraceAudioProcessor();
    ~SpaceTraceAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // Full 360 ms composed Stereo Pair corrections plus the shipping HRIR tail.
    double getTailLengthSeconds() const override { return 0.40; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& parameters() noexcept { return parameters_; }
    const juce::AudioProcessorValueTreeState& parameters() const noexcept { return parameters_; }

    RendererMode rendererMode() const noexcept;
    void setRendererMode(RendererMode mode);

    InputMode inputMode() const noexcept;

    BuiltInDatasetId selectedBuiltInDataset() const noexcept;
    juce::String selectedDatasetName() const;
    void selectBuiltInDataset(BuiltInDatasetId id);

    CompensationMode compensationMode() const noexcept;
    void setCompensationMode(CompensationMode mode);
    bool loadCustomCompensationIR(const juce::File& file, juce::String& error);

    juce::String statusText() const;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static constexpr const char* paramAzimuth = "azimuth";
    static constexpr const char* paramElevation = "elevation";
    static constexpr const char* paramDistance = "distance";
    static constexpr const char* paramInputMode = "input_mode";
    static constexpr const char* paramWidthOffset = "width_offset";
    static constexpr const char* paramAirLoss = "air_loss";
    static constexpr const char* paramOutputDb = "output_db";
    static constexpr const char* paramBypass = "bypass";
    static constexpr double maxCustomCompensationSeconds = 0.25;
    static constexpr int stateSchemaVersion = 5;

private:
    static std::size_t requiredKernelLength(const HRTFSet& set);

    bool prepareStructuralState(bool suspendHost);
    void updateStructuralStateProperties();
    void restoreStructuralStateProperties();

    juce::AudioProcessorValueTreeState parameters_;
    juce::ValueTree structuralState_ {"SpaceTraceStructuralState"};

    std::shared_ptr<const PreparedHeadData> activeModernHead_;
    std::shared_ptr<const PreparedHeadData> activePapaHead_;
    // Requested structural state. Publication to the audio thread happens only
    // after the matching runtime HRTF/compensation pair is prepared.
    std::atomic<int> rendererModeIndex_ {static_cast<int>(RendererMode::ModernHRTF)};
    std::atomic<int> activeRendererModeIndex_ {static_cast<int>(RendererMode::ModernHRTF)};
    std::atomic<int> selectedDatasetIndex_ {0};
    std::atomic<int> compensationModeIndex_ {static_cast<int>(CompensationMode::DatasetCorrected)};
    std::atomic<int> activeDatasetIndex_ {0};
    std::atomic<int> activeCompensationModeIndex_ {static_cast<int>(CompensationMode::Raw)};

    RealtimeFIRRenderer hrtfRenderer_;
    RealtimeFIRRenderer stereoPairRenderer_;
    PapaPanRenderer papaPanRenderer_;
    juce::dsp::Convolution commonCompensation_;
    juce::dsp::Convolution stereoPairLeftCompensation_;
    juce::dsp::Convolution stereoPairRightCompensation_;
    juce::AudioBuffer<float> sourceScratch_;
    juce::AudioBuffer<float> stereoPairScratch_;
    juce::SmoothedValue<float> outputGain_;
    juce::SmoothedValue<float> distanceGain_;
    juce::SmoothedValue<float> distanceAirHighGain_;
    DistanceModel distanceModel_;

    CompensationProfile customCompensation_;

    // Cached once during construction so processBlock performs only atomic loads.
    std::atomic<float>* azimuthValue_ = nullptr;
    std::atomic<float>* elevationValue_ = nullptr;
    std::atomic<float>* distanceValue_ = nullptr;
    std::atomic<float>* inputModeValue_ = nullptr;
    std::atomic<float>* widthOffsetValue_ = nullptr;
    std::atomic<float>* airLossValue_ = nullptr;
    std::atomic<float>* outputDbValue_ = nullptr;
    std::atomic<float>* bypassValue_ = nullptr;
    juce::AudioProcessorParameter* bypassParameter_ = nullptr;
    std::atomic<bool> prepared_ {false};
    std::atomic<bool> correctionReady_ {false};
    std::atomic<bool> stereoPairCorrectionReady_ {false};
    double currentSampleRate_ = 44100.0;
    int currentMaxBlock_ = 512;
    std::atomic<bool> rendererReady_ {false};
    std::atomic<float> activeHeadTrimLinear_ {1.0f};
    std::atomic<float> activeMonoTrimLinear_ {1.0f};
    std::atomic<float> activeHeadLeftGainLinear_ {1.0f};
    std::atomic<float> activeHeadRightGainLinear_ {1.0f};
    std::atomic<float> activeStereoPairLeftGainLinear_ {1.0f};
    std::atomic<float> activeStereoPairRightGainLinear_ {1.0f};
    std::atomic<float> activeStereoPairTrimLinear_ {1.0f};
    juce::String restoredHeadHash_;
    juce::String unresolvedHeadId_;
    bool wasBypassed_ = false;
    bool wasStereoPairMode_ = false;
    juce::String status_ {"Ready"};
    mutable juce::CriticalSection stateLock_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpaceTraceAudioProcessor)
};

} // namespace spacetrace::plugin
