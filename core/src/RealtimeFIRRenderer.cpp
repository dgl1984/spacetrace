#include "spacetrace/RealtimeFIRRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace spacetrace {
namespace {
constexpr auto invalidIndex = static_cast<std::size_t>(-1);

void copyWithSimpleDelay(const std::vector<float>& src, std::size_t srcLength, double delay,
                         std::vector<float>& dst, std::size_t& length) noexcept {
    std::fill(dst.begin(), dst.end(), 0.0f);
    delay = std::max(0.0, delay);
    const auto integer = static_cast<std::size_t>(std::floor(delay));
    const double frac = delay - std::floor(delay);
    srcLength = std::min(srcLength, src.size());
    const auto needed = srcLength + integer + (frac > 1.0e-9 ? 1u : 0u);
    length = std::min(dst.size(), needed);
    for (std::size_t i = 0; i < srcLength; ++i) {
        const auto p = i + integer;
        if (p < dst.size()) dst[p] += src[i] * static_cast<float>(1.0 - frac);
        if (frac > 1.0e-9 && p + 1 < dst.size()) dst[p + 1] += src[i] * static_cast<float>(frac);
    }
}

void copyWithSimpleDelay(const Measurement& m, bool leftEar,
                         std::vector<float>& dst, std::size_t& length) noexcept {
    const auto& src = leftEar ? m.left : m.right;
    copyWithSimpleDelay(src, src.size(),
                        leftEar ? m.leftDelaySamples : m.rightDelaySamples,
                        dst, length);
}

}

void RealtimeFIRRenderer::prepare(std::size_t maxIRLength, double sampleRate, double transitionMs) {
    maxIRLength = std::max<std::size_t>(maxIRLength, 2);
    history_.assign(maxIRLength, 0.0f);
    currentLeft_.assign(maxIRLength, 0.0f);
    currentRight_.assign(maxIRLength, 0.0f);
    targetLeft_.assign(maxIRLength, 0.0f);
    targetRight_.assign(maxIRLength, 0.0f);
    scratchLeft_.assign(maxIRLength, 0.0f);
    scratchRight_.assign(maxIRLength, 0.0f);
    transitionSamples_ = static_cast<std::size_t>(std::max(1.0, sampleRate * transitionMs / 1000.0));
    reset();
}

void RealtimeFIRRenderer::reset() noexcept {
    std::fill(history_.begin(), history_.end(), 0.0f);
    std::fill(currentLeft_.begin(), currentLeft_.end(), 0.0f);
    std::fill(currentRight_.begin(), currentRight_.end(), 0.0f);
    std::fill(targetLeft_.begin(), targetLeft_.end(), 0.0f);
    std::fill(targetRight_.begin(), targetRight_.end(), 0.0f);
    std::fill(scratchLeft_.begin(), scratchLeft_.end(), 0.0f);
    std::fill(scratchRight_.begin(), scratchRight_.end(), 0.0f);
    historyWrite_ = 0;
    currentIndex_ = invalidIndex;
    targetIndex_ = invalidIndex;
    transitionRemaining_ = 0;
    currentLeftLength_ = currentRightLength_ = targetLeftLength_ = targetRightLength_ = 0;
    hasCurrentKernel_ = false;
    positionDirty_ = true;
}

void RealtimeFIRRenderer::setHRTF(const HRTFSet* set) noexcept {
    if (set_ == set) return;

    // A structural dataset switch can arrive while a position transition is
    // audible. Preserve that exact in-progress blend as the new starting
    // kernel before changing the source set; otherwise clearing the transition
    // here would jump back to its stale starting endpoint.
    collapseTransitionToCurrent();

    set_ = set;
    positionDirty_ = true;
    currentIndex_ = invalidIndex;
    targetIndex_ = invalidIndex;
}

void RealtimeFIRRenderer::setPosition(SphericalPosition position) noexcept {
    if (position.azimuthDegrees == position_.azimuthDegrees &&
        position.elevationDegrees == position_.elevationDegrees &&
        position.distanceMetres == position_.distanceMetres)
        return;
    position_ = position;
    positionDirty_ = true;
}

void RealtimeFIRRenderer::collapseTransitionToCurrent() noexcept {
    if (transitionRemaining_ == 0 || !hasCurrentKernel_) return;

    const float t = 1.0f - static_cast<float>(transitionRemaining_) /
                               static_cast<float>(transitionSamples_);
    const auto leftLength = std::max(currentLeftLength_, targetLeftLength_);
    const auto rightLength = std::max(currentRightLength_, targetRightLength_);
    for (std::size_t i = 0; i < leftLength; ++i)
        currentLeft_[i] += (targetLeft_[i] - currentLeft_[i]) * t;
    for (std::size_t i = 0; i < rightLength; ++i)
        currentRight_[i] += (targetRight_[i] - currentRight_[i]) * t;
    currentLeftLength_ = leftLength;
    currentRightLength_ = rightLength;
    currentIndex_ = invalidIndex;
    targetIndex_ = invalidIndex;
    transitionRemaining_ = 0;
}

