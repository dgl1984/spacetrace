#include "spacetrace/Compensation.h"
#include "spacetrace/DatasetCatalog.h"
#include "spacetrace/FIRRenderer.h"
#include "spacetrace/HRTFSet.h"
#include "spacetrace/NativePackage.h"
#include "spacetrace/RealtimeFIRRenderer.h"
#include "spacetrace/PapaPanRenderer.h"
#include "spacetrace/Resample.h"
#include "spacetrace/DistanceModel.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace spacetrace;

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static HRTFSet twoPointSet() {
    HRTFMetadata md;
    md.name = "two point test";
    md.subject = "synthetic";
    md.sourceFormat = "test";
    md.processing = "synthetic test kernels";

    Measurement left;
    left.position = {90.0, 0.0, 1.0};
    left.left = {1.0f, 0.0f, 0.0f};
    left.right = {0.0f, 0.5f, 0.0f};

    Measurement right;
    right.position = {-90.0, 0.0, 1.0};
    right.left = {0.0f, 0.5f, 0.0f};
    right.right = {1.0f, 0.0f, 0.0f};

    return HRTFSet(48000.0, md, {left, right});
}

static HRTFSet papaRingSet(double sampleRate = 44100.0) {
    HRTFMetadata md;
    md.name = "papa ring test";
    md.subject = "synthetic";
    md.sourceFormat = "test";
    std::vector<Measurement> measurements;
    measurements.reserve(PapaPanRenderer::sectorCount);
    for (int sector = 0; sector < PapaPanRenderer::sectorCount; ++sector) {
        Measurement m;
        m.position = {static_cast<double>(sector * 15), 0.0, 1.0};
        const float g = 1.0f + 0.01f * static_cast<float>(sector);
        m.left = {g};
        m.right = {g};
        measurements.push_back(std::move(m));
    }
    return HRTFSet(sampleRate, std::move(md), std::move(measurements));
}

