#pragma once

#include "spacetrace/Compensation.h"
#include "spacetrace/HRTFSet.h"

#include <vector>

namespace spacetrace {

struct StereoBuffer {
    std::vector<float> left;
    std::vector<float> right;
};

[[nodiscard]] StereoBuffer renderNearestOffline(const std::vector<float>& mono,
                                                 const HRTFSet& hrtf,
                                                 const SphericalPosition& position,
                                                 const CompensationProfile* commonCompensation = nullptr);

} // namespace spacetrace
