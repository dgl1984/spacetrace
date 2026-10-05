#pragma once

#include "spacetrace/HRTFSet.h"

#include <algorithm>

namespace spacetrace::plugin {

struct StereoPairGeometry {
    double leftAzimuthDegrees = 90.0;
    double rightAzimuthDegrees = 270.0;
};

inline StereoPairGeometry stereoPairGeometry(double centreAzimuthDegrees,
                                             double widthOffsetDegrees) noexcept {
    const auto width = std::clamp(widthOffsetDegrees, -45.0, 45.0);
    return {
        wrapAzimuthDegrees(centreAzimuthDegrees + 90.0 + width),
        wrapAzimuthDegrees(centreAzimuthDegrees - 90.0 - width)
    };
}

} // namespace spacetrace::plugin
