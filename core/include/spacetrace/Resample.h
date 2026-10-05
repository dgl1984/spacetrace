#pragma once

#include "spacetrace/Compensation.h"
#include "spacetrace/HRTFSet.h"

#include <vector>

namespace spacetrace {

[[nodiscard]] std::vector<float> resampleImpulse(const std::vector<float>& input,
                                                 double sourceRate,
                                                 double targetRate,
                                                 int halfKernel = 16);
[[nodiscard]] HRTFSet resampleHRTFSet(const HRTFSet& input, double targetRate);
[[nodiscard]] CompensationProfile resampleCompensation(const CompensationProfile& input,
                                                        double targetRate);

} // namespace spacetrace