void RealtimeFIRRenderer::requestPositionKernel(const SphericalPosition& position) noexcept {
    if (set_ == nullptr || set_->measurements().empty()) return;
    if (transitionRemaining_ > 0) collapseTransitionToCurrent();

    const auto& measurements = set_->measurements();
    const double targetAz = wrapAzimuthDegrees(position.azimuthDegrees);
    const double targetEl = std::clamp(position.elevationDegrees, -90.0, 90.0);
    constexpr double ringEpsilon = 1.0e-4;

    double lowerEl = -std::numeric_limits<double>::infinity();
    double upperEl = std::numeric_limits<double>::infinity();
    for (const auto& m : measurements) {
        const double el = m.position.elevationDegrees;
        if (el <= targetEl + ringEpsilon && el > lowerEl) lowerEl = el;
        if (el >= targetEl - ringEpsilon && el < upperEl) upperEl = el;
    }
    if (!std::isfinite(lowerEl)) lowerEl = upperEl;
    if (!std::isfinite(upperEl)) upperEl = lowerEl;
    if (!std::isfinite(lowerEl) || !std::isfinite(upperEl)) return;

    struct RingBlend {
        std::size_t lower = invalidIndex;
        std::size_t upper = invalidIndex;
        double upperWeight = 0.0;
        bool valid = false;
    };

    auto ringBlend = [&](double ringEl) noexcept {
        RingBlend out;
        double bestBack = std::numeric_limits<double>::infinity();
        double bestForward = std::numeric_limits<double>::infinity();
        for (std::size_t i = 0; i < measurements.size(); ++i) {
            const auto& m = measurements[i];
            if (std::abs(m.position.elevationDegrees - ringEl) > ringEpsilon) continue;
            const double az = wrapAzimuthDegrees(m.position.azimuthDegrees);
            double back = std::fmod(targetAz - az + 360.0, 360.0);
            double forward = std::fmod(az - targetAz + 360.0, 360.0);
            if (back < 0.0) back += 360.0;
            if (forward < 0.0) forward += 360.0;
            if (back < bestBack) { bestBack = back; out.lower = i; }
            if (forward < bestForward) { bestForward = forward; out.upper = i; }
        }
        if (out.lower == invalidIndex || out.upper == invalidIndex) return out;
        const double span = bestBack + bestForward;
        out.upperWeight = (out.lower == out.upper || span <= 1.0e-12) ? 0.0 : bestBack / span;
        out.valid = true;
        return out;
    };

    const auto low = ringBlend(lowerEl);
    const auto high = ringBlend(upperEl);
    if (!low.valid || !high.valid) {
        const auto nearest = set_->nearestMeasurementIndex(position);
        const auto& m = measurements[nearest];
        copyWithSimpleDelay(m, true, targetLeft_, targetLeftLength_);
        copyWithSimpleDelay(m, false, targetRight_, targetRightLength_);
        targetIndex_ = nearest;
    } else {
        std::fill(scratchLeft_.begin(), scratchLeft_.end(), 0.0f);
        std::fill(scratchRight_.begin(), scratchRight_.end(), 0.0f);

        const double elevationWeight = std::abs(upperEl - lowerEl) <= ringEpsilon
            ? 0.0
            : std::clamp((targetEl - lowerEl) / (upperEl - lowerEl), 0.0, 1.0);

        struct WeightedMeasurement { std::size_t index; double weight; };
        const std::array<WeightedMeasurement, 4> corners {{
            {low.lower,  (1.0 - elevationWeight) * (1.0 - low.upperWeight)},
            {low.upper,  (1.0 - elevationWeight) * low.upperWeight},
            {high.lower, elevationWeight * (1.0 - high.upperWeight)},
            {high.upper, elevationWeight * high.upperWeight},
        }};

        double weightSum = 0.0;
        double leftDelay = 0.0;
        double rightDelay = 0.0;
        std::size_t baseLeftLength = 0;
        std::size_t baseRightLength = 0;
        std::size_t dominantIndex = invalidIndex;
        double dominantWeight = 0.0;
        for (const auto& corner : corners) {
            if (!(corner.weight > 0.0) || corner.index == invalidIndex) continue;
            const auto& m = measurements[corner.index];
            weightSum += corner.weight;
            if (corner.weight > dominantWeight) { dominantWeight = corner.weight; dominantIndex = corner.index; }
            leftDelay += corner.weight * std::max(0.0, m.leftDelaySamples);
            rightDelay += corner.weight * std::max(0.0, m.rightDelaySamples);
            baseLeftLength = std::max(baseLeftLength, m.left.size());
            baseRightLength = std::max(baseRightLength, m.right.size());
            const auto leftN = std::min(scratchLeft_.size(), m.left.size());
            const auto rightN = std::min(scratchRight_.size(), m.right.size());
            const float wf = static_cast<float>(corner.weight);
            for (std::size_t k = 0; k < leftN; ++k) scratchLeft_[k] += m.left[k] * wf;
            for (std::size_t k = 0; k < rightN; ++k) scratchRight_[k] += m.right[k] * wf;
        }

        if (!(weightSum > 0.0) || !std::isfinite(weightSum)) return;
        const float inv = static_cast<float>(1.0 / weightSum);
        baseLeftLength = std::min(baseLeftLength, scratchLeft_.size());
        baseRightLength = std::min(baseRightLength, scratchRight_.size());
        for (std::size_t k = 0; k < baseLeftLength; ++k) scratchLeft_[k] *= inv;
        for (std::size_t k = 0; k < baseRightLength; ++k) scratchRight_[k] *= inv;
        leftDelay /= weightSum;
        rightDelay /= weightSum;

        copyWithSimpleDelay(scratchLeft_, baseLeftLength, leftDelay, targetLeft_, targetLeftLength_);
        copyWithSimpleDelay(scratchRight_, baseRightLength, rightDelay, targetRight_, targetRightLength_);
        targetIndex_ = (dominantWeight >= 1.0 - 1.0e-9) ? dominantIndex : invalidIndex;
    }

    if (!hasCurrentKernel_) {
        currentLeft_.swap(targetLeft_);
        currentRight_.swap(targetRight_);
        currentLeftLength_ = targetLeftLength_;
        currentRightLength_ = targetRightLength_;
        currentIndex_ = targetIndex_;
        targetIndex_ = invalidIndex;
        transitionRemaining_ = 0;
        hasCurrentKernel_ = true;
    } else {
        transitionRemaining_ = transitionSamples_;
    }
}