int main() {
    try {
        require(std::abs(wrapAzimuthDegrees(181.0) + 179.0) < 1e-9, "azimuth wrapping failed");
        require(std::abs(wrapAzimuthDegrees(270.0) + 90.0) < 1e-9, "270 degree right mapping failed");
        require(std::abs(azimuthForDisplay360(-90.0) - 270.0) < 1e-9, "display azimuth failed");

        // Distance keeps 1 m neutral, softens the near-field gain boost, and
        // adds only a restrained high-frequency loss at long distances.
        require(std::abs(DistanceModel::levelGain(1.0f) - 1.0f) < 1.0e-6f,
                "distance reference gain is not unity");
        require(std::abs(DistanceModel::levelGain(0.25f) - 2.0f) < 1.0e-6f,
                "near distance gain is not the intended softened +6 dB curve");
        require(std::abs(DistanceModel::levelGain(2.0f) - 0.5f) < 1.0e-6f,
                "far distance gain is not inverse-distance");
        require(std::abs(DistanceModel::airHighGain(1.0f) - 1.0f) < 1.0e-6f,
                "air shelf is not neutral at 1 m");
        require(DistanceModel::airHighGain(20.0f) < 1.0f &&
                DistanceModel::airHighGain(20.0f) > 0.70f,
                "air shelf is not restrained at maximum distance");
        DistanceModel distanceProbe;
        distanceProbe.prepare(48000.0);
        for (int i = 0; i < 4096; ++i) {
            const float x = std::sin(static_cast<float>(i) * 0.731f);
            const float y = distanceProbe.processSample(0, x, DistanceModel::airHighGain(1.0f));
            require(std::abs(y - x) < 1.0e-6f,
                    "1 m distance air-loss stage is not spectrally neutral");
        }
        distanceProbe.reset();
        for (int i = 0; i < 4096; ++i) {
            const float x = std::sin(static_cast<float>(i) * 1.113f);
            const float y = distanceProbe.processSample(0, x, 1.0f);
            require(std::abs(y - x) < 1.0e-6f,
                    "disabled air-loss stage changed the spectrum");
        }
        distanceProbe.reset();
        float dc = 1.0f;
        for (int i = 0; i < 4096; ++i) dc = distanceProbe.processSample(0, 1.0f, 0.75f);
        require(std::abs(dc - 1.0f) < 1.0e-4f,
                "distance air shelf changed steady-state DC gain");
        distanceProbe.reset();
        double hfAbs = 0.0;
        for (int i = 0; i < 4096; ++i) {
            const float x = (i & 1) ? -1.0f : 1.0f;
            const float y = distanceProbe.processSample(0, x, 0.75f);
            if (i >= 3096) hfAbs += std::abs(y);
        }
        require(hfAbs / 1000.0 < 0.85,
                "distance air shelf did not attenuate high-frequency energy");

        auto set = twoPointSet();
        std::string reason;
        require(set.valid(&reason), "valid test HRTF rejected");
        require(set.nearestMeasurementIndex({80.0, 0.0, 1.0}) == 0, "+90 must select listener-left");
        require(set.nearestMeasurementIndex({-80.0, 0.0, 1.0}) == 1, "-90 must select listener-right");

        const auto out = renderNearestOffline({1.0f}, set, {90.0, 0.0, 1.0});
        require(out.left.size() == 3 && out.right.size() == 3, "FIR output length failed");
        require(out.left[0] == 1.0f && out.right[1] == 0.5f, "FIR output values failed");

        CompensationProfile cp;
        cp.name = "test correction";
        cp.version = "1";
        cp.sampleRate = 48000.0;
        cp.impulse = {0.5f};
        require(cp.valid(&reason), "valid compensation rejected");
        const auto corrected = renderNearestOffline({1.0f}, set, {90.0, 0.0, 1.0}, &cp);
        require(std::abs(corrected.left[0] - 0.5f) < 1e-6f, "common compensation failed");

        require(defaultBuiltInDataset().id == BuiltInDatasetId::IrcamListen1050,
                "IRCAM LISTEN 1050 must be the built-in default");
        require(!datasetDescriptor(BuiltInDatasetId::MitKemarNormalPinna).isDefault,
                "KEMAR must remain the alternative dataset");

        DatasetPackage package;
        package.hrtf = set;
        package.datasetCompensation = cp;
        package.hasDatasetCompensation = true;
        const auto temp = std::filesystem::temp_directory_path() / "spacetrace_native_roundtrip.sthrtf";
        require(NativePackage::writeFile(temp, package, &reason), "native package write failed");
        auto loaded = NativePackage::loadFile(temp);
        require(static_cast<bool>(loaded), "native package read failed");
        require(loaded.package->hrtf.measurements().size() == 2, "native package measurement count failed");
        require(loaded.package->hrtf.metadata().processing == "synthetic test kernels",
                "native package processing metadata failed");
        require(loaded.package->hasDatasetCompensation, "native package compensation missing");
        require(std::abs(loaded.package->datasetCompensation.impulse[0] - 0.5f) < 1e-6f,
                "native package compensation value failed");
        std::filesystem::remove(temp);

        auto up = resampleHRTFSet(set, 96000.0);
        require(std::abs(up.sampleRate() - 96000.0) < 1e-9, "HRTF resample rate failed");
        require(up.measurements().front().left.size() >= 5, "HRTF resample length failed");

        // FIR resampling must preserve convolution gain rather than ordinary
        // waveform sample amplitude. A centered unit impulse has unity DC gain
        // and must remain unity across host sample rates.
        std::vector<float> unitFir(129, 0.0f);
        unitFir[64] = 1.0f;
        for (const double targetRate : {48000.0, 88200.0, 96000.0}) {
            const auto resampled = resampleImpulse(unitFir, 44100.0, targetRate);
            double dcGain = 0.0;
            for (const auto v : resampled) dcGain += v;
            require(std::abs(dcGain - 1.0) < 5.0e-3,
                    "FIR resampling changed unity convolution gain");
        }

        RealtimeFIRRenderer rt;
        rt.prepare(16, 48000.0, 1.0);
        rt.setHRTF(&set);
        rt.setPosition({90.0, 0.0, 1.0});
        const float in[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        float l[4] = {}, r[4] = {};
        rt.process(in, l, r, 4);
        require(std::abs(l[0] - 1.0f) < 1e-6f, "realtime FIR left impulse failed");
        require(std::abs(r[1] - 0.5f) < 1e-6f, "realtime FIR right impulse failed");

        // A position between measured directions must synthesize an intermediate
        // HRTF rather than remaining at one endpoint until the nearest-neighbor
        // boundary is crossed. With this symmetric two-point set, front is the
        // exact midpoint and therefore produces the same response in both ears.
        RealtimeFIRRenderer midpointRt;
        midpointRt.prepare(16, 48000.0, 1.0);
        midpointRt.setHRTF(&set);
        midpointRt.setPosition({0.0, 0.0, 1.0});
        float midpointIn[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        float midpointL[4] = {}, midpointR[4] = {};
        midpointRt.process(midpointIn, midpointL, midpointR, 4);
        require(midpointRt.currentMeasurementIndex() == static_cast<std::size_t>(-1),
                "intermediate position incorrectly snapped to a measured HRTF");
        require(std::abs(midpointL[0] - midpointR[0]) < 1e-6f &&
                std::abs(midpointL[1] - midpointR[1]) < 1e-6f,
                "midpoint interpolation is not symmetric");
        require(midpointL[0] > 0.45f && midpointL[0] < 0.55f,
                "midpoint interpolation did not blend neighboring HRTFs");

        // Papa Pan preserves the documented historical horizontal character:
        // 24 nearest 15-degree sectors, one-sector movement per 256 frames at
        // 44.1 kHz, and a sample-rate-invariant time cadence.
        auto papaSet = papaRingSet();
        PapaPanRenderer papa;
        require(papa.prepare(papaSet, 8, 44100.0), "Papa Pan failed to prepare a complete horizontal ring");
        require(papa.stepIntervalSamples() == 256, "Papa Pan 44.1-kHz cadence is not 256 frames");
        require(std::abs(PapaPanRenderer::frontGainForAzimuth(0.0) - 1.5f) < 1.0e-6f,
                "Papa Pan front gain must peak at 1.5");
        require(std::abs(PapaPanRenderer::frontGainForAzimuth(90.0) - 1.0f) < 1.0e-6f &&
                std::abs(PapaPanRenderer::frontGainForAzimuth(180.0) - 1.0f) < 1.0e-6f,
                "Papa Pan side/rear front-gain contour is wrong");

        papa.setAzimuth(7.4);
        float papaIn[1] = {1.0f}, papaL[1] = {}, papaR[1] = {};
        papa.process(papaIn, papaL, papaR, 1);
        require(papa.currentSector() == 0, "Papa Pan nearest-sector selection failed below midpoint");
        papa.reset();
        papa.setAzimuth(7.6);
        papa.process(papaIn, papaL, papaR, 1);
        require(papa.currentSector() == 1, "Papa Pan nearest-sector selection failed above midpoint");

        // A far target cannot teleport: after one historical interval it has
        // advanced only one 15-degree sector toward the target.
        papa.reset();
        papa.setAzimuth(0.0);
        std::vector<float> papaSilence(1, 0.0f), papaOutL(1), papaOutR(1);
        papa.process(papaSilence.data(), papaOutL.data(), papaOutR.data(), 1);
        papa.setAzimuth(90.0);
        std::vector<float> papaRun(255, 0.0f), papaRunL(255), papaRunR(255);
        papa.process(papaRun.data(), papaRunL.data(), papaRunR.data(), papaRun.size());
        require(papa.currentSector() == 1, "Papa Pan did not limit motion to one sector per historical interval");

        PapaPanRenderer papa96;
        auto papa96Set = resampleHRTFSet(papaSet, 96000.0);
        require(papa96.prepare(papa96Set, 8, 96000.0), "Papa Pan 96-kHz preparation failed");
        require(papa96.stepIntervalSamples() == 557, "Papa Pan movement cadence changed with sample rate");

        // The optimized circular FIR loop must remain sample-identical to the
        // offline reference after repeated ring-buffer wraparounds.
        Measurement wrapMeasurement;
        wrapMeasurement.position = {0.0, 0.0, 1.0};
        wrapMeasurement.left = {0.50f, -0.25f, 0.125f, 0.0625f, -0.03125f};
        wrapMeasurement.right = wrapMeasurement.left;
        HRTFSet wrapSet(48000.0, {}, {wrapMeasurement});
        std::vector<float> wrapInput(97);
        for (std::size_t i = 0; i < wrapInput.size(); ++i)
            wrapInput[i] = static_cast<float>(static_cast<int>((i * 17u) % 23u) - 11) / 11.0f;
        const auto wrapReference = renderNearestOffline(wrapInput, wrapSet, {0.0, 0.0, 1.0});
        RealtimeFIRRenderer wrapRt;
        wrapRt.prepare(8, 48000.0, 1.0);
        wrapRt.setHRTF(&wrapSet);
        wrapRt.setPosition({0.0, 0.0, 1.0});
        std::vector<float> wrapLeft(wrapInput.size()), wrapRight(wrapInput.size());
        wrapRt.process(wrapInput.data(), wrapLeft.data(), wrapRight.data(), wrapInput.size());
        for (std::size_t i = 0; i < wrapInput.size(); ++i) {
            require(std::abs(wrapLeft[i] - wrapReference.left[i]) < 1e-6f,
                    "optimized FIR wraparound diverged from offline left reference");
            require(std::abs(wrapRight[i] - wrapReference.right[i]) < 1e-6f,
                    "optimized FIR wraparound diverged from offline right reference");
        }

        // Switching HRTF sets must preserve the current copied kernel and crossfade
        // into the new set rather than clearing to silence.
        auto switched = twoPointSet();
        switched.measurements()[0].left = {0.25f, 0.0f, 0.0f};
        switched.measurements()[0].right = {0.0f, 0.25f, 0.0f};
        rt.setHRTF(&switched);
        require(rt.hasCurrentKernel(), "HRTF set switch discarded the running kernel");
        const float in2[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        float l2[4] = {}, r2[4] = {};
        rt.process(in2, l2, r2, 4);
        require(std::isfinite(l2[0]) && std::isfinite(r2[0]), "HRTF set crossfade produced invalid output");

        // Switching datasets during an active position fade must start from the
        // audible in-progress blend, not jump back to the fade's stale origin.
        Measurement switchA0, switchA1, switchB;
        switchA0.position = {0.0, 0.0, 1.0};
        switchA1.position = {90.0, 0.0, 1.0};
        switchB.position = {0.0, 0.0, 1.0};
        switchA0.left = switchA0.right = {1.0f};
        switchA1.left = switchA1.right = {3.0f};
        switchB.left = switchB.right = {5.0f};
        HRTFSet switchSetA(1000.0, {}, {switchA0, switchA1});
        HRTFSet switchSetB(1000.0, {}, {switchB});
        RealtimeFIRRenderer switchRt;
        switchRt.prepare(8, 1000.0, 20.0);
        switchRt.setHRTF(&switchSetA);
        switchRt.setPosition({0.0, 0.0, 1.0});
        float switchOne[1] = {1.0f}, switchOutL[1] = {}, switchOutR[1] = {};
        switchRt.process(switchOne, switchOutL, switchOutR, 1);
        switchRt.setPosition({90.0, 0.0, 1.0});
        float switchRun[10]; std::fill_n(switchRun, 10, 1.0f);
        float switchRunL[10] = {}, switchRunR[10] = {};
        switchRt.process(switchRun, switchRunL, switchRunR, 10);
        const float audibleBeforeSwitch = switchRunL[9];
        switchRt.setHRTF(&switchSetB);
        float switchAfter[1] = {1.0f}, switchAfterL[1] = {}, switchAfterR[1] = {};
        switchRt.process(switchAfter, switchAfterL, switchAfterR, 1);
        require(std::abs(switchAfterL[0] - audibleBeforeSwitch) < 0.12f,
                "dataset switch jumped back to stale transition origin");

        // A stationary position must not require a new measurement lookup, while
        // a changed position must still complete the normal kernel crossfade.
        const auto heldIndex = rt.currentMeasurementIndex();
        const float silence[2] = {};
        float silenceL[2] = {}, silenceR[2] = {};
        rt.process(silence, silenceL, silenceR, 2);
        require(rt.currentMeasurementIndex() == heldIndex, "stationary position changed measurement unexpectedly");
        rt.setPosition({-90.0, 0.0, 1.0});
        float transitionIn[64] = {};
        float transitionL[64] = {}, transitionR[64] = {};
        rt.process(transitionIn, transitionL, transitionR, 64);
        require(rt.currentMeasurementIndex() == 1, "position dirty-state did not select the changed direction");

        // Continuous automation can reverse before a crossfade completes. The
        // renderer must retarget from the audible in-progress blend rather than
        // ignoring the reversal because the old endpoint is still current.
        RealtimeFIRRenderer reverseRt;
        reverseRt.prepare(16, 48000.0, 1.0);  // 48-sample transition
        reverseRt.setHRTF(&set);
        reverseRt.setPosition({90.0, 0.0, 1.0});
        float reverseInit[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        float reverseInitL[4] = {}, reverseInitR[4] = {};
        reverseRt.process(reverseInit, reverseInitL, reverseInitR, 4);
        require(reverseRt.currentMeasurementIndex() == 0, "reverse test did not establish left endpoint");
        reverseRt.setPosition({-90.0, 0.0, 1.0});
        float reversePart[12] = {};
        float reversePartL[12] = {}, reversePartR[12] = {};
        reverseRt.process(reversePart, reversePartL, reversePartR, 12);
        reverseRt.setPosition({90.0, 0.0, 1.0});
        float reverseFinish[64] = {};
        float reverseFinishL[64] = {}, reverseFinishR[64] = {};
        reverseRt.process(reverseFinish, reverseFinishL, reverseFinishR, 64);
        require(reverseRt.currentMeasurementIndex() == 0,
                "mid-transition direction reversal completed at the stale target");

        Measurement broken;
        broken.position = {0.0, 0.0, 1.0};
        broken.left = {1.0f};
        HRTFSet invalid(48000.0, {}, {broken});
        require(!invalid.valid(&reason), "invalid HRTF accepted");

        std::cout << "SpaceTrace 0.5.0 core tests: PASS\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "SpaceTrace 0.5.0 core tests: FAIL: " << e.what() << '\n';
        return 1;
    }
}
