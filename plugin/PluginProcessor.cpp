#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace spacetrace::plugin {
namespace {
constexpr int ircamIndex = 0;
constexpr int kemarIndex = 1;
constexpr int ku100Index = 2;
constexpr int full2DegIndex = 3;
constexpr int fabianIndex = 4;

int datasetIndex(BuiltInDatasetId id) noexcept {
    switch (id) {
        case BuiltInDatasetId::IrcamListen1050: return ircamIndex;
        case BuiltInDatasetId::MitKemarNormalPinna: return kemarIndex;
        case BuiltInDatasetId::Sadie2D1Ku100: return ku100Index;
        case BuiltInDatasetId::ThkKu100Full2Deg: return full2DegIndex;
        case BuiltInDatasetId::FabianHato0: return fabianIndex;
    }
    return ircamIndex;
}

BuiltInDatasetId datasetId(int index) noexcept {
    switch (index) {
        case kemarIndex: return BuiltInDatasetId::MitKemarNormalPinna;
        case ku100Index: return BuiltInDatasetId::Sadie2D1Ku100;
        case full2DegIndex: return BuiltInDatasetId::ThkKu100Full2Deg;
        case fabianIndex: return BuiltInDatasetId::FabianHato0;
        default: return BuiltInDatasetId::IrcamListen1050;
    }
}

juce::String azimuthText(float v) {
    const auto wrapped = std::fmod(v + 360.0f, 360.0f);
    if (std::abs(wrapped - 0.0f) < 0.5f || std::abs(wrapped - 360.0f) < 0.5f) return "0 degrees, front";
    if (std::abs(wrapped - 90.0f) < 0.5f) return "90 degrees, left";
    if (std::abs(wrapped - 180.0f) < 0.5f) return "180 degrees, rear";
    if (std::abs(wrapped - 270.0f) < 0.5f) return "270 degrees, right";
    return juce::String(wrapped, 0) + " degrees";
}

float parseNumber(const juce::String& s) {
    return s.retainCharacters("-+.0123456789").getFloatValue();
}

juce::MemoryBlock encodeCustomIR(const CompensationProfile& profile) {
    juce::MemoryBlock out;
    if (profile.impulse.empty() || profile.sampleRate <= 0.0) return out;
    const auto count = static_cast<std::uint32_t>(profile.impulse.size());
    out.setSize(sizeof(double) + sizeof(std::uint32_t) + profile.impulse.size() * sizeof(float), false);
    auto* p = static_cast<std::byte*>(out.getData());
    std::memcpy(p, &profile.sampleRate, sizeof(double));
    p += sizeof(double);
    std::memcpy(p, &count, sizeof(count));
    p += sizeof(count);
    std::memcpy(p, profile.impulse.data(), profile.impulse.size() * sizeof(float));
    return out;
}

bool decodeCustomIR(const juce::MemoryBlock& data, CompensationProfile& profile) {
    if (data.getSize() < sizeof(double) + sizeof(std::uint32_t)) return false;
    const auto* p = static_cast<const std::byte*>(data.getData());
    double sampleRate = 0.0;
    std::uint32_t count = 0;
    std::memcpy(&sampleRate, p, sizeof(sampleRate));
    p += sizeof(sampleRate);
    std::memcpy(&count, p, sizeof(count));
    p += sizeof(count);
    const auto expected = sizeof(double) + sizeof(std::uint32_t) + static_cast<std::size_t>(count) * sizeof(float);
    if (!(sampleRate > 0.0) || count == 0 || data.getSize() != expected) return false;
    profile.name = "Project Custom Correction IR";
    profile.version = "state";
    profile.sampleRate = sampleRate;
    profile.impulse.resize(count);
    std::memcpy(profile.impulse.data(), p, static_cast<std::size_t>(count) * sizeof(float));
    std::string reason;
    return profile.valid(&reason);
}

juce::AudioBuffer<float> monoIRBuffer(const CompensationProfile& profile) {
    juce::AudioBuffer<float> buffer(1, static_cast<int>(profile.impulse.size()));
    if (!profile.impulse.empty())
        std::copy(profile.impulse.begin(), profile.impulse.end(), buffer.getWritePointer(0));
    return buffer;
}
}