float RealtimeFIRRenderer::processKernel(const std::vector<float>& kernel, std::size_t kernelLength) const noexcept {
    if (history_.empty()) return 0.0f;
    float sum = 0.0f;
    const auto n = std::min(kernelLength, history_.size());

    const auto beforeWrap = std::min(n, historyWrite_);
    for (std::size_t k = 0; k < beforeWrap; ++k)
        sum += history_[historyWrite_ - 1u - k] * kernel[k];

    const auto remaining = n - beforeWrap;
    for (std::size_t k = 0; k < remaining; ++k)
        sum += history_[history_.size() - 1u - k] * kernel[beforeWrap + k];

    return sum;
}

void RealtimeFIRRenderer::process(const float* mono, float* left, float* right, std::size_t numSamples) noexcept {
    if (set_ == nullptr || history_.empty() || mono == nullptr || left == nullptr || right == nullptr) {
        if (left) std::fill(left, left + numSamples, 0.0f);
        if (right) std::fill(right, right + numSamples, 0.0f);
        return;
    }

    if (positionDirty_) {
        requestPositionKernel(position_);
        positionDirty_ = false;
    }

    for (std::size_t i = 0; i < numSamples; ++i) {
        history_[historyWrite_] = mono[i];
        historyWrite_ = (historyWrite_ + 1) % history_.size();

        const float aL = processKernel(currentLeft_, currentLeftLength_);
        const float aR = processKernel(currentRight_, currentRightLength_);
        if (transitionRemaining_ == 0) {
            left[i] = aL;
            right[i] = aR;
            continue;
        }

        const float bL = processKernel(targetLeft_, targetLeftLength_);
        const float bR = processKernel(targetRight_, targetRightLength_);
        const float t = 1.0f - static_cast<float>(transitionRemaining_) / static_cast<float>(transitionSamples_);
        left[i] = aL + (bL - aL) * t;
        right[i] = aR + (bR - aR) * t;

        if (--transitionRemaining_ == 0) {
            currentLeft_.swap(targetLeft_);
            currentRight_.swap(targetRight_);
            currentLeftLength_ = targetLeftLength_;
            currentRightLength_ = targetRightLength_;
            currentIndex_ = targetIndex_;
            targetIndex_ = invalidIndex;
        }
    }
}

} // namespace spacetrace
