#include "PluginProcessor.h"
#include "HeadRepository.h"
#include <juce_dsp/juce_dsp.h>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace spacetrace;
using namespace spacetrace::plugin;

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

static void parameter(SpaceTraceAudioProcessor& p, const char* id, float value) {
    auto* v = p.parameters().getParameter(id);
    v->setValueNotifyingHost(v->convertTo0to1(value));
}

// Independent frequency-domain reference from package assets. In particular,
// this includes exactly one correction per source and no mono calibration.
static double expectedStereoPower(const PreparedHeadData& head, double rate) {
    constexpr int order = 16, size = 1 << order;
    juce::dsp::FFT fft(order);
    double total = 0;
    for (int leg = 0; leg < 2; ++leg) {
        const auto& row = head.hrtf.nearestMeasurement({leg == 0 ? 90.0 : -90.0, 0, 1});
        const auto& correction = leg == 0 ? head.stereoPair.leftCompensation : head.stereoPair.rightCompensation;
        std::vector<float> c(size * 2, 0);
        std::copy(correction.impulse.begin(), correction.impulse.end(), c.begin());
        fft.performRealOnlyForwardTransform(c.data(), true);
        for (int ear = 0; ear < 2; ++ear) {
            const auto& ir = ear == 0 ? row.left : row.right;
            const double delay = ear == 0 ? row.leftDelaySamples : row.rightDelaySamples;
            const double fraction = delay - std::floor(delay);
            std::vector<float> h(size * 2, 0);
            std::copy(ir.begin(), ir.end(), h.begin());
            fft.performRealOnlyForwardTransform(h.data(), true);
            double sum = 0, weights = 0;
            for (int k = 1; k < size / 2; ++k) {
                const double hz = k * rate / size;
                if (hz < 80 || hz > 16000) continue;
                const double delayPower = (1-fraction)*(1-fraction) + fraction*fraction
                    + 2*fraction*(1-fraction)*std::cos(juce::MathConstants<double>::twoPi * hz / rate);
                const double hPower = double(h[2*k])*h[2*k] + double(h[2*k+1])*h[2*k+1];
                const double cPower = double(c[2*k])*c[2*k] + double(c[2*k+1])*c[2*k+1];
                sum += hPower * cPower * delayPower / hz;
                weights += 1 / hz;
            }
            const double db = head.levelTrimDb + head.stereoPair.trimDb
                + (leg == 0 ? head.stereoPair.leftGainDb : head.stereoPair.rightGainDb)
                + (ear == 0 ? head.leftGainDb : head.rightGainDb);
            total += sum / weights * std::pow(10.0, db / 10) / 2;
        }
    }
    return total;
}

// Measure the real processor's complete impulse response, including all filters
// and gain stages. Separate L/R excitations give independent stereo power.
static double power(BuiltInDatasetId head, double rate, bool pair,
                    float left, float right, bool monoBus = false) {
    SpaceTraceAudioProcessor p;
    if (monoBus) {
        auto layout = p.getBusesLayout();
        layout.inputBuses.set(0, juce::AudioChannelSet::mono());
        require(p.setBusesLayout(layout), "mono bus rejected");
    }
    p.selectBuiltInDataset(head);
    parameter(p, SpaceTraceAudioProcessor::paramInputMode, pair ? 1.0f : 0.0f);
    p.prepareToPlay(rate, 512);
    require(!p.statusText().containsIgnoreCase("unavailable"), "head unavailable");
    constexpr int order = 16, size = 1 << order;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> block(2, 512);
    // Let the renderer settle before measurement.
    for (int i = 0; i < 8; ++i) { block.clear(); p.processBlock(block, midi); }
    std::vector<float> ears[2] {std::vector<float>(size * 2), std::vector<float>(size * 2)};
    for (int offset = 0; offset < size; offset += 512) {
        block.clear();
        if (offset == 0) { block.setSample(0, 0, left); block.setSample(1, 0, right); }
        p.processBlock(block, midi);
        for (int ear = 0; ear < 2; ++ear)
            std::copy_n(block.getReadPointer(ear), 512, ears[ear].begin() + offset);
    }
    p.releaseResources();
    juce::dsp::FFT fft(order);
    double result = 0;
    for (auto& ear : ears) {
        fft.performRealOnlyForwardTransform(ear.data(), true);
        double sum = 0, weights = 0;
        for (int k = 1; k < size / 2; ++k) {
            const double hz = k * rate / size;
            if (hz < 80 || hz > 16000) continue;
            const double re = ear[2*k], im = ear[2*k+1];
            sum += (re*re + im*im) / hz;
            weights += 1 / hz;
        }
        result += sum / weights / 2;
    }
    return result;
}

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    try {
        const BuiltInDatasetId ids[] {BuiltInDatasetId::IrcamListen1050,
            BuiltInDatasetId::MitKemarNormalPinna, BuiltInDatasetId::Sadie2D1Ku100,
            BuiltInDatasetId::ThkKu100Full2Deg, BuiltInDatasetId::FabianHato0};
        for (double rate : {44100.0, 48000.0, 96000.0}) {
            for (int i = 0; i < 5; ++i) {
                juce::String error;
                const auto keepPrepared = HeadRepository::instance().prepare(ids[i], rate, error);
                require(keepPrepared != nullptr, "head preparation failed");
                const double mono = power(ids[i], rate, false, 1, 1);
                const double db = 10 * std::log10(mono);
                std::cout << "head " << i+1 << " rate " << rate << " mono dB " << db << std::endl;
                // One fixed native-rate calibration; rate-converted fractional
                // delay can produce a small residual (IRCAM: about 0.42 dB at 96k).
                require(std::abs(db) < .5, "mono reference misses unity by >0.5 dB");
                const double trueMono = power(ids[i], rate, false, 1, 0, true);
                require(std::abs(10 * std::log10(trueMono / mono)) < .001,
                        "mono bus differs from identical stereo input");
                const double leftOnly = power(ids[i], rate, false, 1, 0);
                require(std::abs(10 * std::log10(leftOnly / mono) + 6.0206) < .001,
                        "stereo fold-down no longer averages channels");
                require(power(ids[i], rate, false, 1, -1) < 1e-12,
                        "opposite-polarity mono input should cancel");
                const double stereo = power(ids[i], rate, true, 1, 0) + power(ids[i], rate, true, 0, 1);
                const double sdb = 10 * std::log10(stereo);
                std::cout << "head " << i+1 << " rate " << rate << " stereo dB " << sdb << std::endl;
                require(std::abs(10 * std::log10(stereo / expectedStereoPower(*keepPrepared, rate))) < .005,
                        "stereo differs from package reference; possible duplicated correction or mono gain leak");
            }
        }
        std::cout << "Mono level and stereo preservation: PASS\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