SpaceTraceAudioProcessor::SpaceTraceAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters_(*this, nullptr, "PARAMETERS", createParameterLayout()) {
    azimuthValue_ = parameters_.getRawParameterValue(paramAzimuth);
    elevationValue_ = parameters_.getRawParameterValue(paramElevation);
    distanceValue_ = parameters_.getRawParameterValue(paramDistance);
    inputModeValue_ = parameters_.getRawParameterValue(paramInputMode);
    widthOffsetValue_ = parameters_.getRawParameterValue(paramWidthOffset);
    airLossValue_ = parameters_.getRawParameterValue(paramAirLoss);
    outputDbValue_ = parameters_.getRawParameterValue(paramOutputDb);
    bypassValue_ = parameters_.getRawParameterValue(paramBypass);
    bypassParameter_ = parameters_.getParameter(paramBypass);
    jassert(azimuthValue_ != nullptr && elevationValue_ != nullptr && distanceValue_ != nullptr &&
            inputModeValue_ != nullptr && widthOffsetValue_ != nullptr && airLossValue_ != nullptr && outputDbValue_ != nullptr &&
            bypassValue_ != nullptr && bypassParameter_ != nullptr);

    structuralState_.setProperty("rendererMode", static_cast<int>(RendererMode::ModernHRTF), nullptr);
    structuralState_.setProperty("dataset", juce::String(defaultBuiltInDataset().stableId.data()), nullptr);
    structuralState_.setProperty("compensationMode", static_cast<int>(CompensationMode::DatasetCorrected), nullptr);
}

SpaceTraceAudioProcessor::~SpaceTraceAudioProcessor() {
    // Be defensive about hosts removing the plug-in while transport is still
    // running. suspendProcessing() takes JUCE's processor callback lock before
    // setting the suspended flag, so an in-flight processBlock finishes before
    // we clear DSP histories and no new wrapper callback should enter while the
    // object is being destroyed.
    suspendProcessing(true);
    releaseResources();
}

juce::AudioProcessorValueTreeState::ParameterLayout SpaceTraceAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto azAttrs = juce::AudioParameterFloatAttributes()
        .withLabel("degrees")
        .withStringFromValueFunction([](float value, int) { return azimuthText(value); })
        .withValueFromStringFunction([](const juce::String& text) { return parseNumber(text); });
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(paramAzimuth, 1), "Azimuth",
        juce::NormalisableRange<float>(0.0f, 360.0f, 1.0f), 0.0f, azAttrs));

    auto elAttrs = juce::AudioParameterFloatAttributes()
        .withLabel("degrees")
        .withStringFromValueFunction([](float value, int) { return juce::String(value, 0) + " degrees"; })
        .withValueFromStringFunction([](const juce::String& text) { return parseNumber(text); });
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(paramElevation, 1), "Elevation",
        juce::NormalisableRange<float>(-90.0f, 90.0f, 1.0f), 0.0f, elAttrs));

    auto distanceRange = juce::NormalisableRange<float>(0.25f, 20.0f, 0.01f);
    distanceRange.setSkewForCentre(2.0f);
    auto distanceAttrs = juce::AudioParameterFloatAttributes()
        .withLabel("metres")
        .withStringFromValueFunction([](float value, int) { return juce::String(value, 2) + " metres"; })
        .withValueFromStringFunction([](const juce::String& text) { return parseNumber(text); });
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(paramDistance, 1), "Distance", distanceRange, 1.0f, distanceAttrs));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID(paramInputMode, 1), "Input Mode",
        juce::StringArray {"Mono Point Source", "Stereo Pair"}, 0));

    auto widthAttrs = juce::AudioParameterFloatAttributes()
        .withLabel("degrees")
        .withStringFromValueFunction([](float value, int) { return juce::String(value, 0) + " degrees"; })
        .withValueFromStringFunction([](const juce::String& text) { return parseNumber(text); });
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(paramWidthOffset, 1), "Stereo Pair Width Offset",
        juce::NormalisableRange<float>(-45.0f, 45.0f, 1.0f), 0.0f, widthAttrs));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(paramAirLoss, 1), "Air Loss", true));

    auto gainAttrs = juce::AudioParameterFloatAttributes()
        .withLabel("dB")
        .withStringFromValueFunction([](float value, int) { return juce::String(value, 1) + " dB"; })
        .withValueFromStringFunction([](const juce::String& text) { return parseNumber(text); });
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID(paramOutputDb, 1), "Output Level",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f, gainAttrs));

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID(paramBypass, 1), "Bypass", false));

    return layout;
}

juce::AudioProcessorParameter* SpaceTraceAudioProcessor::getBypassParameter() const {
    return bypassParameter_;
}

std::size_t SpaceTraceAudioProcessor::requiredKernelLength(const HRTFSet& set) {
    std::size_t n = 2;
    for (const auto& m : set.measurements()) {
        const auto maxDelay = std::max(m.leftDelaySamples, m.rightDelaySamples);
        const auto delaySamples = static_cast<std::size_t>(std::ceil(std::max(0.0, maxDelay)));
        n = std::max(n, m.left.size() + delaySamples + 1u);
    }
    return n;
}

