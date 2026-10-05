#pragma once

#include <string>
#include <vector>

namespace spacetrace {

enum class CompensationMode {
    Raw = 0,
    DatasetCorrected = 1,
    CustomIR = 2,
};

struct CompensationProfile {
    std::string name;
    std::string version;
    double sampleRate = 0.0;
    std::vector<float> impulse;

    [[nodiscard]] bool valid(std::string* reason = nullptr) const;
};

} // namespace spacetrace
