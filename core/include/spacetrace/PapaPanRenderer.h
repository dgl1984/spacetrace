#pragma once

#include "spacetrace/HRTFSet.h"

#include <array>
#include <cstddef>
#include <vector>

namespace spacetrace {

// Historical Papa-style horizontal renderer reconstructed from documented Papa
// Engine behaviour while using SpaceTrace's official IRCAM LISTEN 1050 data.
//
// Behaviour intentionally differs from RealtimeFIRRenderer:
// - horizontal IRCAM ring only (24 directions, 15 degrees apart)
// - nearest 15-degree sector rather than interpolation
// - sector can advance by at most one step per historical 256-frame/44.1-kHz interval
// - documented Papa front-hemisphere gain contour is applied inside this renderer
//
// All storage is allocated/prebuilt in prepare(); setAzimuth/process are realtime-safe.
class PapaPanRenderer {
public:
    static constexpr int sectorCount = 24;
    static constexpr double referenceSampleRate = 44100.0;
    static constexpr std::size_t referenceFramesPerStep = 256;

    bool prepare(const HRTFSet& ircam, std::size_t maxIRLength, double sampleRate);
    void reset() noexcept;
    void setAzimuth(double azimuthDegrees) noexcept;
    void process(const float* mono, float* left, float* right, std::size_t numSamples) noexcept;

    [[nodiscard]] bool ready() const noexcept { return ready_; }
    [[nodiscard]] int currentSector() const noexcept { return currentSector_; }
    [[nodiscard]] int targetSector() const noexcept { return targetSector_; }
    [[nodiscard]] std::size_t stepIntervalSamples() const noexcept { return stepIntervalSamples_; }

    [[nodiscard]] static float frontGainForAzimuth(double azimuthDegrees) noexcept;

private:
    static int sectorForAzimuth(double azimuthDegrees) noexcept;
    static int shortestStep(int current, int target) noexcept;
    void loadSectorKernel(int sector) noexcept;
    float processKernel(const std::vector<float>& kernel, std::size_t length) const noexcept;

    std::array<std::vector<float>, sectorCount> leftKernels_;
    std::array<std::vector<float>, sectorCount> rightKernels_;
    std::array<std::size_t, sectorCount> leftLengths_ {};
    std::array<std::size_t, sectorCount> rightLengths_ {};
    std::vector<float> history_;
    std::size_t historyWrite_ = 0;
    std::size_t currentLeftLength_ = 0;
    std::size_t currentRightLength_ = 0;
    const std::vector<float>* currentLeft_ = nullptr;
    const std::vector<float>* currentRight_ = nullptr;
    std::size_t stepIntervalSamples_ = referenceFramesPerStep;
    std::size_t samplesUntilStep_ = referenceFramesPerStep;
    int currentSector_ = -1;
    int targetSector_ = 0;
    double requestedAzimuth_ = 0.0;
    bool ready_ = false;
};

} // namespace spacetrace