bool SpaceTraceAudioProcessor::prepareStructuralState(bool suspendHost) {
    if (suspendHost && !prepared_.load(std::memory_order_acquire)) return false;

    const int requestedDataset = juce::jlimit(0, static_cast<int>(builtInDatasetCount) - 1,
                                               selectedDatasetIndex_.load(std::memory_order_relaxed));
    const auto requestedRenderer = rendererMode();
    const auto requestedMode = compensationMode();
    juce::String error;

    if (requestedRenderer == RendererMode::ModernHRTF) {
        const juce::ScopedLock lock(stateLock_);
        if (unresolvedHeadId_.isNotEmpty()) {
            rendererReady_.store(false, std::memory_order_release);
            status_ = "Referenced head is unavailable: " + unresolvedHeadId_ +
                      ". Select an installed SpaceTrace head to continue.";
            return false;
        }
    }

    std::shared_ptr<const PreparedHeadData> modernHead;
    std::shared_ptr<const PreparedHeadData> papaHead;
    if (requestedRenderer == RendererMode::ModernHRTF) {
        modernHead = HeadRepository::instance().prepare(datasetId(requestedDataset), currentSampleRate_, error);
        if (!modernHead) {
            rendererReady_.store(false, std::memory_order_release);
            const juce::ScopedLock lock(stateLock_);
            status_ = "Head unavailable: " + error;
            return false;
        }
    } else {
        papaHead = HeadRepository::instance().prepare(BuiltInDatasetId::IrcamListen1050, currentSampleRate_, error);
        if (!papaHead) {
            rendererReady_.store(false, std::memory_order_release);
            const juce::ScopedLock lock(stateLock_);
            status_ = "Papa Pan unavailable: " + error;
            return false;
        }
    }

    const auto& correctionHead = requestedRenderer == RendererMode::PapaPan ? papaHead : modernHead;
    CompensationProfile selected;
    bool haveIR = false;
    if (requestedMode == CompensationMode::DatasetCorrected) {
        if (correctionHead && correctionHead->hasCompensation) {
            selected = correctionHead->compensation;
            haveIR = true;
        }
    } else if (requestedMode == CompensationMode::CustomIR) {
        const juce::ScopedLock lock(stateLock_);
        if (!customCompensation_.impulse.empty()) {
            selected = customCompensation_;
            haveIR = true;
        }
    }

    if (suspendHost) suspendProcessing(true);

    rendererReady_.store(false, std::memory_order_release);
    hrtfRenderer_.reset();
    stereoPairRenderer_.reset();
    papaPanRenderer_.reset();
    commonCompensation_.reset();
    stereoPairLeftCompensation_.reset();
    stereoPairRightCompensation_.reset();
    correctionReady_.store(false, std::memory_order_release);
    stereoPairCorrectionReady_.store(false, std::memory_order_release);

    if (requestedRenderer == RendererMode::ModernHRTF) {
        activeModernHead_ = modernHead;
        const auto maxIR = requiredKernelLength(activeModernHead_->hrtf);
        hrtfRenderer_.prepare(maxIR, currentSampleRate_, 20.0);
        stereoPairRenderer_.prepare(maxIR, currentSampleRate_, 20.0);
        hrtfRenderer_.setHRTF(&activeModernHead_->hrtf);
        stereoPairRenderer_.setHRTF(&activeModernHead_->hrtf);
    } else {
        activePapaHead_ = papaHead;
        const auto maxIR = requiredKernelLength(activePapaHead_->hrtf);
        if (!papaPanRenderer_.prepare(activePapaHead_->hrtf, maxIR, currentSampleRate_)) {
            if (suspendHost) suspendProcessing(false);
            const juce::ScopedLock lock(stateLock_);
            status_ = "Papa Pan unavailable: IRCAM LISTEN 1050 could not be prepared.";
            return false;
        }
    }

    if (haveIR) {
        auto irBuffer = monoIRBuffer(selected);
        commonCompensation_.loadImpulseResponse(std::move(irBuffer), selected.sampleRate,
                                                 juce::dsp::Convolution::Stereo::no,
                                                 juce::dsp::Convolution::Trim::no,
                                                 juce::dsp::Convolution::Normalise::no);
        commonCompensation_.prepare({currentSampleRate_, static_cast<juce::uint32>(currentMaxBlock_), 2});
        correctionReady_.store(true, std::memory_order_release);
    }

    if (requestedRenderer == RendererMode::ModernHRTF &&
        requestedMode == CompensationMode::DatasetCorrected &&
        activeModernHead_ && activeModernHead_->stereoPair.hasCompensation) {
        auto leftIR = monoIRBuffer(activeModernHead_->stereoPair.leftCompensation);
        auto rightIR = monoIRBuffer(activeModernHead_->stereoPair.rightCompensation);
        stereoPairLeftCompensation_.loadImpulseResponse(std::move(leftIR),
            activeModernHead_->stereoPair.leftCompensation.sampleRate,
            juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::no,
            juce::dsp::Convolution::Normalise::no);
        stereoPairRightCompensation_.loadImpulseResponse(std::move(rightIR),
            activeModernHead_->stereoPair.rightCompensation.sampleRate,
            juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::no,
            juce::dsp::Convolution::Normalise::no);
        const juce::dsp::ProcessSpec spec {currentSampleRate_, static_cast<juce::uint32>(currentMaxBlock_), 2};
        stereoPairLeftCompensation_.prepare(spec);
        stereoPairRightCompensation_.prepare(spec);
        stereoPairCorrectionReady_.store(true, std::memory_order_release);
    }

    const auto effectiveMode = (requestedMode == CompensationMode::Raw || haveIR)
        ? requestedMode : CompensationMode::Raw;
    activeDatasetIndex_.store(requestedDataset, std::memory_order_release);
    activeRendererModeIndex_.store(static_cast<int>(requestedRenderer), std::memory_order_release);
    activeCompensationModeIndex_.store(static_cast<int>(effectiveMode), std::memory_order_release);
    activeHeadTrimLinear_.store(requestedRenderer == RendererMode::ModernHRTF && activeModernHead_
        ? juce::Decibels::decibelsToGain(activeModernHead_->levelTrimDb) : 1.0f,
        std::memory_order_release);
    activeMonoTrimLinear_.store(requestedRenderer == RendererMode::ModernHRTF && activeModernHead_
        ? juce::Decibels::decibelsToGain(activeModernHead_->monoTrimDb) : 1.0f,
        std::memory_order_release);
    activeHeadLeftGainLinear_.store(requestedRenderer == RendererMode::ModernHRTF && activeModernHead_
        ? juce::Decibels::decibelsToGain(activeModernHead_->leftGainDb) : 1.0f,
        std::memory_order_release);
    activeHeadRightGainLinear_.store(requestedRenderer == RendererMode::ModernHRTF && activeModernHead_
        ? juce::Decibels::decibelsToGain(activeModernHead_->rightGainDb) : 1.0f,
        std::memory_order_release);
    activeStereoPairLeftGainLinear_.store(requestedRenderer == RendererMode::ModernHRTF && activeModernHead_
        ? juce::Decibels::decibelsToGain(activeModernHead_->stereoPair.leftGainDb) : 1.0f,
        std::memory_order_release);
    activeStereoPairRightGainLinear_.store(requestedRenderer == RendererMode::ModernHRTF && activeModernHead_
        ? juce::Decibels::decibelsToGain(activeModernHead_->stereoPair.rightGainDb) : 1.0f,
        std::memory_order_release);
    activeStereoPairTrimLinear_.store(requestedRenderer == RendererMode::ModernHRTF && activeModernHead_
        ? juce::Decibels::decibelsToGain(activeModernHead_->stereoPair.trimDb) : 1.0f,
        std::memory_order_release);
    rendererReady_.store(true, std::memory_order_release);

    if (suspendHost) suspendProcessing(false);

    const juce::ScopedLock lock(stateLock_);
    if (requestedRenderer == RendererMode::PapaPan) {
        if (requestedMode == CompensationMode::Raw)
            status_ = "Papa Pan Raw: historical 24-direction horizontal renderer";
        else if (haveIR)
            status_ = requestedMode == CompensationMode::DatasetCorrected
                ? "Papa Pan Corrected: IRCAM global tonal restoration"
                : "Papa Pan: Custom Correction IR active";
        else
            status_ = "Papa Pan correction unavailable; using Raw";
    } else if (requestedMode == CompensationMode::DatasetCorrected && !haveIR) {
        status_ = correctionHead && correctionHead->id == BuiltInDatasetId::FabianHato0
            ? "FABIAN HATO 0 correction is unavailable; using Raw"
            : "Selected compensation is unavailable; using Raw";
    } else if (restoredHeadHash_.isNotEmpty() && restoredHeadHash_ != activeModernHead_->headHash) {
        status_ = "Loaded " + activeModernHead_->displayName + " by stable ID; installed head data differs from the project-saved hash.";
    } else if (requestedMode == CompensationMode::Raw) {
        status_ = "Raw HRTF: dataset compensation bypassed";
    } else if (haveIR) {
        if (requestedMode == CompensationMode::DatasetCorrected && activeModernHead_ && activeModernHead_->stereoPair.hasCompensation)
            status_ = "Dataset Corrected: Stereo Pair angle corrections available";
        else
            status_ = requestedMode == CompensationMode::DatasetCorrected ? "Dataset Corrected" : "Custom Correction IR active";
    } else {
        status_ = activeModernHead_->displayName + " ready";
    }
    return true;
}

void SpaceTraceAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    currentSampleRate_ = sampleRate;
    currentMaxBlock_ = std::max(1, samplesPerBlock);

    sourceScratch_.setSize(2, currentMaxBlock_, false, false, true);
    stereoPairScratch_.setSize(2, currentMaxBlock_, false, false, true);
    outputGain_.reset(sampleRate, 0.02);
    distanceGain_.reset(sampleRate, 0.05);
    distanceAirHighGain_.reset(sampleRate, 0.05);
    distanceModel_.prepare(sampleRate);
    outputGain_.setCurrentAndTargetValue(1.0f);
    distanceGain_.setCurrentAndTargetValue(1.0f);
    distanceAirHighGain_.setCurrentAndTargetValue(1.0f);
    wasBypassed_ = false;
    wasStereoPairMode_ = false;

    prepared_.store(true, std::memory_order_release);
    prepareStructuralState(false);
}

void SpaceTraceAudioProcessor::releaseResources() {
    prepared_.store(false, std::memory_order_release);
    rendererReady_.store(false, std::memory_order_release);
    commonCompensation_.reset();
    stereoPairLeftCompensation_.reset();
    stereoPairRightCompensation_.reset();
    stereoPairCorrectionReady_.store(false, std::memory_order_release);
    distanceModel_.reset();
    hrtfRenderer_.reset();
    stereoPairRenderer_.reset();
    papaPanRenderer_.reset();
    activeModernHead_.reset();
    activePapaHead_.reset();
}

