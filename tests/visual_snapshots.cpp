#include "PluginProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

using spacetrace::BuiltInDatasetId;
using spacetrace::CompensationMode;
using spacetrace::plugin::RendererMode;
using spacetrace::plugin::SpaceTraceAudioProcessor;

namespace {
struct StateSpec {
    juce::String name;
    std::function<void(SpaceTraceAudioProcessor&)> configure;
};

struct ImageStats {
    bool valid = false;
    bool uniform = true;
    bool black = true;
    juce::uint32 first = 0;
    int minRgb = 255;
    int maxRgb = 0;
};

void require(bool condition, const juce::String& message) {
    if (!condition) throw std::runtime_error(message.toStdString());
}

void setParameter(SpaceTraceAudioProcessor& processor, const char* id, float plainValue) {
    auto* parameter = processor.parameters().getParameter(id);
    require(parameter != nullptr, "Missing parameter: " + juce::String(id));
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

juce::File writeCustomImpulse() {
    const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("SpaceTrace_snapshot_custom_correction", ".wav", false);
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    require(stream != nullptr, "Could not create temporary custom-correction WAV");

    juce::WavAudioFormat wav;
    auto options = juce::AudioFormatWriter::Options{}
        .withSampleRate(44100.0)
        .withChannelLayout(juce::AudioChannelSet::mono())
        .withBitsPerSample(24);
    auto writer = wav.createWriterFor(stream, options);
    require(writer != nullptr, "Could not create temporary custom-correction writer");

    juce::AudioBuffer<float> impulse(1, 64);
    impulse.clear();
    impulse.setSample(0, 0, 1.0f);
    require(writer->writeFromAudioSampleBuffer(impulse, 0, impulse.getNumSamples()),
            "Could not write temporary custom-correction WAV");
    writer.reset();
    return file;
}

void markReferencedHeadMissing(SpaceTraceAudioProcessor& processor) {
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), static_cast<int>(state.getSize()));
    require(xml != nullptr, "Could not decode SpaceTrace state for missing-head snapshot");
    auto root = juce::ValueTree::fromXml(*xml);
    auto structural = root.getChildWithName("SpaceTraceStructuralState");
    require(structural.isValid(), "Structural state missing while preparing missing-head snapshot");
    structural.setProperty("dataset", "org.lanesaudio.spacetrace.snapshot-missing-head", nullptr);
    structural.setProperty("headHash", "snapshot-missing-hash", nullptr);
    if (auto modified = root.createXml()) {
        juce::MemoryBlock encoded;
        juce::AudioProcessor::copyXmlToBinary(*modified, encoded);
        processor.setStateInformation(encoded.getData(), static_cast<int>(encoded.getSize()));
        return;
    }
    throw std::runtime_error("Could not encode missing-head snapshot state");
}

ImageStats inspectImage(const juce::Image& image) {
    ImageStats stats;
    if (!image.isValid() || image.getWidth() <= 0 || image.getHeight() <= 0) return stats;
    stats.valid = true;

    juce::Image::BitmapData pixels(image, juce::Image::BitmapData::readOnly);
    bool first = true;
    for (int y = 0; y < image.getHeight(); ++y) {
        for (int x = 0; x < image.getWidth(); ++x) {
            const auto c = pixels.getPixelColour(x, y);
            const auto argb = c.getARGB();
            if (first) {
                stats.first = argb;
                first = false;
            } else if (argb != stats.first) {
                stats.uniform = false;
            }
            const int rgbMax = std::max({static_cast<int>(c.getRed()), static_cast<int>(c.getGreen()), static_cast<int>(c.getBlue())});
            const int rgbMin = std::min({static_cast<int>(c.getRed()), static_cast<int>(c.getGreen()), static_cast<int>(c.getBlue())});
            stats.minRgb = std::min(stats.minRgb, rgbMin);
            stats.maxRgb = std::max(stats.maxRgb, rgbMax);
            if (rgbMax > 3 && c.getAlpha() > 3) stats.black = false;
        }
    }
    return stats;
}

juce::String scaleLabel(float scale) {
    return juce::String(static_cast<int>(std::lround(scale * 100.0f))) + "pct";
}

