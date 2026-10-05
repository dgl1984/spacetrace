#pragma once

#include <array>
#include <cstddef>

namespace spacetrace {

// A deliberately conservative free-field distance model.
// 1 metre is neutral. Closer distances use softened gain rather than full 1/r
// boost; farther distances retain 1/r falloff and add a subtle HF shelf.
class DistanceModel {
public:
    static float levelGain(float metres) noexcept;
    static float airHighGain(float metres) noexcept;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;
    float processSample(std::size_t channel, float input, float highGain) noexcept;

private:
    float lowpassAlpha_ = 1.0f;
    std::array<float, 2> lowState_ {0.0f, 0.0f};
    std::array<bool, 2> initialized_ {false, false};
};

} // namespace spacetrace