bool SpaceTraceAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void SpaceTraceAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();
    const auto inChannels = getTotalNumInputChannels();
    if (buffer.getNumChannels() < 2 || inChannels < 1) {
        buffer.clear();
        return;
    }

    const bool bypassed = bypassValue_->load(std::memory_order_relaxed) >= 0.5f;
    if (bypassed) {
        if (!wasBypassed_) {
            hrtfRenderer_.reset();
            stereoPairRenderer_.reset();
            papaPanRenderer_.reset();
            commonCompensation_.reset();
            stereoPairLeftCompensation_.reset();
            stereoPairRightCompensation_.reset();
            distanceModel_.reset();
            wasStereoPairMode_ = false;
            wasBypassed_ = true;
        }
        if (inChannels == 1)
            buffer.copyFrom(1, 0, buffer, 0, 0, numSamples);
        return;
    }
    wasBypassed_ = false;

    if (!rendererReady_.load(std::memory_order_acquire)) {
        buffer.clear();
        return;
    }

    const int activeRenderer = activeRendererModeIndex_.load(std::memory_order_acquire);
    const bool papaMode = activeRenderer == static_cast<int>(RendererMode::PapaPan);
    const bool stereoPairMode = !papaMode && inChannels > 1 &&
        inputModeValue_->load(std::memory_order_relaxed) >= 0.5f;

    // Mono Point Source and Stereo Pair give the renderer histories different
    // source-channel meanings. Never carry an old convolution tail across that
    // topology boundary when the discrete host parameter changes. reset() is
    // allocation-free once the renderer has been prepared.
    if (stereoPairMode != wasStereoPairMode_) {
        hrtfRenderer_.reset();
        stereoPairRenderer_.reset();
        stereoPairLeftCompensation_.reset();
        stereoPairRightCompensation_.reset();
        commonCompensation_.reset();
        wasStereoPairMode_ = stereoPairMode;
    }

    const float az = azimuthValue_->load(std::memory_order_relaxed);
    const float el = elevationValue_->load(std::memory_order_relaxed);
    const float distance = distanceValue_->load(std::memory_order_relaxed);
    if (papaMode)
        papaPanRenderer_.setAzimuth(static_cast<double>(az));
    else if (stereoPairMode) {
        const auto geometry = stereoPairGeometry(static_cast<double>(az),
                                                 static_cast<double>(widthOffsetValue_->load(std::memory_order_relaxed)));
        hrtfRenderer_.setPosition({geometry.leftAzimuthDegrees, static_cast<double>(el), 1.0});
        stereoPairRenderer_.setPosition({geometry.rightAzimuthDegrees, static_cast<double>(el), 1.0});
    } else {
        hrtfRenderer_.setPosition({wrapAzimuthDegrees(static_cast<double>(az)), static_cast<double>(el), 1.0});
    }

    outputGain_.setTargetValue(juce::Decibels::decibelsToGain(
        outputDbValue_->load(std::memory_order_relaxed)));
    distanceGain_.setTargetValue(DistanceModel::levelGain(distance));
    distanceAirHighGain_.setTargetValue(airLossValue_->load(std::memory_order_relaxed) >= 0.5f
        ? DistanceModel::airHighGain(distance) : 1.0f);

    int offset = 0;
    while (offset < numSamples) {
        const int n = juce::jmin(currentMaxBlock_, numSamples - offset);
        auto* primarySource = sourceScratch_.getWritePointer(0);
        auto* secondarySource = sourceScratch_.getWritePointer(1);
        const auto* inL = buffer.getReadPointer(0, offset);
        const auto* inR = inChannels > 1 ? buffer.getReadPointer(1, offset) : nullptr;
        if (stereoPairMode) {
            std::copy(inL, inL + n, primarySource);
            std::copy(inR, inR + n, secondarySource);
        } else if (inR != nullptr) {
            for (int i = 0; i < n; ++i) primarySource[i] = 0.5f * (inL[i] + inR[i]);
        } else {
            std::copy(inL, inL + n, primarySource);
        }

        auto* outL = buffer.getWritePointer(0, offset);
        auto* outR = buffer.getWritePointer(1, offset);
        if (papaMode)
            papaPanRenderer_.process(primarySource, outL, outR, static_cast<std::size_t>(n));
        else if (stereoPairMode) {
            auto* pairL = stereoPairScratch_.getWritePointer(0);
            auto* pairR = stereoPairScratch_.getWritePointer(1);
            hrtfRenderer_.process(primarySource, outL, outR, static_cast<std::size_t>(n));
            stereoPairRenderer_.process(secondarySource, pairL, pairR, static_cast<std::size_t>(n));

            const auto activeModeRaw = activeCompensationModeIndex_.load(std::memory_order_acquire);
            const bool useStereoPairCorrection =
                activeModeRaw == static_cast<int>(CompensationMode::DatasetCorrected) &&
                stereoPairCorrectionReady_.load(std::memory_order_acquire);
            if (useStereoPairCorrection) {
                juce::dsp::AudioBlock<float> primaryBlock(buffer);
                auto primarySubBlock = primaryBlock.getSubBlock(static_cast<std::size_t>(offset), static_cast<std::size_t>(n));
                juce::dsp::ProcessContextReplacing<float> primaryContext(primarySubBlock);
                stereoPairLeftCompensation_.process(primaryContext);

                juce::dsp::AudioBlock<float> secondaryBlock(stereoPairScratch_);
                auto secondarySubBlock = secondaryBlock.getSubBlock(0, static_cast<std::size_t>(n));
                juce::dsp::ProcessContextReplacing<float> secondaryContext(secondarySubBlock);
                stereoPairRightCompensation_.process(secondaryContext);
            }

            const float pairLeftGain = activeStereoPairLeftGainLinear_.load(std::memory_order_relaxed);
            const float pairRightGain = activeStereoPairRightGainLinear_.load(std::memory_order_relaxed);
            for (int i = 0; i < n; ++i) {
                outL[i] = outL[i] * pairLeftGain + pairL[i] * pairRightGain;
                outR[i] = outR[i] * pairLeftGain + pairR[i] * pairRightGain;
            }
        } else
            hrtfRenderer_.process(primarySource, outL, outR, static_cast<std::size_t>(n));
        offset += n;
    }

    const auto activeModeRaw = activeCompensationModeIndex_.load(std::memory_order_acquire);
    const auto activeMode = activeModeRaw == static_cast<int>(CompensationMode::CustomIR)
        ? CompensationMode::CustomIR
        : activeModeRaw == static_cast<int>(CompensationMode::DatasetCorrected)
            ? CompensationMode::DatasetCorrected
            : CompensationMode::Raw;
    const bool usedStereoPairCorrection = stereoPairMode &&
        activeMode == CompensationMode::DatasetCorrected &&
        stereoPairCorrectionReady_.load(std::memory_order_acquire);
    if (!usedStereoPairCorrection &&
        activeMode != CompensationMode::Raw && correctionReady_.load(std::memory_order_acquire)) {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        commonCompensation_.process(context);
    }

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);
    const float headTrim = activeHeadTrimLinear_.load(std::memory_order_relaxed);
    const float headLeft = activeHeadLeftGainLinear_.load(std::memory_order_relaxed);
    const float headRight = activeHeadRightGainLinear_.load(std::memory_order_relaxed);
    // One fixed level calibration per input topology; keep correction shape,
    // ear balance, and the stereo source gains independent of this reference.
    const float modeTrim = stereoPairMode
        ? activeStereoPairTrimLinear_.load(std::memory_order_relaxed)
        : activeMonoTrimLinear_.load(std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i) {
        const float air = distanceAirHighGain_.getNextValue();
        left[i] = distanceModel_.processSample(0, left[i], air);
        right[i] = distanceModel_.processSample(1, right[i], air);
        const float g = outputGain_.getNextValue() * distanceGain_.getNextValue() * headTrim * modeTrim;
        left[i] *= g * headLeft;
        right[i] *= g * headRight;
    }
}

