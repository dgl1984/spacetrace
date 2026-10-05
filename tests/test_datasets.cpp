#include "spacetrace/DatasetCatalog.h"
#include "spacetrace/FIRRenderer.h"
#include "spacetrace/NativePackage.h"
#include "spacetrace/PapaPanRenderer.h"
#include "spacetrace/Resample.h"

#include <cmath>
#include <complex>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>

using namespace spacetrace;

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static double energy(const std::vector<float>& x) {
    return std::inner_product(x.begin(), x.end(), x.begin(), 0.0);
}

static double responseMagnitude(const std::vector<float>& h, double sampleRate, double frequency) {
    std::complex<double> sum {};
    for (std::size_t n = 0; n < h.size(); ++n)
        sum += static_cast<double>(h[n]) * std::polar(1.0, -2.0 * 3.14159265358979323846 * frequency * static_cast<double>(n) / sampleRate);
    return std::abs(sum);
}

static void requireResponseStable(const std::vector<float>& before, double beforeRate,
                                  const std::vector<float>& after, double afterRate,
                                  double maxDb, const char* message) {
    for (const double f : {100.0, 1000.0, 5000.0, 10000.0, 20000.0}) {
        const auto a = responseMagnitude(before, beforeRate, f);
        const auto b = responseMagnitude(after, afterRate, f);
        require(a > 1.0e-8 && b > 1.0e-8 && std::abs(20.0 * std::log10(b / a)) < maxDb, message);
    }
}

static const Measurement& exactHorizontal(const HRTFSet& set, double azimuth) {
    const auto& m = set.nearestMeasurement({azimuth, 0.0, set.measurements().front().position.distanceMetres});
    require(std::abs(wrapAzimuthDegrees(m.position.azimuthDegrees - azimuth)) < 0.01,
            "expected horizontal azimuth not present");
    require(std::abs(m.position.elevationDegrees) < 0.01, "expected elevation-zero measurement not present");
    return m;
}

