#include "AccessibleComboBox.h"

namespace spacetrace::plugin {

bool AccessibleComboBox::keyPressed(const juce::KeyPress& key) {
    if (!key.getModifiers().isAnyModifierKeyDown()) {
        if (key == juce::KeyPress::homeKey) {
            if (getNumItems() > 0)
                setSelectedItemIndex(0, juce::sendNotificationSync);
            return true;
        }

        if (key == juce::KeyPress::endKey) {
            if (getNumItems() > 0)
                setSelectedItemIndex(getNumItems() - 1, juce::sendNotificationSync);
            return true;
        }
    }

    return juce::ComboBox::keyPressed(key);
}

} // namespace spacetrace::plugin
