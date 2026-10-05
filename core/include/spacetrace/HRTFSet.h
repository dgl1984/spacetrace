#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace spacetrace {

// SpaceTrace canonical listener coordinates:
//   0 degrees = front
//  +90 degrees = listener-left
//  180/-180 = rear
//  -90 degrees = listener-right (equivalent to 270 degrees in 0..360 displays)
struct SphericalPosition {
    double azimuthDegrees = 0.0;
    double elevationDegrees = 0.0;
    double distanceMetres = 1.0;
};

struct Measurement {
    SphericalPosition position;
    std::vector<float> left;
    std::vector<float> right;
    double leftDelaySamples = 0.0;
    double rightDelaySamples = 0.0;
};

struct HRTFMetadata {
    std::string name;
    std::string subject;
    std::string sourceFormat;
    std::string sourcePath;
    std::string sourceUrl;
    std::string database;
    std::string licence;
    std::string attribution;
    std::string contentSha256;
    std::string processing;
};

class HRTFSet {
public:
    HRTFSet() = default;
    HRTFSet(double sampleRate, HRTFMetadata metadata, std::vector<Measurement> measurements);

    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] const HRTFMetadata& metadata() const noexcept { return metadata_; }
    [[nodiscard]] const std::vector<Measurement>& measurements() const noexcept { return measurements_; }
    [[nodiscard]] std::vector<Measurement>& measurements() noexcept { return measurements_; }

    [[nodiscard]] bool valid(std::string* reason = nullptr) const;
    [[nodiscard]] std::size_t nearestMeasurementIndex(const SphericalPosition& target) const;
    [[nodiscard]] const Measurement& nearestMeasurement(const SphericalPosition& target) const;

private:
    double sampleRate_ = 0.0;
    HRTFMetadata metadata_;
    std::vector<Measurement> measurements_;
};

[[nodiscard]] double wrapAzimuthDegrees(double degrees) noexcept;
[[nodiscard]] double azimuthForDisplay360(double canonicalDegrees) noexcept;
[[nodiscard]] double angularDistanceDegrees(const SphericalPosition& a,
                                            const SphericalPosition& b) noexcept;

} // namespace spacetrace