int main() {
    try {
        const auto root = std::filesystem::path(SPACETRACE_SOURCE_DIR);
        const auto ircamPath = root / "resources/datasets/IRC_1050_R_44100.sthrtf";
        const auto kemarPath = root / "resources/datasets/mit_kemar_normal_pinna.sthrtf";
        const auto ku100Path = root / "resources/datasets/sadie2_d1_ku100_44100.sthrtf";
        const auto full2DegPath = root / "Heads/KU100_FULL2DEG/head.sthrtf";
        const auto fabianPath = root / "Heads/FABIAN_HATO0/head.sthrtf";

        auto ircam = NativePackage::loadFile(ircamPath);
        auto kemar = NativePackage::loadFile(kemarPath);
        require(static_cast<bool>(ircam), "IRCAM package failed to load");
        require(static_cast<bool>(kemar), "KEMAR package failed to load");

        const auto& i = *ircam.package;
        const auto& k = *kemar.package;
        require(i.hrtf.metadata().contentSha256 == "5efc0dcc002551313b60a9bce6c957db1b07c8d25d1474c9f9333c5175c4dd83",
                "IRCAM official source hash mismatch");
        require(k.hrtf.metadata().contentSha256 == "e7035994f5fd754058424c061380ee92b1d5ed58fccef2887a4266916616acdf",
                "KEMAR official source hash mismatch");
        require(i.hrtf.measurements().size() == 187, "IRCAM measurement count mismatch");
        require(k.hrtf.measurements().size() == 710, "KEMAR measurement count mismatch");
        require(i.hrtf.measurements().front().left.size() == 128, "IRCAM compact HRIR length mismatch");
        require(k.hrtf.measurements().front().left.size() == 512, "KEMAR HRIR length mismatch");
        require(i.hasDatasetCompensation && k.hasDatasetCompensation, "built-in dataset compensation missing");

        auto full2Deg = NativePackage::loadFile(full2DegPath);
        require(static_cast<bool>(full2Deg), "TH Köln KU100 FULL2DEG package failed to load");
        const auto& full = *full2Deg.package;
        require(full.hrtf.metadata().contentSha256 == "d3671e6829323b93b7ae95aff6d9316d15d6c128b7f2ab615c21cc3ab085ddb7",
                "FULL2DEG source hash mismatch");
        require(full.hrtf.metadata().database == "THK", "FULL2DEG database identity mismatch");
        require(std::abs(full.hrtf.sampleRate() - 48000.0) < 0.1, "FULL2DEG sample rate mismatch");
        require(full.hrtf.measurements().size() == 16020, "FULL2DEG measurement count mismatch");
        require(full.hrtf.measurements().front().left.size() == 128, "FULL2DEG HRIR length mismatch");
        require(!full.hasDatasetCompensation, "FULL2DEG must remain Raw-only until its own correction is approved");

        auto fabian = NativePackage::loadFile(fabianPath);
        require(static_cast<bool>(fabian), "FABIAN HATO 0 package failed to load");
        const auto& f = *fabian.package;
        require(f.hrtf.metadata().contentSha256 == "83ebbcd9a09d17679b95d201c9775438c0bb1199d565c3fc7a25448a905cdc3c",
                "FABIAN source hash mismatch");
        require(f.hrtf.metadata().database == "FABIAN HRTF database", "FABIAN database identity mismatch");
        require(std::abs(f.hrtf.sampleRate() - 44100.0) < 0.1, "FABIAN sample rate mismatch");
        require(f.hrtf.measurements().size() == 11950, "FABIAN measurement count mismatch");
        require(f.hrtf.measurements().front().left.size() == 256, "FABIAN HRIR length mismatch");
        require(!f.hasDatasetCompensation, "FABIAN directional head package must remain Raw-only; correction belongs in external correction.wav");
        require(i.datasetCompensation.version == "tonetrace-raw-v1", "IRCAM Tone Trace correction identity changed");
        require(k.datasetCompensation.version == "tonetrace-raw-v1", "KEMAR Tone Trace correction identity changed");

        // Papa Pan depends on the exact 24-point horizontal IRCAM ring. Keep
        // that dependency explicit so future dataset preparation cannot break
        // the historical renderer while the generic HRTF tests still pass.
        for (int sector = 0; sector < PapaPanRenderer::sectorCount; ++sector)
            (void) exactHorizontal(i.hrtf, static_cast<double>(sector * 15));
        PapaPanRenderer papaNative;
        require(papaNative.prepare(i.hrtf, 256, 44100.0),
                "official IRCAM package cannot prepare Papa Pan");
        const auto ircam96ForPapa = resampleHRTFSet(i.hrtf, 96000.0);
        PapaPanRenderer papa96Native;
        require(papa96Native.prepare(ircam96ForPapa, 512, 96000.0),
                "96-kHz IRCAM package cannot prepare Papa Pan");
        require(papa96Native.stepIntervalSamples() == 557,
                "Papa Pan historical cadence changed after IRCAM host-rate conversion");

        // Host-rate conversion must preserve the physical-frequency response.
        // This specifically guards the former FIR-resampling gain defect that
        // could shift HRTFs/corrections by several dB above 44.1 kHz.
        for (const double hostRate : {48000.0, 88200.0, 96000.0}) {
            const auto ircamHost = resampleHRTFSet(i.hrtf, hostRate);
            const auto kemarHost = resampleHRTFSet(k.hrtf, hostRate);
            const auto& iFront = i.hrtf.nearestMeasurement({0.0, 0.0, 2.06});
            const auto& iFrontHost = ircamHost.nearestMeasurement({0.0, 0.0, 2.06});
            const auto& kFront = k.hrtf.nearestMeasurement({0.0, 0.0, 1.4});
            const auto& kFrontHost = kemarHost.nearestMeasurement({0.0, 0.0, 1.4});
            requireResponseStable(iFront.left, i.hrtf.sampleRate(), iFrontHost.left, hostRate, 0.06,
                                  "IRCAM HRTF response changed during host-rate conversion");
            requireResponseStable(kFront.left, k.hrtf.sampleRate(), kFrontHost.left, hostRate, 0.12,
                                  "KEMAR HRTF response changed during host-rate conversion");

            const auto iComp = resampleCompensation(i.datasetCompensation, hostRate);
            const auto kComp = resampleCompensation(k.datasetCompensation, hostRate);
            requireResponseStable(i.datasetCompensation.impulse, i.datasetCompensation.sampleRate,
                                  iComp.impulse, hostRate, 0.02,
                                  "IRCAM correction response changed during host-rate conversion");
            requireResponseStable(k.datasetCompensation.impulse, k.datasetCompensation.sampleRate,
                                  kComp.impulse, hostRate, 0.02,
                                  "KEMAR correction response changed during host-rate conversion");
        }

        require(defaultBuiltInDataset().id == BuiltInDatasetId::IrcamListen1050,
                "IRCAM must remain the default built-in dataset");

        const auto& il = exactHorizontal(i.hrtf, 90.0);
        const auto& ir = exactHorizontal(i.hrtf, -90.0);
        require(10.0 * std::log10(energy(il.left) / energy(il.right)) > 10.0,
                "IRCAM +90 must strongly favor the listener-left ear");
        require(10.0 * std::log10(energy(ir.left) / energy(ir.right)) < -10.0,
                "IRCAM -90 must strongly favor the listener-right ear");
        require(il.rightDelaySamples > il.leftDelaySamples,
                "IRCAM +90 delay ordering is inconsistent with listener-left source");
        require(ir.leftDelaySamples > ir.rightDelaySamples,
                "IRCAM -90 delay ordering is inconsistent with listener-right source");

        const auto& kl = exactHorizontal(k.hrtf, 90.0);
        const auto& kr = exactHorizontal(k.hrtf, -90.0);
        require(10.0 * std::log10(energy(kl.left) / energy(kl.right)) > 10.0,
                "KEMAR +90 must strongly favor the listener-left ear");
        require(10.0 * std::log10(energy(kr.left) / energy(kr.right)) < -10.0,
                "KEMAR -90 must strongly favor the listener-right ear");

        if (std::filesystem::exists(ku100Path)) {
            auto ku100 = NativePackage::loadFile(ku100Path);
            require(static_cast<bool>(ku100), "SADIE II D1 / KU100 package failed to load");
            const auto& u = *ku100.package;
            require(u.hrtf.metadata().database == "SADIE II", "KU100 database identity mismatch");
            require(u.hrtf.metadata().subject.find("D1") != std::string::npos, "KU100 subject identity mismatch");
            require(std::abs(u.hrtf.sampleRate() - 44100.0) < 0.1, "KU100 sample rate mismatch");
            require(u.hrtf.measurements().size() == 8802, "KU100 measurement count mismatch");
            require(u.hrtf.measurements().front().left.size() == 256, "KU100 HRIR length mismatch");
            require(u.hasDatasetCompensation, "KU100 measured dataset correction is missing");
            require(u.datasetCompensation.version == "tonetrace-raw-v1", "KU100 Tone Trace correction identity changed");
            require(u.datasetCompensation.name == "Tone Trace raw-HRTF global restoration", "KU100 Tone Trace correction name changed");
            require(u.datasetCompensation.impulse.size() == 7938, "KU100 Tone Trace correction length changed");

            const auto& ul = exactHorizontal(u.hrtf, 90.0);
            const auto& ur = exactHorizontal(u.hrtf, -90.0);
            require(10.0 * std::log10(energy(ul.left) / energy(ul.right)) > 6.0,
                    "KU100 +90 must favor the listener-left ear");
            require(10.0 * std::log10(energy(ur.left) / energy(ur.right)) < -6.0,
                    "KU100 -90 must favor the listener-right ear");
        }

        // Exercise actual package timing through the offline renderer.
        const auto impulse = std::vector<float>{1.0f};
        const auto rendered = renderNearestOffline(impulse, i.hrtf, {90.0, 0.0, 2.06});
        require(rendered.left.size() > 128 && rendered.right.size() > 128,
                "IRCAM per-ear delay was not included in rendered output");

        std::cout << "SpaceTrace 0.6.0 external-head dataset tests: PASS\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "SpaceTrace 0.6.0 external-head dataset tests: FAIL: " << e.what() << '\n';
        return 1;
    }
}
