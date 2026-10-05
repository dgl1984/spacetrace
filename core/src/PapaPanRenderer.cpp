#include "spacetrace/PapaPanRenderer.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace spacetrace {
namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr auto invalidIndex = static_cast<std::size_t>(-1);

void copyWithSimpleDelay(const std::vector<float>& src, double delay,
                         std::vector<float>& dst, std::size_t& length) noexcept {
    std::fill(dst.begin(), dst.end(), 0.0f);
    delay = std::max(0.0, delay);
    const auto integer = static_cast<std::size_t>(std::floor(delay));
    const double frac = delay - std::floor(delay);
    const auto needed = src.size() + integer + (frac > 1.0e-9 ? 1u : 0u);
    length = std::min(dst.size(), needed);
    for (std::size_t i = 0; i < src.size(); ++i) {
        const auto p = i + integer;
        if (p < dst.size()) dst[p] += src[i] * static_cast<float>(1.0 - frac);
        if (frac > 1.0e-9 && p + 1 < dst.size()) dst[p + 1] += src[i] * static_cast<float>(frac);
    }
}
}

bool PapaPanRenderer::prepare(const HRTFSet& ircam, std::size_t maxIRLength, double sampleRate) {
    ready_ = false;
    maxIRLength = std::max<std::size_t>(maxIRLength, 2);
    history_.assign(maxIRLength, 0.0f);
    for (auto& k : leftKernels_) k.assign(maxIRLength, 0.0f);
    for (auto& k : rightKernels_) k.assign(maxIRLength, 0.0f);
    leftLengths_.fill(0);
    rightLengths_.fill(0);

    if (!(sampleRate > 0.0) || ircam.measurements().empty()) {
        reset();
        return false;
    }

    for (int sector = 0; sector < sectorCount; ++sector) {
        const double wantedAz = static_cast<double>(sector * 15);
        std::size_t best = invalidIndex;
        double bestError = std::numeric_limits<double>::infinity();
        for (std::size_t i = 0; i < ircam.measurements().size(); ++i) {
            const auto& m = ircam.measurements()[i];
            if (std::abs(m.position.elevationDegrees) > 1.0e-4) continue;
            double d = std::abs(azimuthForDisplay360(m.position.azimuthDegrees) - wantedAz);
            d = std::min(d, 360.0 - d);
            if (d < bestError) {
                bestError = d;
                best = i;
            }
        }
        if (best == invalidIndex || bestError > 1.0e-3) {
            reset();
            return false;
        }
        const auto& m = ircam.measurements()[best];
        copyWithSimpleDelay(m.left, m.leftDelaySamples, leftKernels_[sector], leftLengths_[sector]);
        copyWithSimpleDelay(m.right, m.rightDelaySamples, rightKernels_[sector], rightLengths_[sector]);
    }

    stepIntervalSamples_ = static_cast<std::size_t>(std::max(
        1.0, std::round(sampleRate * static_cast<double>(referenceFramesPerStep) / referenceSampleRate)));
    ready_ = true;
    reset();
    return true;
}

void PapaPanRenderer::reset() noexcept {
    std::fill(history_.begin(), history_.end(), 0.0f);
    historyWrite_ = 0;
    currentLeft_ = nullptr;
    currentRight_ = nullptr;
    currentLeftLength_ = currentRightLength_ = 0;
    currentSector_ = -1;
    targetSector_ = sectorForAzimuth(requestedAzimuth_);
    samplesUntilStep_ = stepIntervalSamples_;
}

int PapaPanRenderer::sectorForAzimuth(double azimuthDegrees) noexcept {
    const double display = azimuthForDisplay360(azimuthDegrees);
    int sector = static_cast<int>(std::floor(display / 15.0 + 0.5)) % sectorCount;
    if (sector < 0) sector += sectorCount;
    return sector;
}

int PapaPanRenderer::shortestStep(int current, int target) noexcept {
    if (current == target) return 0;
    const int clockwise = (target - current + sectorCount) % sectorCount;
    return clockwise <= sectorCount / 2 ? 1 : -1;
}

void PapaPanRenderer::setAzimuth(double azimuthDegrees) noexcept {
    requestedAzimuth_ = wrapAzimuthDegrees(azimuthDegrees);
    targetSector_ = sectorForAzimuth(requestedAzimuth_);
}

float PapaPanRenderer::frontGainForAzimuth(double azimuthDegrees) noexcept {
    const double radians = wrapAzimuthDegrees(azimuthDegrees) * kPi / 180.0;
    const double ux = std::cos(radians);
    if (!(ux > 0.0)) return 1.0f;
    const double uy = std::sin(radians);
    const double gain = (std::sin((uy + 0.5) * kPi) * 0.5 + 0.5) * 0.5 + 1.0;
    return static_cast<float>(gain);
}

void PapaPanRenderer::loadSectorKernel(int sector) noexcept {
    sector %= sectorCount;
    if (sector < 0) sector += sectorCount;
    currentSector_ = sector;
    currentLeft_ = &leftKernels_[sector];
    currentRight_ = &rightKernels_[sector];
    currentLeftLength_ = leftLengths_[sector];
    currentRightLength_ = rightLengths_[sector];
}

float PapaPanRenderer::processKernel(const std::vector<float>& kernel, std::size_t length) const noexcept {
    if (history_.empty()) return 0.0f;
    float sum = 0.0f;
    const auto n = std::min(length, history_.size());
    const auto beforeWrap = std::min(n, historyWrite_);
    for (std::size_t k = 0; k < beforeWrap; ++k)
        sum += history_[historyWrite_ - 1u - k] * kernel[k];
    const auto remaining = n - beforeWrap;
    for (std::size_t k = 0; k < remaining; ++k)
        sum += history_[history_.size() - 1u - k] * kernel[beforeWrap + k];
    return sum;
}

void PapaPanRenderer::process(const float* mono, float* left, float* right, std::size_t numSamples) noexcept {
    if (!ready_ || history_.empty() || mono == nullptr || left == nullptr || right == nullptr) {
        if (left) std::fill(left, left + numSamples, 0.0f);
        if (right) std::fill(right, right + numSamples, 0.0f);
        return;
    }
    if (currentSector_ < 0) loadSectorKernel(targetSector_);
    const float frontGain = frontGainForAzimuth(requestedAzimuth_);

    for (std::size_t i = 0; i < numSamples; ++i) {
        history_[historyWrite_] = mono[i];
        historyWrite_ = (historyWrite_ + 1u) % history_.size();
        left[i] = processKernel(*currentLeft_, currentLeftLength_) * frontGain;
        right[i] = processKernel(*currentRight_, currentRightLength_) * frontGain;

        if (--samplesUntilStep_ == 0) {
            if (const int step = shortestStep(currentSector_, targetSector_); step != 0)
                loadSectorKernel(currentSector_ + step);
            samplesUntilStep_ = stepIntervalSamples_;
        }
    }
}

} // namespace spacetrace
