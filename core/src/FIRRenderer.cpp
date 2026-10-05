#include "spacetrace/FIRRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace spacetrace {
namespace {
std::vector<float> convolve(const std::vector<float>& x, const std::vector<float>& h) {
    if (x.empty() || h.empty()) return {};
    std::vector<float> y(x.size() + h.size() - 1, 0.0f);
    for (std::size_t n = 0; n < x.size(); ++n)
        for (std::size_t k = 0; k < h.size(); ++k)
            y[n + k] += x[n] * h[k];
    return y;
}

std::vector<float> withDelay(const std::vector<float>& h, double delaySamples) {
    if (h.empty() || std::abs(delaySamples) < 1.0e-9) return h;
    const auto integer = static_cast<std::size_t>(std::max(0.0, std::floor(delaySamples)));
    const double frac = std::clamp(delaySamples - std::floor(delaySamples), 0.0, 0.999999);
    std::vector<float> out(h.size() + integer + 2, 0.0f);
    for (std::size_t i = 0; i < h.size(); ++i) {
        const auto p = i + integer;
        out[p] += h[i] * static_cast<float>(1.0 - frac);
        out[p + 1] += h[i] * static_cast<float>(frac);
    }
    return out;
}
}

StereoBuffer renderNearestOffline(const std::vector<float>& mono,
                                  const HRTFSet& hrtf,
                                  const SphericalPosition& position,
                                  const CompensationProfile* commonCompensation) {
    const auto& m = hrtf.nearestMeasurement(position);
    auto leftIR = withDelay(m.left, m.leftDelaySamples);
    auto rightIR = withDelay(m.right, m.rightDelaySamples);

    auto left = convolve(mono, leftIR);
    auto right = convolve(mono, rightIR);
    if (commonCompensation != nullptr && !commonCompensation->impulse.empty()) {
        left = convolve(left, commonCompensation->impulse);
        right = convolve(right, commonCompensation->impulse);
    }
    return {std::move(left), std::move(right)};
}

} // namespace spacetrace