void renderState(const StateSpec& spec, float scale, const juce::File& output,
                 juce::StringArray& manifestLines) {
    SpaceTraceAudioProcessor processor;
    spec.configure(processor);
    processor.prepareToPlay(44100.0, 512);

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    require(editor != nullptr, "Could not create editor for " + spec.name);
    editor->setSize(900, 680);
    editor->resized();

    const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, scale,
                                                        juce::SoftwareImageType{});
    const auto stats = inspectImage(image);
    require(stats.valid, spec.name + ": snapshot image is invalid");
    require(!stats.uniform, spec.name + ": snapshot image is uniform/blank");
    require(!stats.black, spec.name + ": snapshot image is black");

    const auto filename = spec.name + "_" + scaleLabel(scale) + ".png";
    const auto file = output.getChildFile(filename);
    juce::FileOutputStream stream(file);
    require(stream.openedOk(), "Could not create " + file.getFullPathName());
    juce::PNGImageFormat png;
    require(png.writeImageToStream(image, stream), "PNG encoding failed for " + filename);
    stream.flush();

    manifestLines.add(filename + "\tstate=" + spec.name + "\tscale=" + juce::String(scale, 2) +
                      "\tsize=" + juce::String(image.getWidth()) + "x" + juce::String(image.getHeight()) +
                      "\trgb-range=" + juce::String(stats.minRgb) + ".." + juce::String(stats.maxRgb));
    processor.releaseResources();
}

