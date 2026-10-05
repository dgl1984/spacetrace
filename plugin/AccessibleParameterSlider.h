#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace spacetrace::plugin {

// A normal JUCE Slider with SpaceTrace's missing conventional keyboard
// semantics: Home/End, optional Page Up/Page Down coarse movement, and
// Enter/F2 exact-entry requests to the owning editor. Pointer/drag/wheel/
// double-click behavior remains JUCE-owned, and host synchronization remains
// owned by the normal JUCE parameter attachment.
class AccessibleParameterSlider final : public juce::Slider {
public:
    AccessibleParameterSlider();
    ~AccessibleParameterSlider() override = default;

    std::function<void(AccessibleParameterSlider&)> onRequestExactEntry;

    bool keyPressed(const juce::KeyPress& key) override;

    // Optional coarse keyboard step used by Page Up / Page Down. A value of
    // zero leaves those keys unhandled so the owning editor can opt in only
    // where a coarse adjustment is useful.
    void setKeyboardPageStep(double step) noexcept { keyboardPageStep_ = step > 0.0 ? step : 0.0; }
    [[nodiscard]] double getKeyboardPageStep() const noexcept { return keyboardPageStep_; }

    [[nodiscard]] juce::String formatExactValue() const;
    [[nodiscard]] bool parseExactValue(const juce::String& text, double& value) const;
    void setFromExactEntry(double value);

private:
    double keyboardPageStep_ = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AccessibleParameterSlider)
};

} // namespace spacetrace::plugin
