#pragma once

#include "spacetrace/HRTFSet.h"

#include <cstddef>
#include <vector>

namespace spacetrace {

// Allocation-free after prepare(). This stage handles only the directional HRTF.
// Common dataset compensation is intentionally a separate processor stage.
class RealtimeFIRRenderer {
public:
    void prepare(std::size_t maxIRLength, double sampleRate, double transitionMs = 20.0);
    void reset() noexcept;

    // The pointed-to set must remain alive while it is selected. Changing sets keeps
    // the current copied FIR kernel and crossfades into the first kernel requested
    // from the new set; it does not abruptly clear the renderer.
    void setHRTF(const HRTFSet* set) noexcept;
    void setPosition(SphericalPosition position) noexcept;

    void process(const float* mono, float* left, float* right, std::size_t numSamples) noexcept;

    [[nodiscard]] std::size_t currentMeasurementIndex() const noexcept { return currentIndex_; }
    [[nodiscard]] bool hasCurrentKernel() const noexcept { return hasCurrentKernel_; }

private:
    void collapseTransitionToCurrent() noexcept;
    void requestPositionKernel(const SphericalPosition& position) noexcept;
    float processKernel(const std::vector<float>& kernel, std::size_t kernelLength) const noexcept;

    const HRTFSet* set_ = nullptr;
    SphericalPosition position_ {};
    std::vector<float> history_;
    std::vector<float> currentLeft_, currentRight_, targetLeft_, targetRight_;
    std::vector<float> scratchLeft_, scratchRight_;
    std::size_t historyWrite_ = 0;
    std::size_t currentLeftLength_ = 0, currentRightLength_ = 0;
    std::size_t targetLeftLength_ = 0, targetRightLength_ = 0;
    std::size_t currentIndex_ = static_cast<std::size_t>(-1);
    std::size_t targetIndex_ = static_cast<std::size_t>(-1);
    std::size_t transitionSamples_ = 1;
    std::size_t transitionRemaining_ = 0;
    bool hasCurrentKernel_ = false;
    bool positionDirty_ = true;
};

} // namespace spacetrace
