#include "spacetrace/DatasetCatalog.h"
#include "spacetrace/FIRRenderer.h"
#include "spacetrace/HRTFSet.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace spacetrace;

static HRTFSet makeSyntheticRing() {
    HRTFMetadata md;
    md.name = "SpaceTrace synthetic probe ring";
    md.subject = "not for listening tests";
    md.sourceFormat = "synthetic";

    std::vector<Measurement> ms;
    for (int az = -180; az < 180; az += 45) {
        Measurement m;
        m.position = {static_cast<double>(az), 0.0, 1.0};
        const double pan = std::sin(az * 3.14159265358979323846 / 180.0); // positive = left
        m.left = {static_cast<float>(0.75 + 0.25 * pan), 0.0f, 0.0f};
        m.right = {static_cast<float>(0.75 - 0.25 * pan), 0.0f, 0.0f};
        ms.push_back(std::move(m));
    }
    return HRTFSet(48000.0, std::move(md), std::move(ms));
}

int main(int argc, char** argv) {
    const double az = argc > 1 ? std::atof(argv[1]) : 0.0;
    const double el = argc > 2 ? std::atof(argv[2]) : 0.0;
    auto set = makeSyntheticRing();
    const auto index = set.nearestMeasurementIndex({az, el, 1.0});
    const auto& m = set.measurements()[index];
    const auto out = renderNearestOffline({1.0f}, set, {az, el, 1.0});

    std::cout << "SpaceTrace 0.1.0 probe\n";
    std::cout << "Default built-in HRTF: " << defaultBuiltInDataset().displayName << "\n";
    std::cout << "Canonical convention: 0 front, +90 left, -90/270 right\n";
    std::cout << "Requested: az " << az << ", el " << el << "\n";
    std::cout << "Selected synthetic point: az " << m.position.azimuthDegrees << "\n";
    std::cout << "Impulse output L/R: " << out.left.front() << " / " << out.right.front() << "\n";
    return 0;
}
