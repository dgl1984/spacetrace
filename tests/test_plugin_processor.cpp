#include "PluginProcessor.h"
#include "HeadRepository.h"
#include "AccessibleComboBox.h"
#include "AccessibleParameterSlider.h"
#include "spacetrace/Resample.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>

#include <cmath>
#include <complex>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>

using spacetrace::BuiltInDatasetId;
using spacetrace::CompensationMode;
using spacetrace::plugin::InputMode;
using spacetrace::plugin::SpaceTraceAudioProcessor;
using spacetrace::plugin::RendererMode;
using spacetrace::plugin::HeadRepository;
using spacetrace::plugin::stereoPairGeometry;

namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void setParameter(SpaceTraceAudioProcessor& p, const char* id, float plainValue) {
    auto* parameter = p.parameters().getParameter(id);
    require(parameter != nullptr, "missing plugin parameter");
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

juce::File writeMonoImpulse(double seconds, const juce::String& stem, float gain = 1.0f) {
    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile(stem, ".wav", false);
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    require(stream != nullptr, "could not create temporary WAV");

    juce::WavAudioFormat wav;
    auto options = juce::AudioFormatWriter::Options{}
        .withSampleRate(44100.0)
        .withChannelLayout(juce::AudioChannelSet::mono())
        .withBitsPerSample(24);
    auto writer = wav.createWriterFor(stream, options);
    require(writer != nullptr, "could not create WAV writer");

    const int samples = static_cast<int>(std::round(seconds * 44100.0));
    juce::AudioBuffer<float> audio(1, samples);
    audio.clear();
    if (samples > 0) audio.setSample(0, 0, gain);
    require(writer->writeFromAudioSampleBuffer(audio, 0, samples), "could not write temporary WAV");
    writer.reset();
    return file;
}

float maxAbs(const juce::AudioBuffer<float>& b) {
    float peak = 0.0f;
    for (int c = 0; c < b.getNumChannels(); ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
            peak = std::max(peak, std::abs(b.getSample(c, i)));
    return peak;
}
}

int main() {
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    try {
        // Use the production minimum-phase rate converter on the full combined
        // WAVs. Compare physical-frequency response with the stored 48 kHz IRs.
        {
            const auto heads = juce::File(juce::SystemStats::getEnvironmentVariable("SPACETRACE_HEADS_DIR", ""));
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            double worstDb = 0.0;
            for (const auto* folder : {"IRCAM_1050", "KU100_SADIE_D1", "KU100_FULL2DEG"}) {
                for (const auto* side : {"left", "right"}) {
                    const auto file = heads.getChildFile(folder).getChildFile(
                        juce::String("stereo_pair_") + side + "_correction.wav");
                    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
                    require(reader != nullptr && reader->lengthInSamples == 17279,
                            "combined Stereo Pair IR is missing or truncated");
                    juce::AudioBuffer<float> buffer(1, static_cast<int>(reader->lengthInSamples));
                    require(reader->read(&buffer, 0, buffer.getNumSamples(), 0, true, false),
                            "could not read combined Stereo Pair IR");
                    spacetrace::CompensationProfile original;
                    original.sampleRate = reader->sampleRate;
                    original.impulse.assign(buffer.getReadPointer(0), buffer.getReadPointer(0) + buffer.getNumSamples());
                    const auto magnitude = [](const spacetrace::CompensationProfile& profile, double hz) {
                        std::complex<double> sum{};
                        for (std::size_t n = 0; n < profile.impulse.size(); ++n)
                            sum += static_cast<double>(profile.impulse[n]) * std::polar(1.0,
                                -juce::MathConstants<double>::twoPi * hz * static_cast<double>(n) / profile.sampleRate);
                        return std::abs(sum);
                    };
                    for (const double rate : {44100.0, 48000.0, 88200.0, 96000.0}) {
                        const auto converted = spacetrace::resampleCompensation(original, rate);
                        require(converted.impulse.size() >= static_cast<std::size_t>(0.359 * rate),
                                "rate conversion truncated the combined correction tail");
                        for (int step = 0; step <= 48; ++step) {
                            const double hz = 20.0 * std::pow(1000.0, step / 48.0);
                            const double delta = std::abs(20.0 * std::log10(magnitude(converted, hz) / magnitude(original, hz)));
                            require(std::isfinite(delta) && delta < 0.05,
                                    "combined correction response changed during host-rate conversion");
                            worstDb = std::max(worstDb, delta);
                        }
                    }
                }
            }
            std::cout << "Combined correction maximum rate-conversion deviation: " << worstDb << " dB\n";
        }
        // Stereo Pair geometry is shared by DSP and the read-only display.
        // Width Offset bows both source positions symmetrically around the
        // canonical 90/270 geometry without introducing separate L/R controls.
        {
            // stereoPairGeometry returns SpaceTrace canonical internal azimuths
            // in [-180, 180). The UI may present their equivalent 0..360
            // values, e.g. canonical -90 is display 270.
            const auto centred = stereoPairGeometry(0.0, 0.0);
            require(std::abs(centred.leftAzimuthDegrees - 90.0) < 1.0e-9 &&
                    std::abs(centred.rightAzimuthDegrees + 90.0) < 1.0e-9,
                    "Stereo Pair zero-width canonical geometry changed");
            require(std::abs(spacetrace::azimuthForDisplay360(centred.leftAzimuthDegrees) - 90.0) < 1.0e-9 &&
                    std::abs(spacetrace::azimuthForDisplay360(centred.rightAzimuthDegrees) - 270.0) < 1.0e-9,
                    "Stereo Pair zero-width display geometry changed");

            const auto frontBow = stereoPairGeometry(0.0, -45.0);
            require(std::abs(frontBow.leftAzimuthDegrees - 45.0) < 1.0e-9 &&
                    std::abs(frontBow.rightAzimuthDegrees + 45.0) < 1.0e-9,
                    "Stereo Pair -45 degree canonical geometry is wrong");
            require(std::abs(spacetrace::azimuthForDisplay360(frontBow.leftAzimuthDegrees) - 45.0) < 1.0e-9 &&
                    std::abs(spacetrace::azimuthForDisplay360(frontBow.rightAzimuthDegrees) - 315.0) < 1.0e-9,
                    "Stereo Pair -45 degree display geometry is wrong");

            const auto rearBow = stereoPairGeometry(0.0, 45.0);
            require(std::abs(rearBow.leftAzimuthDegrees - 135.0) < 1.0e-9 &&
                    std::abs(rearBow.rightAzimuthDegrees + 135.0) < 1.0e-9,
                    "Stereo Pair +45 degree canonical geometry is wrong");
            require(std::abs(spacetrace::azimuthForDisplay360(rearBow.leftAzimuthDegrees) - 135.0) < 1.0e-9 &&
                    std::abs(spacetrace::azimuthForDisplay360(rearBow.rightAzimuthDegrees) - 225.0) < 1.0e-9,
                    "Stereo Pair +45 degree display geometry is wrong");

            const auto rotated = stereoPairGeometry(90.0, 0.0);
            require(std::abs(rotated.leftAzimuthDegrees + 180.0) < 1.0e-9 &&
                    std::abs(rotated.rightAzimuthDegrees - 0.0) < 1.0e-9,
                    "Stereo Pair azimuth rotation canonical geometry is wrong");
            require(std::abs(spacetrace::azimuthForDisplay360(rotated.leftAzimuthDegrees) - 180.0) < 1.0e-9 &&
                    std::abs(spacetrace::azimuthForDisplay360(rotated.rightAzimuthDegrees) - 0.0) < 1.0e-9,
                    "Stereo Pair azimuth rotation display geometry is wrong");
        }

        // Universal keyboard contract: JUCE 8.0.12 does not provide Home/End
        // for sliders or combo boxes, nor Page Up/Page Down for sliders.
        // SpaceTrace supplies those conventional keys without adding duplicate
        // controls or a screen-reader-specific speech path.
        spacetrace::plugin::AccessibleParameterSlider keyboardSlider;
        keyboardSlider.setRange(0.0, 360.0, 1.0);
        double exactValue = -1;
        for (int repeat = 0; repeat < 100; ++repeat) {
            require(keyboardSlider.parseExactValue("   123.5   ", exactValue) && exactValue == 123.5,
                    "trimmed exact-entry value failed to parse");
            for (const auto* invalid : {"", "   ", "nan", "inf", "12x", "-1", "361", "1e999"})
                require(!keyboardSlider.parseExactValue(invalid, exactValue), "invalid exact-entry value accepted");
        }
        keyboardSlider.setKeyboardPageStep(10.0);
        keyboardSlider.setValue(123.0, juce::dontSendNotification);
        require(keyboardSlider.keyPressed(juce::KeyPress(juce::KeyPress::homeKey)),
                "slider Home key was not handled");
        require(std::abs(keyboardSlider.getValue() - 0.0) < 0.001,
                "slider Home key did not select the minimum");
        require(keyboardSlider.keyPressed(juce::KeyPress(juce::KeyPress::endKey)),
                "slider End key was not handled");
        require(std::abs(keyboardSlider.getValue() - 360.0) < 0.001,
                "slider End key did not select the maximum");
        keyboardSlider.setValue(100.0, juce::dontSendNotification);
        require(keyboardSlider.keyPressed(juce::KeyPress(juce::KeyPress::pageUpKey)),
                "slider Page Up key was not handled");
        require(std::abs(keyboardSlider.getValue() - 110.0) < 0.001,
                "slider Page Up did not apply the coarse step");
        require(keyboardSlider.keyPressed(juce::KeyPress(juce::KeyPress::pageDownKey)),
                "slider Page Down key was not handled");
        require(std::abs(keyboardSlider.getValue() - 100.0) < 0.001,
                "slider Page Down did not apply the coarse step");

        spacetrace::plugin::AccessibleComboBox keyboardCombo;
        keyboardCombo.addItem("First", 1);
        keyboardCombo.addItem("Middle", 2);
        keyboardCombo.addItem("Last", 3);
        keyboardCombo.setSelectedId(2, juce::dontSendNotification);
        require(keyboardCombo.keyPressed(juce::KeyPress(juce::KeyPress::homeKey)),
                "combo Home key was not handled");
        require(keyboardCombo.getSelectedId() == 1,
                "combo Home key did not select the first item");
        require(keyboardCombo.keyPressed(juce::KeyPress(juce::KeyPress::endKey)),
                "combo End key was not handled");
        require(keyboardCombo.getSelectedId() == 3,
                "combo End key did not select the last item");

        // Process-wide prepared head data must be shared between instances at
        // the same sample rate. This is the key multi-instance startup path.
        juce::String cacheError;
        auto sharedA = HeadRepository::instance().prepare(BuiltInDatasetId::IrcamListen1050, 44100.0, cacheError);
        require(sharedA != nullptr, "external IRCAM head failed to prepare");
        auto sharedB = HeadRepository::instance().prepare(BuiltInDatasetId::IrcamListen1050, 44100.0, cacheError);
        require(sharedB != nullptr && sharedA.get() == sharedB.get(),
                "same-rate instances did not share immutable prepared head data");
        require(sharedA->hasCompensation, "IRCAM external correction is missing");
        require(sharedA->stereoPair.hasCompensation, "IRCAM Stereo Pair correction pair is missing");
        require(std::abs(sharedA->stereoPair.leftGainDb - (-1.570725f)) < 0.01f &&
                std::abs(sharedA->stereoPair.rightGainDb - 1.570725f) < 0.01f,
                "IRCAM Stereo Pair source-balance calibration changed unexpectedly");
        require(std::abs(sharedA->stereoPair.trimDb - (-0.452849f)) < 0.01f,
                "IRCAM Stereo Pair level calibration changed unexpectedly");

        auto kemar = HeadRepository::instance().prepare(BuiltInDatasetId::MitKemarNormalPinna, 48000.0, cacheError);
        require(kemar != nullptr && kemar->stereoPair.hasCompensation,
                "KEMAR Stereo Pair correction pair failed to prepare");
        require(std::abs(kemar->stereoPair.trimDb - (-4.373117f)) < 0.02f,
                "KEMAR Stereo Pair level calibration changed unexpectedly");

        auto full2Deg = HeadRepository::instance().prepare(BuiltInDatasetId::ThkKu100Full2Deg, 48000.0, cacheError);
        require(full2Deg != nullptr, "FULL2DEG external head failed to prepare");
        require(full2Deg->hasCompensation, "FULL2DEG approved external correction is missing");
        require(std::abs(full2Deg->levelTrimDb - (-4.13f)) < 0.02f, "FULL2DEG fixed level trim changed unexpectedly");
        require(std::abs(full2Deg->leftGainDb - 0.140995f) < 0.005f &&
                std::abs(full2Deg->rightGainDb - (-0.140995f)) < 0.005f,
                "FULL2DEG front-center channel calibration changed unexpectedly");

        auto fabian = HeadRepository::instance().prepare(BuiltInDatasetId::FabianHato0, 44100.0, cacheError);
        require(fabian != nullptr, "FABIAN external head failed to prepare");
        require(fabian->hasCompensation, "FABIAN approved external correction is missing");
        require(std::abs(fabian->levelTrimDb - (-9.74f)) < 0.01f, "FABIAN fixed level trim changed unexpectedly");
        require(std::abs(fabian->leftGainDb - 0.291073f) < 0.005f &&
                std::abs(fabian->rightGainDb - (-0.291073f)) < 0.005f,
                "FABIAN front-center channel calibration changed unexpectedly");

        SpaceTraceAudioProcessor p;
        require(p.rendererMode() == RendererMode::ModernHRTF,
                "Modern HRTF must be the default renderer");
        require(p.selectedBuiltInDataset() == BuiltInDatasetId::IrcamListen1050,
                "IRCAM LISTEN 1050 must be the plugin default");
        require(p.compensationMode() == CompensationMode::DatasetCorrected,
                "Dataset Corrected must be the plugin default");
        require(p.inputMode() == InputMode::MonoPointSource,
                "Mono Point Source must be the default input mode");
        require(p.parameters().getRawParameterValue(SpaceTraceAudioProcessor::paramAirLoss)->load() >= 0.5f,
                "Air Loss must default on");
        require(p.getParameters().size() == 8,
                "SpaceTrace must expose exactly eight automatable parameters");
        require(p.getTailLengthSeconds() >= 0.38, "host tail is too short for composed stereo correction FIRs");
        require(p.getBypassParameter() == p.parameters().getParameter(SpaceTraceAudioProcessor::paramBypass),
                "host bypass is not the existing SpaceTrace bypass parameter");

        // Exercise the real external IRCAM package in Raw mode. In SpaceTrace's
        // coordinate convention +90 degrees is listener-left.
        p.setCompensationMode(CompensationMode::Raw);
        setParameter(p, SpaceTraceAudioProcessor::paramAzimuth, 90.0f);
        setParameter(p, SpaceTraceAudioProcessor::paramElevation, 0.0f);
        setParameter(p, SpaceTraceAudioProcessor::paramDistance, 1.0f);
        p.prepareToPlay(44100.0, 512);

        juce::AudioBuffer<float> impulse(2, 512);
        impulse.clear();
        impulse.setSample(0, 0, 1.0f);
        impulse.setSample(1, 0, 1.0f);
        juce::MidiBuffer midi;
        p.processBlock(impulse, midi);
        const auto leftEnergy = impulse.getRMSLevel(0, 0, impulse.getNumSamples());
        const auto rightEnergy = impulse.getRMSLevel(1, 0, impulse.getNumSamples());
        require(leftEnergy > rightEnergy * 1.5f, "+90 IRCAM render must favor the listener-left ear");

        // Entering bypass must clear wet histories so re-enabling does not
        // resurrect a tail captured before bypass.
        setParameter(p, SpaceTraceAudioProcessor::paramBypass, 1.0f);
        juce::AudioBuffer<float> silence(2, 512);
        silence.clear();
        p.processBlock(silence, midi);
        setParameter(p, SpaceTraceAudioProcessor::paramBypass, 0.0f);
        silence.clear();
        p.processBlock(silence, midi);
        require(maxAbs(silence) < 1.0e-6f, "bypass release resurrected stale wet history");
        p.releaseResources();

        // Modern Stereo Pair maps input L to center+90 and input R to center-90.
        // At center 0 this means listener-left and listener-right respectively.
        SpaceTraceAudioProcessor stereoLeft;
        stereoLeft.setCompensationMode(CompensationMode::Raw);
        setParameter(stereoLeft, SpaceTraceAudioProcessor::paramInputMode, 1.0f);
        setParameter(stereoLeft, SpaceTraceAudioProcessor::paramAzimuth, 0.0f);
        setParameter(stereoLeft, SpaceTraceAudioProcessor::paramElevation, 0.0f);
        setParameter(stereoLeft, SpaceTraceAudioProcessor::paramDistance, 1.0f);
        stereoLeft.prepareToPlay(44100.0, 512);
        juce::AudioBuffer<float> stereoLeftImpulse(2, 512);
        stereoLeftImpulse.clear();
        stereoLeftImpulse.setSample(0, 0, 1.0f);
        stereoLeft.processBlock(stereoLeftImpulse, midi);
        require(stereoLeftImpulse.getRMSLevel(0, 0, 512) > stereoLeftImpulse.getRMSLevel(1, 0, 512) * 1.5f,
                "Stereo Pair input Left at center 0 did not map to listener-left");
        stereoLeft.releaseResources();

        SpaceTraceAudioProcessor stereoRight;
        stereoRight.setCompensationMode(CompensationMode::Raw);
        setParameter(stereoRight, SpaceTraceAudioProcessor::paramInputMode, 1.0f);
        setParameter(stereoRight, SpaceTraceAudioProcessor::paramAzimuth, 0.0f);
        setParameter(stereoRight, SpaceTraceAudioProcessor::paramElevation, 0.0f);
        setParameter(stereoRight, SpaceTraceAudioProcessor::paramDistance, 1.0f);
        stereoRight.prepareToPlay(44100.0, 512);
        juce::AudioBuffer<float> stereoRightImpulse(2, 512);
        stereoRightImpulse.clear();
        stereoRightImpulse.setSample(1, 0, 1.0f);
        stereoRight.processBlock(stereoRightImpulse, midi);
        require(stereoRightImpulse.getRMSLevel(1, 0, 512) > stereoRightImpulse.getRMSLevel(0, 0, 512) * 1.5f,
                "Stereo Pair input Right at center 0 did not map to listener-right");
        stereoRight.releaseResources();

        // Width Offset is a real Stereo Pair positioning parameter, not a visual-only control.
        // With center Azimuth 0, -45 and +45 move input Left to 45 and 135 degrees.
        auto renderStereoWidthProbe = [&](float widthOffset) {
            SpaceTraceAudioProcessor probe;
            probe.setCompensationMode(CompensationMode::Raw);
            setParameter(probe, SpaceTraceAudioProcessor::paramInputMode, 1.0f);
            setParameter(probe, SpaceTraceAudioProcessor::paramAzimuth, 0.0f);
            setParameter(probe, SpaceTraceAudioProcessor::paramElevation, 0.0f);
            setParameter(probe, SpaceTraceAudioProcessor::paramDistance, 1.0f);
            setParameter(probe, SpaceTraceAudioProcessor::paramWidthOffset, widthOffset);
            probe.prepareToPlay(44100.0, 512);
            juce::AudioBuffer<float> block(2, 512);
            block.clear();
            block.setSample(0, 0, 1.0f);
            probe.processBlock(block, midi);
            probe.releaseResources();
            return block;
        };
        const auto widthFront = renderStereoWidthProbe(-45.0f);
        const auto widthRear = renderStereoWidthProbe(45.0f);
        float widthDifference = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                widthDifference = std::max(widthDifference,
                    std::abs(widthFront.getSample(c, i) - widthRear.getSample(c, i)));
        require(widthDifference > 1.0e-4f,
                "Stereo Pair Width Offset did not change the rendered source geometry");

        // Dataset Corrected Stereo Pair must use the packaged per-leg side-angle
        // correction pair rather than the normal post-sum global correction.
        auto renderKemarStereoLeft = [&](CompensationMode mode) {
            SpaceTraceAudioProcessor probe;
            probe.selectBuiltInDataset(BuiltInDatasetId::MitKemarNormalPinna);
            probe.setCompensationMode(mode);
            setParameter(probe, SpaceTraceAudioProcessor::paramInputMode, 1.0f);
            setParameter(probe, SpaceTraceAudioProcessor::paramAzimuth, 0.0f);
            setParameter(probe, SpaceTraceAudioProcessor::paramElevation, 0.0f);
            setParameter(probe, SpaceTraceAudioProcessor::paramDistance, 1.0f);
            probe.prepareToPlay(48000.0, 512);
            juce::AudioBuffer<float> block(2, 512);
            block.clear();
            block.setSample(0, 0, 1.0f);
            probe.processBlock(block, midi);
            probe.releaseResources();
            return block;
        };
        const auto kemarStereoRaw = renderKemarStereoLeft(CompensationMode::Raw);
        const auto kemarStereoCorrected = renderKemarStereoLeft(CompensationMode::DatasetCorrected);
        float kemarStereoDifference = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                kemarStereoDifference = std::max(kemarStereoDifference,
                    std::abs(kemarStereoRaw.getSample(c, i) - kemarStereoCorrected.getSample(c, i)));
        require(kemarStereoDifference > 1.0e-4f,
                "KEMAR Dataset Corrected Stereo Pair did not apply the side-angle correction path");

        // Stereo Pair is a calibrated but still perfectly linear sum of two
        // fixed source legs. There is no limiter, AGC, or content-dependent normalizer:
        // both-correlated must equal L-only + R-only.
        auto renderStereoPairImpulse = [&](bool exciteLeft, bool exciteRight) {
            SpaceTraceAudioProcessor probe;
            probe.setCompensationMode(CompensationMode::Raw);
            setParameter(probe, SpaceTraceAudioProcessor::paramInputMode, 1.0f);
            setParameter(probe, SpaceTraceAudioProcessor::paramAzimuth, 45.0f);
            setParameter(probe, SpaceTraceAudioProcessor::paramElevation, 30.0f);
            setParameter(probe, SpaceTraceAudioProcessor::paramDistance, 1.0f);
            probe.prepareToPlay(44100.0, 512);
            juce::AudioBuffer<float> block(2, 512);
            block.clear();
            if (exciteLeft) block.setSample(0, 0, 1.0f);
            if (exciteRight) block.setSample(1, 0, 1.0f);
            probe.processBlock(block, midi);
            probe.releaseResources();
            return block;
        };
        const auto pairLeft = renderStereoPairImpulse(true, false);
        const auto pairRight = renderStereoPairImpulse(false, true);
        const auto pairBoth = renderStereoPairImpulse(true, true);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                require(std::abs(pairBoth.getSample(c, i) -
                                 (pairLeft.getSample(c, i) + pairRight.getSample(c, i))) < 2.0e-5f,
                        "Stereo Pair stopped being a linear sum or introduced nonlinear limiting");

        // A discrete Input Mode change must not reinterpret old FIR history as
        // belonging to the new source topology. Excite the Stereo Pair left
        // renderer at the end of a short block, then switch to Mono. The next
        // silent block must remain silent rather than emitting the old pair tail.
        SpaceTraceAudioProcessor inputModeTransition;
        inputModeTransition.setCompensationMode(CompensationMode::Raw);
        setParameter(inputModeTransition, SpaceTraceAudioProcessor::paramInputMode, 1.0f);
        setParameter(inputModeTransition, SpaceTraceAudioProcessor::paramDistance, 1.0f);
        inputModeTransition.prepareToPlay(44100.0, 64);
        juce::AudioBuffer<float> transitionBlock(2, 64);
        transitionBlock.clear();
        transitionBlock.setSample(0, 63, 1.0f);
        inputModeTransition.processBlock(transitionBlock, midi);
        setParameter(inputModeTransition, SpaceTraceAudioProcessor::paramInputMode, 0.0f);
        transitionBlock.clear();
        inputModeTransition.processBlock(transitionBlock, midi);
        require(maxAbs(transitionBlock) < 1.0e-7f,
                "Input Mode transition leaked stale Stereo Pair renderer history");
        inputModeTransition.releaseResources();

        // Papa Pan is intentionally mono-only. A stored Stereo Pair selection
        // must not split Papa into two historical renderers.
        auto renderPapaSingleChannel = [&](int inputChannel) {
            SpaceTraceAudioProcessor probe;
            probe.setRendererMode(RendererMode::PapaPan);
            probe.setCompensationMode(CompensationMode::Raw);
            setParameter(probe, SpaceTraceAudioProcessor::paramInputMode, 1.0f);
            setParameter(probe, SpaceTraceAudioProcessor::paramAzimuth, 90.0f);
            setParameter(probe, SpaceTraceAudioProcessor::paramDistance, 1.0f);
            probe.prepareToPlay(44100.0, 512);
            juce::AudioBuffer<float> block(2, 512);
            block.clear();
            block.setSample(inputChannel, 0, 1.0f);
            probe.processBlock(block, midi);
            probe.releaseResources();
            return block;
        };
        const auto papaFromLeft = renderPapaSingleChannel(0);
        const auto papaFromRight = renderPapaSingleChannel(1);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                require(std::abs(papaFromLeft.getSample(c, i) - papaFromRight.getSample(c, i)) < 1.0e-6f,
                        "Papa Pan stopped behaving as a mono point-source renderer");

        // Air Loss off removes only the synthetic HF shelf. At 1 m ON/OFF must
        // be identical; at long distance the OFF case must retain more HF energy.
        auto renderAirProbe = [&](float distanceMetres, bool airLossOn) {
            SpaceTraceAudioProcessor probe;
            probe.setCompensationMode(CompensationMode::Raw);
            setParameter(probe, SpaceTraceAudioProcessor::paramAzimuth, 0.0f);
            setParameter(probe, SpaceTraceAudioProcessor::paramElevation, 0.0f);
            setParameter(probe, SpaceTraceAudioProcessor::paramDistance, distanceMetres);
            setParameter(probe, SpaceTraceAudioProcessor::paramAirLoss, airLossOn ? 1.0f : 0.0f);
            probe.prepareToPlay(44100.0, 512);
            juce::AudioBuffer<float> block(2, 512);
            for (int pass = 0; pass < 8; ++pass) {
                for (int i = 0; i < 512; ++i) {
                    const float x = (i & 1) ? -0.25f : 0.25f;
                    block.setSample(0, i, x);
                    block.setSample(1, i, x);
                }
                probe.processBlock(block, midi);
            }
            probe.releaseResources();
            return block;
        };
        const auto oneMetreAirOn = renderAirProbe(1.0f, true);
        const auto oneMetreAirOff = renderAirProbe(1.0f, false);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                require(std::abs(oneMetreAirOn.getSample(c, i) - oneMetreAirOff.getSample(c, i)) < 1.0e-6f,
                        "Air Loss changed the 1 m reference spectrum");
        const auto farAirOn = renderAirProbe(20.0f, true);
        const auto farAirOff = renderAirProbe(20.0f, false);
        const float farOnRms = farAirOn.getRMSLevel(0, 0, 512) + farAirOn.getRMSLevel(1, 0, 512);
        const float farOffRms = farAirOff.getRMSLevel(0, 0, 512) + farAirOff.getRMSLevel(1, 0, 512);
        require(farOffRms > farOnRms * 1.05f,
                "Air Loss off did not disable long-distance synthetic HF attenuation");

        // Project state must restore both host parameters and structural state.
        SpaceTraceAudioProcessor saved;
        saved.selectBuiltInDataset(BuiltInDatasetId::Sadie2D1Ku100);
        saved.setRendererMode(RendererMode::PapaPan);
        saved.setCompensationMode(CompensationMode::Raw);
        setParameter(saved, SpaceTraceAudioProcessor::paramAzimuth, 123.0f);
        setParameter(saved, SpaceTraceAudioProcessor::paramInputMode, 1.0f);
        setParameter(saved, SpaceTraceAudioProcessor::paramWidthOffset, 22.0f);
        setParameter(saved, SpaceTraceAudioProcessor::paramAirLoss, 0.0f);
        juce::MemoryBlock state;
        saved.getStateInformation(state);

        SpaceTraceAudioProcessor restored;
        restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        require(restored.rendererMode() == RendererMode::PapaPan,
                "project state did not restore Papa Pan renderer");
        require(restored.selectedBuiltInDataset() == BuiltInDatasetId::Sadie2D1Ku100,
                "project state did not retain the remembered Modern HRTF head while Papa Pan was selected");
        require(restored.compensationMode() == CompensationMode::Raw,
                "project state did not restore compensation mode");
        require(std::abs(restored.parameters().getRawParameterValue(SpaceTraceAudioProcessor::paramAzimuth)->load() - 123.0f) < 0.01f,
                "project state did not restore azimuth");
        require(restored.inputMode() == InputMode::StereoPair,
                "project state did not restore Stereo Pair input mode");
        require(std::abs(restored.parameters().getRawParameterValue(SpaceTraceAudioProcessor::paramWidthOffset)->load() - 22.0f) < 0.01f,
                "project state did not restore Stereo Pair Width Offset");
        require(restored.parameters().getRawParameterValue(SpaceTraceAudioProcessor::paramAirLoss)->load() < 0.5f,
                "project state did not restore Air Loss off");

        // Schema 4 adds Input Mode and Air Loss as normal host parameters.
        // Loading an earlier schema must preserve the old project and supply
        // the new 1.0 defaults instead of inventing migration-only DSP.
        juce::MemoryBlock legacyState;
        {
            SpaceTraceAudioProcessor legacySource;
            setParameter(legacySource, SpaceTraceAudioProcessor::paramAzimuth, 211.0f);
            legacySource.getStateInformation(legacyState);
            auto legacyXml = juce::AudioProcessor::getXmlFromBinary(
                legacyState.getData(), static_cast<int>(legacyState.getSize()));
            require(legacyXml != nullptr, "could not decode legacy migration probe state");
            auto legacyRoot = juce::ValueTree::fromXml(*legacyXml);
            legacyRoot.setProperty("schemaVersion", 3, nullptr);
            auto legacyParams = legacyRoot.getChildWithName(legacySource.parameters().state.getType());
            require(legacyParams.isValid(), "legacy migration probe has no parameter tree");
            for (int i = legacyParams.getNumChildren(); --i >= 0;) {
                const auto child = legacyParams.getChild(i);
                const auto id = child.getProperty("id").toString();
                if (id == SpaceTraceAudioProcessor::paramInputMode ||
                    id == SpaceTraceAudioProcessor::paramAirLoss)
                    legacyParams.removeChild(i, nullptr);
            }
            auto migratedXml = legacyRoot.createXml();
            require(migratedXml != nullptr, "could not encode legacy migration probe state");
            juce::AudioProcessor::copyXmlToBinary(*migratedXml, legacyState);
        }
        SpaceTraceAudioProcessor migrated;
        migrated.setStateInformation(legacyState.getData(), static_cast<int>(legacyState.getSize()));
        require(std::abs(migrated.parameters().getRawParameterValue(SpaceTraceAudioProcessor::paramAzimuth)->load() - 211.0f) < 0.01f,
                "schema-3 migration lost an existing parameter");
        require(migrated.inputMode() == InputMode::MonoPointSource,
                "schema-3 migration did not default Input Mode to Mono Point Source");
        require(migrated.parameters().getRawParameterValue(SpaceTraceAudioProcessor::paramAirLoss)->load() >= 0.5f,
                "schema-3 migration did not default Air Loss on");

        // Schema 5 adds Stereo Pair Width Offset. Schema-4 projects must
        // restore with the neutral 0-degree reference geometry.
        juce::MemoryBlock schema4State;
        {
            SpaceTraceAudioProcessor schema4Source;
            setParameter(schema4Source, SpaceTraceAudioProcessor::paramWidthOffset, 31.0f);
            schema4Source.getStateInformation(schema4State);
            auto schema4Xml = juce::AudioProcessor::getXmlFromBinary(
                schema4State.getData(), static_cast<int>(schema4State.getSize()));
            require(schema4Xml != nullptr, "could not decode schema-4 migration probe state");
            auto schema4Root = juce::ValueTree::fromXml(*schema4Xml);
            schema4Root.setProperty("schemaVersion", 4, nullptr);
            auto schema4Params = schema4Root.getChildWithName(schema4Source.parameters().state.getType());
            require(schema4Params.isValid(), "schema-4 migration probe has no parameter tree");
            for (int i = schema4Params.getNumChildren(); --i >= 0;) {
                const auto child = schema4Params.getChild(i);
                if (child.getProperty("id").toString() == SpaceTraceAudioProcessor::paramWidthOffset)
                    schema4Params.removeChild(i, nullptr);
            }
            auto migratedXml = schema4Root.createXml();
            require(migratedXml != nullptr, "could not encode schema-4 migration probe state");
            juce::AudioProcessor::copyXmlToBinary(*migratedXml, schema4State);
        }
        SpaceTraceAudioProcessor migratedSchema4;
        setParameter(migratedSchema4, SpaceTraceAudioProcessor::paramWidthOffset, -37.0f);
        migratedSchema4.setStateInformation(schema4State.getData(), static_cast<int>(schema4State.getSize()));
        require(std::abs(migratedSchema4.parameters().getRawParameterValue(SpaceTraceAudioProcessor::paramWidthOffset)->load()) < 0.01f,
                "schema-4 migration did not default Stereo Pair Width Offset to zero");

        // Papa Pan must prepare and render independently of the remembered
        // Modern HRTF head. Dataset Corrected deliberately uses IRCAM's global
        // restoration because IRCAM is Papa's reproducible head basis.
        restored.setCompensationMode(CompensationMode::DatasetCorrected);
        restored.prepareToPlay(44100.0, 256);
        require(restored.statusText().contains("Papa Pan Corrected") &&
                restored.statusText().contains("IRCAM global tonal restoration"),
                "Papa Pan did not select the IRCAM global correction");
        juce::AudioBuffer<float> papaImpulse(2, 256);
        papaImpulse.clear();
        papaImpulse.setSample(0, 0, 1.0f);
        papaImpulse.setSample(1, 0, 1.0f);
        restored.processBlock(papaImpulse, midi);
        require(maxAbs(papaImpulse) > 1.0e-6f, "Papa Pan produced silence");
        restored.releaseResources();
        restored.setRendererMode(RendererMode::ModernHRTF);
        require(restored.selectedBuiltInDataset() == BuiltInDatasetId::Sadie2D1Ku100,
                "switching back from Papa Pan lost the remembered KU100 head");

        // Custom compensation is intentionally bounded so the host's declared
        // tail remains truthful. The accepted IR is embedded in project state.
        const auto tooLong = writeMonoImpulse(0.30, "SpaceTrace_too_long_compensation");
        juce::String error;
        require(!restored.loadCustomCompensationIR(tooLong, error), "over-length custom compensation was accepted");

        const auto validIR = writeMonoImpulse(0.18, "SpaceTrace_valid_compensation");
        error.clear();
        require(restored.loadCustomCompensationIR(validIR, error), "valid custom compensation was rejected");
        require(restored.compensationMode() == CompensationMode::CustomIR,
                "loading a custom correction IR did not select CustomIR mode");
        juce::MemoryBlock customState;
        restored.getStateInformation(customState);

        SpaceTraceAudioProcessor customRestored;
        customRestored.setStateInformation(customState.getData(), static_cast<int>(customState.getSize()));
        require(customRestored.compensationMode() == CompensationMode::CustomIR,
                "project state did not restore CustomIR mode");
        customRestored.prepareToPlay(44100.0, 512);
        require(customRestored.statusText().contains("Custom Correction IR active"),
                "restored custom IR was not prepared");
        customRestored.releaseResources();

        // Replacing a custom IR while CustomIR is already selected must reload
        // the active convolution rather than returning early on the unchanged
        // mode. Verify this on one prepared processor using two gain-only IRs.
        const auto customA = writeMonoImpulse(0.01, "SpaceTrace_custom_reload_A", 1.0f);
        const auto customB = writeMonoImpulse(0.01, "SpaceTrace_custom_reload_B", 0.25f);
        SpaceTraceAudioProcessor reloadProbe;
        reloadProbe.setCompensationMode(CompensationMode::Raw);
        reloadProbe.prepareToPlay(44100.0, 512);
        error.clear();
        require(reloadProbe.loadCustomCompensationIR(customA, error), "first custom reload IR failed");
        juce::AudioBuffer<float> reloadImpulseA(2, 512);
        reloadImpulseA.clear();
        reloadImpulseA.setSample(0, 0, 1.0f);
        reloadImpulseA.setSample(1, 0, 1.0f);
        reloadProbe.processBlock(reloadImpulseA, midi);
        const float peakA = maxAbs(reloadImpulseA);
        error.clear();
        require(reloadProbe.loadCustomCompensationIR(customB, error), "replacement custom reload IR failed");
        juce::AudioBuffer<float> reloadImpulseB(2, 512);
        reloadImpulseB.clear();
        reloadImpulseB.setSample(0, 0, 1.0f);
        reloadImpulseB.setSample(1, 0, 1.0f);
        reloadProbe.processBlock(reloadImpulseB, midi);
        const float peakB = maxAbs(reloadImpulseB);
        require(peakA > 1.0e-6f && peakB > 1.0e-6f && peakB < peakA * 0.40f,
                "replacing CustomIR did not reload the active convolution");
        reloadProbe.releaseResources();

        tooLong.deleteFile();
        validIR.deleteFile();
        customA.deleteFile();
        customB.deleteFile();

        // Lifecycle stress: repeatedly construct, prepare, process, perform the
        // same synchronous structural swaps used by the editor, and destroy the
        // processor without an explicit releaseResources() call. This cannot
        // reproduce a host-wrapper teardown race, but it exercises the exact
        // processor-owned destruction path that REAPER reaches when an instance
        // is removed while transport is active.
        for (int pass = 0; pass < 64; ++pass) {
            auto stressed = std::make_unique<SpaceTraceAudioProcessor>();
            stressed->prepareToPlay(44100.0, 128);

            // Exercise the real editor ownership/lifetime path too. REAPER
            // destroys the editor before the processor when a visible FX is
            // removed with its track. The visual foundation must not leave a
            // timer, attachment, or callback behind that can touch the
            // processor after the editor has gone away.
            std::unique_ptr<juce::AudioProcessorEditor> stressedEditor(stressed->createEditor());
            require(stressedEditor != nullptr, "lifecycle stress could not create plugin editor");

            juce::AudioBuffer<float> block(2, 128);
            juce::MidiBuffer stressMidi;
            for (int blockIndex = 0; blockIndex < 8; ++blockIndex) {
                block.clear();
                block.setSample(0, 0, 0.5f);
                block.setSample(1, 0, 0.5f);
                setParameter(*stressed, SpaceTraceAudioProcessor::paramAzimuth,
                             static_cast<float>((pass * 37 + blockIndex * 53) % 360));
                setParameter(*stressed, SpaceTraceAudioProcessor::paramInputMode,
                             (pass + blockIndex) % 2 == 0 ? 0.0f : 1.0f);
                setParameter(*stressed, SpaceTraceAudioProcessor::paramAirLoss,
                             (pass + blockIndex) % 3 == 0 ? 0.0f : 1.0f);
                stressed->processBlock(block, stressMidi);
            }

            stressed->setRendererMode((pass % 5) == 0 ? RendererMode::PapaPan : RendererMode::ModernHRTF);
            stressed->selectBuiltInDataset((pass % 2) == 0
                ? BuiltInDatasetId::Sadie2D1Ku100
                : BuiltInDatasetId::MitKemarNormalPinna);
            stressed->setCompensationMode((pass % 3) == 0
                ? CompensationMode::Raw
                : CompensationMode::DatasetCorrected);

            block.clear();
            block.setSample(0, 0, 0.25f);
            block.setSample(1, 0, 0.25f);
            stressed->processBlock(block, stressMidi);

            if ((pass % 4) == 0) {
                setParameter(*stressed, SpaceTraceAudioProcessor::paramBypass, 1.0f);
                stressed->processBlock(block, stressMidi);
            }

            stressedEditor.reset();
            stressed.reset();
        }

        std::cout << "SpaceTrace 1.0 plugin processor tests: PASS\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "SpaceTrace 1.0 plugin processor tests: FAIL: " << e.what() << '\n';
        return 1;
    }
}
