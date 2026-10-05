#include "spacetrace/HRTFSet.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace spacetrace {
namespace {
constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double degToRad(double d) noexcept { return d * kPi / 180.0; }
}

double wrapAzimuthDegrees(double degrees) noexcept {
    double v = std::fmod(degrees + 180.0, 360.0);
    if (v < 0.0) v += 360.0;
    return v - 180.0;
}

double azimuthForDisplay360(double canonicalDegrees) noexcept {
    double v = std::fmod(canonicalDegrees, 360.0);
    if (v < 0.0) v += 360.0;
    return v;
}

double angularDistanceDegrees(const SphericalPosition& a,
                              const SphericalPosition& b) noexcept {
    const double az1 = degToRad(wrapAzimuthDegrees(a.azimuthDegrees));
    const double az2 = degToRad(wrapAzimuthDegrees(b.azimuthDegrees));
    const double el1 = degToRad(a.elevationDegrees);
    const double el2 = degToRad(b.elevationDegrees);

    const double dot = std::sin(el1) * std::sin(el2)
                     + std::cos(el1) * std::cos(el2) * std::cos(az1 - az2);
    const double clamped = std::clamp(dot, -1.0, 1.0);
    return std::acos(clamped) * 180.0 / kPi;
}

HRTFSet::HRTFSet(double sampleRate, HRTFMetadata metadata, std::vector<Measurement> measurements)
    : sampleRate_(sampleRate), metadata_(std::move(metadata)), measurements_(std::move(measurements)) {}

bool HRTFSet::valid(std::string* reason) const {
    auto fail = [&](const char* text) {
        if (reason) *reason = text;
        return false;
    };

    if (!(sampleRate_ > 0.0) || !std::isfinite(sampleRate_))
        return fail("sample rate must be finite and greater than zero");
    if (measurements_.empty())
        return fail("HRTF contains no measurements");

    for (const auto& m : measurements_) {
        if (m.left.empty() || m.right.empty())
            return fail("measurement has an empty ear impulse response");
        if (m.left.size() != m.right.size())
            return fail("left and right impulse responses have different lengths");
        if (!std::isfinite(m.position.azimuthDegrees) ||
            !std::isfinite(m.position.elevationDegrees) ||
            !std::isfinite(m.position.distanceMetres) ||
            !std::isfinite(m.leftDelaySamples) ||
            !std::isfinite(m.rightDelaySamples))
            return fail("measurement contains a non-finite position or delay");
        if (!(m.position.distanceMetres > 0.0))
            return fail("measurement distance must be greater than zero");
        if (m.position.elevationDegrees < -90.0001 || m.position.elevationDegrees > 90.0001)
            return fail("measurement elevation is outside -90..+90 degrees");
    }

    if (reason) reason->clear();
    return true;
}

std::size_t HRTFSet::nearestMeasurementIndex(const SphericalPosition& target) const {
    if (measurements_.empty())
        throw std::runtime_error("nearestMeasurementIndex called on empty HRTFSet");

    std::size_t best = 0;
    double bestScore = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < measurements_.size(); ++i) {
        const auto& p = measurements_[i].position;
        const double angle = angularDistanceDegrees(target, p);
        const double radial = std::abs(target.distanceMetres - p.distanceMetres);
        const double score = angle + radial * 0.01;
        if (score < bestScore) {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

const Measurement& HRTFSet::nearestMeasurement(const SphericalPosition& target) const {
    return measurements_.at(nearestMeasurementIndex(target));
}

} // namespace spacetrace
