#include "spacetrace/DistanceModel.h"

#include <algorithm>
#include <cmath>

namespace spacetrace {
namespace {
constexpr float referenceDistance = 1.0f;
constexpr float minimumDistance = 0.25f;
constexpr float airLossDbPerMetreBeyondReference = 0.15f;
constexpr float maximumAirLossDb = 3.0f;
constexpr float airShelfCornerHz = 5000.0f;
}

float DistanceModel::levelGain(float metres) noexcept {
    const float d = std::max(minimumDistance, metres);
    if (d >= referenceDistance)
        return referenceDistance / d;
    // Full inverse-distance gain would be +12 dB at 0.25 m. That exaggerates
    // proximity without measured near-field HRTFs, so use a softened curve.
    return std::sqrt(referenceDistance / d);
}

float DistanceModel::airHighGain(float metres) noexcept {
    const float d = std::max(referenceDistance, metres);
    const float lossDb = std::min(maximumAirLossDb,
                                  (d - referenceDistance) * airLossDbPerMetreBeyondReference);
    return std::pow(10.0f, -lossDb / 20.0f);
}

void DistanceModel::prepare(double sampleRate) noexcept {
    if (sampleRate <= 0.0) {
        lowpassAlpha_ = 1.0f;
        reset();
        return;
    }
    constexpr double twoPi = 6.28318530717958647692;
    lowpassAlpha_ = static_cast<float>(1.0 - std::exp(-twoPi * airShelfCornerHz / sampleRate));
    reset();
}

void DistanceModel::reset() noexcept {
    lowState_ = {0.0f, 0.0f};
    initialized_ = {false, false};
}

float DistanceModel::processSample(std::size_t channel, float input, float highGain) noexcept {
    const auto index = std::min<std::size_t>(channel, 1u);
    auto& low = lowState_[index];
    if (!initialized_[index]) {
        low = input;
        initialized_[index] = true;
    } else {
        low += lowpassAlpha_ * (input - low);
    }
    // Low frequencies pass at unity; the residual high-frequency component is
    // attenuated according to distance. No allocation, latency, or lookahead.
    return low + std::clamp(highGain, 0.0f, 1.0f) * (input - low);
}

} // namespace spacetrace
