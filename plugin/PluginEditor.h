#pragma once

#include "AccessibleComboBox.h"
#include "AccessibleParameterSlider.h"
#include "PluginProcessor.h"
#include "SpatialDisplay.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>

namespace spacetrace::plugin {

class SpaceTraceAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer {
public:
    explicit SpaceTraceAudioProcessorEditor(SpaceTraceAudioProcessor&);
    ~SpaceTraceAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void configureSlider(AccessibleParameterSlider&, const juce::String& name,
                         const juce::String& description, int decimalPlaces);
    void refreshStructuralControls();
    void refreshSpatialDisplay();
    void chooseCustomIR();

    void beginExactEntry(AccessibleParameterSlider& target);
    void queueExactFinish(bool apply, bool returnFocus);
    void finishExactEntry(bool apply, bool returnFocus);
    void clearExactEntryError();
    void showExactEntryError(const AccessibleParameterSlider& target);

    SpaceTraceAudioProcessor& processor_;

    juce::Label title_;
    juce::Label intro_;
    juce::Label positionHeading_;
    SpatialDisplay spatialDisplay_;
    AccessibleComboBox renderer_;
    AccessibleComboBox inputMode_;
    AccessibleComboBox dataset_;
    AccessibleComboBox compensation_;
    juce::TextButton loadCustomIR_ {"Load Custom Correction IR..."};

    juce::Label azimuthLabel_;
    juce::Label elevationLabel_;
    juce::Label distanceLabel_;
    juce::Label widthOffsetLabel_;
    juce::Label outputLabel_;

    AccessibleParameterSlider azimuth_;
    AccessibleParameterSlider elevation_;
    AccessibleParameterSlider distance_;
    AccessibleParameterSlider widthOffset_;
    AccessibleParameterSlider outputDb_;
    juce::ToggleButton airLoss_ {"Air Loss"};
    juce::ToggleButton bypass_ {"Bypass"};
    juce::Label status_;
    juce::Label exactEntryError_;

    std::unique_ptr<juce::TextEditor> exactEditor_;
    AccessibleParameterSlider* exactTarget_ = nullptr;
    bool exactFinishQueued_ = false;
    juce::String lastStatusText_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> azimuthAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> elevationAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distanceAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> widthOffsetAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> inputModeAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> airLossAttachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment_;
    std::unique_ptr<juce::FileChooser> chooser_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpaceTraceAudioProcessorEditor)
};

} // namespace spacetrace::plugin