StateSpec simpleState(const juce::String& name, float azimuth, float elevation,
                      std::function<void(SpaceTraceAudioProcessor&)> extra = {}) {
    return {name, [=](SpaceTraceAudioProcessor& p) {
        setParameter(p, SpaceTraceAudioProcessor::paramAzimuth, azimuth);
        setParameter(p, SpaceTraceAudioProcessor::paramElevation, elevation);
        if (extra) extra(p);
    }};
}
} // namespace

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI juceInitialiser;
    try {
        require(argc == 2, "Usage: spacetrace_visual_snapshots <output-directory>");
        const juce::File output(argv[1]);
        require(output.createDirectory().wasOk(), "Could not create snapshot output directory: " + output.getFullPathName());

        const auto customIR = writeCustomImpulse();
        std::vector<StateSpec> states;
        states.push_back({"default", [](SpaceTraceAudioProcessor&) {}});
        states.push_back({"head_ircam", [](auto& p) { p.selectBuiltInDataset(BuiltInDatasetId::IrcamListen1050); }});
        states.push_back({"head_kemar", [](auto& p) { p.selectBuiltInDataset(BuiltInDatasetId::MitKemarNormalPinna); }});
        states.push_back({"head_sadie_ku100", [](auto& p) { p.selectBuiltInDataset(BuiltInDatasetId::Sadie2D1Ku100); }});
        states.push_back({"head_full2deg_ku100", [](auto& p) { p.selectBuiltInDataset(BuiltInDatasetId::ThkKu100Full2Deg); }});
        states.push_back({"head_fabian_hato0", [](auto& p) { p.selectBuiltInDataset(BuiltInDatasetId::FabianHato0); }});
        states.push_back({"renderer_papa_pan", [](auto& p) { p.setRendererMode(RendererMode::PapaPan); }});
        states.push_back({"tone_raw", [](auto& p) { p.setCompensationMode(CompensationMode::Raw); }});
        states.push_back({"tone_dataset_corrected", [](auto& p) { p.setCompensationMode(CompensationMode::DatasetCorrected); }});
        states.push_back({"tone_custom_correction", [customIR](auto& p) {
            juce::String error;
            require(p.loadCustomCompensationIR(customIR, error), "Could not prepare Custom Correction snapshot: " + error);
        }});
        states.push_back({"bypass", [](auto& p) { setParameter(p, SpaceTraceAudioProcessor::paramBypass, 1.0f); }});
        states.push_back({"missing_head_error", [](auto& p) { markReferencedHeadMissing(p); }});

        for (float az : {0.0f, 90.0f, 180.0f, 270.0f, 360.0f})
            states.push_back(simpleState("az_" + juce::String(static_cast<int>(az)), az, 0.0f));
        for (float el : {-90.0f, 0.0f, 90.0f})
            states.push_back(simpleState("el_" + juce::String(static_cast<int>(el)), 0.0f, el));

        states.push_back(simpleState("az45_el30", 45.0f, 30.0f));
        states.push_back(simpleState("az90_el45", 90.0f, 45.0f));
        states.push_back(simpleState("az135_elminus30", 135.0f, -30.0f));
        states.push_back(simpleState("az225_el30", 225.0f, 30.0f));
        states.push_back(simpleState("az270_elminus45", 270.0f, -45.0f));
        states.push_back(simpleState("az315_el60", 315.0f, 60.0f));

        const auto stereo = [](float az, float el, float width = 0.0f) {
            return [=](SpaceTraceAudioProcessor& p) {
                setParameter(p, SpaceTraceAudioProcessor::paramInputMode, 1.0f);
                setParameter(p, SpaceTraceAudioProcessor::paramAzimuth, az);
                setParameter(p, SpaceTraceAudioProcessor::paramElevation, el);
                setParameter(p, SpaceTraceAudioProcessor::paramWidthOffset, width);
            };
        };
        states.push_back({"stereo_center_0", stereo(0.0f, 0.0f)});
        states.push_back({"stereo_center_45", stereo(45.0f, 0.0f)});
        states.push_back({"stereo_center_90", stereo(90.0f, 0.0f)});
        states.push_back({"stereo_center_180", stereo(180.0f, 0.0f)});
        states.push_back({"stereo_center_45_el30", stereo(45.0f, 30.0f)});
        states.push_back({"stereo_center_270_elminus45", stereo(270.0f, -45.0f)});
        states.push_back({"stereo_width_minus45", stereo(0.0f, 0.0f, -45.0f)});
        states.push_back({"stereo_width_plus45", stereo(0.0f, 0.0f, 45.0f)});
        states.push_back({"stereo_az45_width_plus20", stereo(45.0f, 0.0f, 20.0f)});
        states.push_back({"air_loss_off_far", [](auto& p) {
            setParameter(p, SpaceTraceAudioProcessor::paramDistance, 20.0f);
            setParameter(p, SpaceTraceAudioProcessor::paramAirLoss, 0.0f);
        }});

        juce::StringArray manifest;
        manifest.add("SpaceTrace visual snapshot manifest");
        manifest.add("Renderer: JUCE Component::createComponentSnapshot with SoftwareImageType");
        manifest.add("Scale values are JUCE component render scales, not simulated Windows host DPI.");
        manifest.add("Real Windows 100/125/150/175/200% DPI validation remains a separate host test.");
        manifest.add(juce::String());

        for (const auto& state : states) renderState(state, 1.0f, output, manifest);
        for (float scale : {1.25f, 1.50f, 1.75f, 2.00f})
            renderState(states.front(), scale, output, manifest);
        for (float scale : {1.25f, 1.50f, 1.75f, 2.00f})
            renderState(StateSpec{"stereo_center_0", stereo(0.0f, 0.0f)}, scale, output, manifest);

        require(output.getChildFile("manifest.txt").replaceWithText(manifest.joinIntoString("\n") + "\n"),
                "Could not write visual snapshot manifest");

        juce::String report;
        report << "# SpaceTrace visual snapshot report\n\n"
               << "Result: PASS\n\n"
               << "The harness instantiated the real SpaceTrace processor/editor, applied deterministic states, "
                  "captured the real component hierarchy with JUCE's software snapshot path, and rejected invalid, "
                  "uniform, or black renders.\n\n"
               << "The 125%, 150%, 175%, and 200% files are component-render scaling checks. They do not claim to "
                  "reproduce Windows per-monitor DPI behavior; that remains a real-host validation step.\n\n"
               << "Rendered PNG count: " << output.findChildFiles(juce::File::findFiles, false, "*.png").size() << "\n";
        require(output.getChildFile("VISUAL_SNAPSHOT_REPORT.md").replaceWithText(report),
                "Could not write visual snapshot report");

        customIR.deleteFile();
        std::cout << "PASS: SpaceTrace visual snapshots written to " << output.getFullPathName() << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
