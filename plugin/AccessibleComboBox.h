#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace spacetrace::plugin {

// JUCE's ComboBox keyboard handling covers arrows and Return but not Home/End.
// SpaceTrace adds the conventional boundary keys while leaving all other
// pointer, popup, accessibility and selection behaviour JUCE-owned.
class AccessibleComboBox final : public juce::ComboBox {
public:
    AccessibleComboBox() = default;
    ~AccessibleComboBox() override = default;

    bool keyPressed(const juce::KeyPress& key) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AccessibleComboBox)
};

} // namespace spacetrace::plugin