RendererMode SpaceTraceAudioProcessor::rendererMode() const noexcept {
    return rendererModeIndex_.load(std::memory_order_relaxed) == static_cast<int>(RendererMode::PapaPan)
        ? RendererMode::PapaPan : RendererMode::ModernHRTF;
}

InputMode SpaceTraceAudioProcessor::inputMode() const noexcept {
    return inputModeValue_ != nullptr && inputModeValue_->load(std::memory_order_relaxed) >= 0.5f
        ? InputMode::StereoPair : InputMode::MonoPointSource;
}

void SpaceTraceAudioProcessor::setRendererMode(RendererMode mode) {
    const auto value = static_cast<int>(mode);
    if (rendererModeIndex_.exchange(value) == value) return;
    updateStructuralStateProperties();
    prepareStructuralState(true);
}

BuiltInDatasetId SpaceTraceAudioProcessor::selectedBuiltInDataset() const noexcept {
    return datasetId(selectedDatasetIndex_.load(std::memory_order_relaxed));
}

juce::String SpaceTraceAudioProcessor::selectedDatasetName() const {
    return juce::String::fromUTF8(datasetDescriptor(selectedBuiltInDataset()).displayName.data());
}

void SpaceTraceAudioProcessor::selectBuiltInDataset(BuiltInDatasetId id) {
    const auto index = datasetIndex(id);
    const bool sameIndex = selectedDatasetIndex_.exchange(index) == index;
    {
        const juce::ScopedLock lock(stateLock_);
        unresolvedHeadId_.clear();
        restoredHeadHash_.clear();
    }
    if (sameIndex && prepared_.load(std::memory_order_acquire) && rendererReady_.load(std::memory_order_acquire)) return;
    updateStructuralStateProperties();
    prepareStructuralState(true);
}

