#include "spacetrace/Compensation.h"

#include <cmath>

namespace spacetrace {

bool CompensationProfile::valid(std::string* reason) const {
    auto fail = [&](const char* text) {
        if (reason) *reason = text;
        return false;
    };
    if (!(sampleRate > 0.0) || !std::isfinite(sampleRate))
        return fail("compensation sample rate must be finite and greater than zero");
    if (impulse.empty())
        return fail("compensation impulse is empty");
    for (const auto v : impulse)
        if (!std::isfinite(v))
            return fail("compensation impulse contains a non-finite value");
    if (reason) reason->clear();
    return true;
}

} // namespace spacetrace
