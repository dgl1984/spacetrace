#include "AccessibleParameterSlider.h"

#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace spacetrace::plugin {

AccessibleParameterSlider::AccessibleParameterSlider()
    : juce::Slider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox) {
    setAccessible(true);
    setWantsKeyboardFocus(true);
    setSliderSnapsToMousePosition(false);
}

bool AccessibleParameterSlider::keyPressed(const juce::KeyPress& key) {
    const auto code = key.getKeyCode();
    if (code == juce::KeyPress::returnKey || code == juce::KeyPress::F2Key) {
        if (onRequestExactEntry) onRequestExactEntry(*this);
        return true;
    }

    if (!key.getModifiers().isAnyModifierKeyDown()) {
        if (code == juce::KeyPress::homeKey) {
            setValue(getMinimum(), juce::sendNotificationSync);
            return true;
        }

        if (code == juce::KeyPress::endKey) {
            setValue(getMaximum(), juce::sendNotificationSync);
            return true;
        }

        if (keyboardPageStep_ > 0.0) {
            if (code == juce::KeyPress::pageUpKey) {
                setValue(getValue() + keyboardPageStep_, juce::sendNotificationSync);
                return true;
            }

            if (code == juce::KeyPress::pageDownKey) {
                setValue(getValue() - keyboardPageStep_, juce::sendNotificationSync);
                return true;
            }
        }
    }

    return juce::Slider::keyPressed(key);
}

juce::String AccessibleParameterSlider::formatExactValue() const {
    return juce::String(getValue(), getNumDecimalPlacesToDisplay());
}

bool AccessibleParameterSlider::parseExactValue(const juce::String& text, double& value) const {
    const auto trimmed = text.trim();
    const auto utf8 = trimmed.toRawUTF8();
    if (utf8 == nullptr || *utf8 == '\0') return false;

    errno = 0;
    char* end = nullptr;
    const double parsed = std::strtod(utf8, &end);
    if (end == utf8 || errno == ERANGE || !std::isfinite(parsed)) return false;
    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end)) != 0) ++end;
    if (*end != '\0') return false;
    if (parsed < getMinimum() || parsed > getMaximum()) return false;

    value = parsed;
    return true;
}

void AccessibleParameterSlider::setFromExactEntry(double value) {
    setValue(value, juce::sendNotificationSync);
}

} // namespace spacetrace::plugin