CompensationMode SpaceTraceAudioProcessor::compensationMode() const noexcept {
    const auto raw = compensationModeIndex_.load(std::memory_order_relaxed);
    if (raw == static_cast<int>(CompensationMode::Raw)) return CompensationMode::Raw;
    if (raw == static_cast<int>(CompensationMode::CustomIR)) return CompensationMode::CustomIR;
    return CompensationMode::DatasetCorrected;
}

void SpaceTraceAudioProcessor::setCompensationMode(CompensationMode mode) {
    const auto value = static_cast<int>(mode);
    if (compensationModeIndex_.exchange(value) == value) return;
    updateStructuralStateProperties();
    prepareStructuralState(true);
}

bool SpaceTraceAudioProcessor::loadCustomCompensationIR(const juce::File& file, juce::String& error) {
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader) {
        error = "The selected file is not a readable audio file.";
        return false;
    }
    const double durationSeconds = reader->sampleRate > 0.0
        ? static_cast<double>(reader->lengthInSamples) / reader->sampleRate
        : 0.0;
    if (reader->numChannels < 1 || reader->numChannels > 2 || reader->lengthInSamples <= 0 ||
        durationSeconds <= 0.0 || durationSeconds > maxCustomCompensationSeconds) {
        error = "Custom compensation must be mono or identical stereo and no longer than 250 milliseconds.";
        return false;
    }

    juce::AudioBuffer<float> audio(static_cast<int>(reader->numChannels),
                                   static_cast<int>(reader->lengthInSamples));
    if (!reader->read(&audio, 0, audio.getNumSamples(), 0, true, true)) {
        error = "The audio file could not be read completely.";
        return false;
    }

    if (audio.getNumChannels() == 2) {
        const auto* a = audio.getReadPointer(0);
        const auto* b = audio.getReadPointer(1);
        for (int i = 0; i < audio.getNumSamples(); ++i) {
            if (std::abs(a[i] - b[i]) > 1.0e-5f) {
                error = "Custom compensation must be mono or identical stereo so binaural cues are not altered.";
                return false;
            }
        }
    }

    CompensationProfile profile;
    profile.name = file.getFileNameWithoutExtension().toStdString();
    profile.version = "custom";
    profile.sampleRate = reader->sampleRate;
    profile.impulse.assign(audio.getReadPointer(0), audio.getReadPointer(0) + audio.getNumSamples());
    std::string reason;
    if (!profile.valid(&reason)) {
        error = juce::String(reason);
        return false;
    }

    {
        const juce::ScopedLock lock(stateLock_);
        customCompensation_ = std::move(profile);
        status_ = "Loaded custom compensation: " + file.getFileName();
    }
    updateStructuralStateProperties();
    if (compensationMode() == CompensationMode::CustomIR)
        prepareStructuralState(true);
    else
        setCompensationMode(CompensationMode::CustomIR);
    error.clear();
    return true;
}

void SpaceTraceAudioProcessor::updateStructuralStateProperties() {
    const juce::ScopedLock lock(stateLock_);
    structuralState_.setProperty("rendererMode", static_cast<int>(rendererMode()), nullptr);
    structuralState_.setProperty("dataset", unresolvedHeadId_.isNotEmpty()
        ? unresolvedHeadId_
        : juce::String(datasetDescriptor(selectedBuiltInDataset()).stableId.data()), nullptr);
    juce::String headHash;
    if (unresolvedHeadId_.isNotEmpty())
        headHash = restoredHeadHash_;
    else if (activeModernHead_ && activeModernHead_->id == selectedBuiltInDataset())
        headHash = activeModernHead_->headHash;
    else {
        juce::String hashError;
        headHash = HeadRepository::instance().contentHash(selectedBuiltInDataset(), hashError);
    }
    if (headHash.isNotEmpty()) structuralState_.setProperty("headHash", headHash, nullptr);
    else structuralState_.removeProperty("headHash", nullptr);
    structuralState_.setProperty("compensationMode", static_cast<int>(compensationMode()), nullptr);
    const auto custom = encodeCustomIR(customCompensation_);
    if (custom.getSize() > 0)
        structuralState_.setProperty("customCompensation", juce::var(custom), nullptr);
    else
        structuralState_.removeProperty("customCompensation", nullptr);
}

void SpaceTraceAudioProcessor::restoreStructuralStateProperties() {
    const juce::ScopedLock lock(stateLock_);
    auto renderer = static_cast<int>(structuralState_.getProperty(
        "rendererMode", static_cast<int>(RendererMode::ModernHRTF)));
    if (renderer < static_cast<int>(RendererMode::ModernHRTF) || renderer > static_cast<int>(RendererMode::PapaPan))
        renderer = static_cast<int>(RendererMode::ModernHRTF);
    rendererModeIndex_.store(renderer, std::memory_order_relaxed);

    const auto dataset = structuralState_.getProperty("dataset").toString();
    int restoredDataset = ircamIndex;
    bool knownDataset = dataset.isEmpty() ||
        dataset == juce::String(datasetDescriptor(BuiltInDatasetId::IrcamListen1050).stableId.data());
    if (dataset == juce::String(datasetDescriptor(BuiltInDatasetId::MitKemarNormalPinna).stableId.data())) {
        restoredDataset = kemarIndex;
        knownDataset = true;
    } else if (dataset == juce::String(datasetDescriptor(BuiltInDatasetId::Sadie2D1Ku100).stableId.data())) {
        restoredDataset = ku100Index;
        knownDataset = true;
    } else if (dataset == juce::String(datasetDescriptor(BuiltInDatasetId::ThkKu100Full2Deg).stableId.data())) {
        restoredDataset = full2DegIndex;
        knownDataset = true;
    } else if (dataset == juce::String(datasetDescriptor(BuiltInDatasetId::FabianHato0).stableId.data())) {
        restoredDataset = fabianIndex;
        knownDataset = true;
    }
    selectedDatasetIndex_.store(restoredDataset, std::memory_order_relaxed);
    unresolvedHeadId_ = knownDataset ? juce::String() : dataset;
    restoredHeadHash_ = structuralState_.getProperty("headHash").toString();

    auto mode = static_cast<int>(structuralState_.getProperty(
        "compensationMode", static_cast<int>(CompensationMode::DatasetCorrected)));
    if (mode < static_cast<int>(CompensationMode::Raw) || mode > static_cast<int>(CompensationMode::CustomIR))
        mode = static_cast<int>(CompensationMode::DatasetCorrected);
    compensationModeIndex_.store(mode, std::memory_order_relaxed);

    customCompensation_ = {};
    const auto binary = structuralState_.getProperty("customCompensation");
    if (const auto* data = binary.getBinaryData())
        decodeCustomIR(*data, customCompensation_);
}

void SpaceTraceAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    updateStructuralStateProperties();
    juce::ValueTree root("SpaceTraceState");
    root.setProperty("schemaVersion", stateSchemaVersion, nullptr);
    root.addChild(parameters_.copyState(), -1, nullptr);
    {
        const juce::ScopedLock lock(stateLock_);
        root.addChild(structuralState_.createCopy(), -1, nullptr);
    }
    if (auto xml = root.createXml()) copyXmlToBinary(*xml, destData);
}

void SpaceTraceAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (!xml) return;
    const auto root = juce::ValueTree::fromXml(*xml);
    if (!root.isValid() || !root.hasType("SpaceTraceState")) return;
    const auto schemaVersion = static_cast<int>(root.getProperty("schemaVersion", stateSchemaVersion));
    if (schemaVersion < 1 || schemaVersion > stateSchemaVersion) return;

    const auto params = root.getChildWithName(parameters_.state.getType());
    if (params.isValid()) parameters_.replaceState(params);
    if (schemaVersion < 5) {
        if (auto* width = parameters_.getParameter(paramWidthOffset))
            width->setValueNotifyingHost(width->convertTo0to1(0.0f));
    }

    const auto structural = root.getChildWithName("SpaceTraceStructuralState");
    if (structural.isValid()) {
        {
            const juce::ScopedLock lock(stateLock_);
            structuralState_ = structural.createCopy();
        }
        restoreStructuralStateProperties();
        prepareStructuralState(true);
    }
}

juce::String SpaceTraceAudioProcessor::statusText() const {
    const juce::ScopedLock lock(stateLock_);
    return status_;
}

juce::AudioProcessorEditor* SpaceTraceAudioProcessor::createEditor() {
    return new SpaceTraceAudioProcessorEditor(*this);
}

} // namespace spacetrace::plugin

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new spacetrace::plugin::SpaceTraceAudioProcessor();
}
