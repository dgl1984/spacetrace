#include "PluginEditor.h"

namespace spacetrace::plugin {
namespace {
void configureDecorativeLabel(juce::Label& label, const juce::String& text, float fontHeight) {
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(fontHeight)));
    label.setJustificationType(juce::Justification::centredLeft);
    label.setAccessible(false);
    label.setInterceptsMouseClicks(false, false);
}
}

SpaceTraceAudioProcessorEditor::SpaceTraceAudioProcessorEditor(SpaceTraceAudioProcessor& p)
    : AudioProcessorEditor(&p), processor_(p) {
    setAccessible(true);
    setTitle("SpaceTrace");
    setDescription("Accessible binaural HRTF spatializer. Use Tab and Shift Tab to move through controls. Enter or F2 opens exact value entry on spatial sliders.");

    configureDecorativeLabel(title_, "SpaceTrace", 28.0f);
    configureDecorativeLabel(intro_,
        "Binaural HRTF spatializer. 0 degrees front, 90 left, 180 rear, 270 right.", 15.0f);
    addAndMakeVisible(title_);
    addAndMakeVisible(intro_);

    configureDecorativeLabel(positionHeading_, "Position", 18.0f);
    addAndMakeVisible(positionHeading_);
    addAndMakeVisible(spatialDisplay_);

    renderer_.addItem("Modern HRTF", 1);
    renderer_.addItem(u8"Papa Pan \u2014 historical", 2);
    renderer_.setTitle("Renderer");
    renderer_.setDescription("Selects the spatial rendering method. Modern HRTF uses the selected head with continuous interpolation. Papa Pan uses the historical 24-direction IRCAM horizontal behaviour. This choice is stored with the project and is not host-automatable.");
    renderer_.setAccessible(true);
    renderer_.setWantsKeyboardFocus(true);
    renderer_.onChange = [this] {
        processor_.setRendererMode(renderer_.getSelectedId() == 2 ? RendererMode::PapaPan : RendererMode::ModernHRTF);
        refreshSpatialDisplay();
    };
    addAndMakeVisible(renderer_);

    inputMode_.addItem("Mono Point Source", 1);
    inputMode_.addItem("Stereo Pair", 2);
    inputMode_.setTitle("Input Mode");
    inputMode_.setDescription("Mono Point Source folds stereo input to one source at the selected position. Stereo Pair is available in Modern HRTF and spatializes the left and right input channels as one linked binaural object. Width Offset moves that pair symmetrically around the calibrated 90/270 reference geometry.");
    inputMode_.setAccessible(true);
    inputMode_.setWantsKeyboardFocus(true);
    inputMode_.onChange = [this] { refreshStructuralControls(); refreshSpatialDisplay(); };
    addAndMakeVisible(inputMode_);

    dataset_.addItem(u8"IRCAM LISTEN 1050 \u2014 default", 1);
    dataset_.addItem("MIT KEMAR Normal Pinna", 2);
    dataset_.addItem("SADIE II D1 / Neumann KU100", 3);
    dataset_.addItem(u8"TH K\u00f6ln KU100 FULL2DEG", 4);
    dataset_.addItem("FABIAN HATO 0", 5);
    dataset_.setTitle("Head Model");
    dataset_.setDescription("Selects the external SpaceTrace head used by Modern HRTF. IRCAM 1050 is the reference head; FABIAN HATO 0 is retained for its especially strong vertical localization and includes its approved fixed tonal correction. This choice is stored with the project and is not host-automatable.");
    dataset_.setAccessible(true);
    dataset_.setWantsKeyboardFocus(true);
    dataset_.onChange = [this] {
        switch (dataset_.getSelectedId()) {
            case 2: processor_.selectBuiltInDataset(BuiltInDatasetId::MitKemarNormalPinna); break;
            case 3: processor_.selectBuiltInDataset(BuiltInDatasetId::Sadie2D1Ku100); break;
            case 4: processor_.selectBuiltInDataset(BuiltInDatasetId::ThkKu100Full2Deg); break;
            case 5: processor_.selectBuiltInDataset(BuiltInDatasetId::FabianHato0); break;
            default: processor_.selectBuiltInDataset(BuiltInDatasetId::IrcamListen1050); break;
        }
    };
    addAndMakeVisible(dataset_);

    compensation_.addItem("Raw", 1);
    compensation_.addItem("Dataset Corrected", 2);
    compensation_.addItem("Custom Correction IR", 3);
    compensation_.setTitle("Tonal Compensation");
    compensation_.setDescription("Raw leaves the HRTF unchanged. Dataset Corrected applies the fixed global tonal restoration stored with the selected dataset. Custom Correction IR replaces that restoration with a user-loaded mono correction without altering left-right cues. It is not a room or reverb slot.");
    compensation_.setAccessible(true);
    compensation_.setWantsKeyboardFocus(true);
    compensation_.onChange = [this] {
        const auto id = compensation_.getSelectedId();
        processor_.setCompensationMode(id == 1 ? CompensationMode::Raw
                                    : id == 3 ? CompensationMode::CustomIR
                                              : CompensationMode::DatasetCorrected);
    };
    addAndMakeVisible(compensation_);

    loadCustomIR_.setAccessible(true);
    loadCustomIR_.setWantsKeyboardFocus(true);
    loadCustomIR_.setTitle("Load Custom Correction IR");
    loadCustomIR_.setDescription("Loads a mono or identical-stereo WAV tonal-correction impulse response. This replaces Dataset Corrected; it does not add room or reverb processing.");
    loadCustomIR_.onClick = [this] { chooseCustomIR(); };
    addAndMakeVisible(loadCustomIR_);

    configureSlider(azimuth_, "Azimuth", "Horizontal direction. 0 degrees is front, 90 left, 180 rear, and 270 right. Arrow keys move 1 degree; Page Up and Page Down move 10 degrees; Home and End jump to 0 and 360 degrees. Press Enter or F2 for exact entry.", 0);
    azimuth_.setKeyboardPageStep(10.0);
    configureSlider(elevation_, "Elevation", "Vertical direction from minus 90 below to plus 90 above. Arrow keys move 1 degree; Page Up and Page Down move 10 degrees; Home and End jump to minus 90 and plus 90 degrees. Press Enter or F2 for exact entry.", 0);
    elevation_.setKeyboardPageStep(10.0);
    configureSlider(distance_, "Distance", "Source distance in metres. One metre is neutral. Farther distances use free-field level falloff plus subtle high-frequency air loss when Air Loss is enabled; closer distances use a softened gain curve because the included HRTFs are not near-field measurements. Press Enter or F2 for exact entry.", 2);
    configureSlider(widthOffset_, "Width Offset", "Stereo Pair only. Moves the linked left and right sources symmetrically around the canonical 90 and 270 degree positions. Minus values bow both sources toward the front; plus values bow them toward the rear. Range minus 45 to plus 45 degrees; zero is the calibrated 90/270 reference geometry. Press Enter or F2 for exact entry.", 0);
    configureSlider(outputDb_, "Output Level", "Final output trim in decibels. Press Enter or F2 for exact entry.", 1);

    airLoss_.setButtonText("Air Loss");
    airLoss_.setAccessible(true);
    airLoss_.setWantsKeyboardFocus(true);
    airLoss_.setTitle("Air Loss");
    airLoss_.setDescription("Enables SpaceTrace's subtle synthetic high-frequency air absorption beyond one metre. Turning this off leaves Distance level and proximity behavior active. At exactly one metre, Air Loss on and off are spectrally identical.");
    addAndMakeVisible(airLoss_);

    // The spatial display follows the real controls themselves. This avoids a
    // high-rate timer dereferencing the processor during editor/track teardown,
    // while still following mouse, keyboard, exact-entry, automation, and state
    // updates delivered through the normal JUCE parameter attachments.
    azimuth_.onValueChange = [this] { refreshSpatialDisplay(); };
    elevation_.onValueChange = [this] { refreshSpatialDisplay(); };
    distance_.onValueChange = [this] { refreshSpatialDisplay(); };
    widthOffset_.onValueChange = [this] { refreshSpatialDisplay(); };

    addAndMakeVisible(azimuth_);
    addAndMakeVisible(elevation_);
    addAndMakeVisible(distance_);
    addAndMakeVisible(widthOffset_);
    addAndMakeVisible(outputDb_);

    configureDecorativeLabel(azimuthLabel_, "Azimuth", 15.0f);
    configureDecorativeLabel(elevationLabel_, "Elevation", 15.0f);
    configureDecorativeLabel(distanceLabel_, "Distance", 15.0f);
    configureDecorativeLabel(widthOffsetLabel_, "Width Offset (Stereo Pair only)", 15.0f);
    configureDecorativeLabel(outputLabel_, "Output Level", 15.0f);
    addAndMakeVisible(azimuthLabel_);
    addAndMakeVisible(elevationLabel_);
    addAndMakeVisible(distanceLabel_);
    addAndMakeVisible(widthOffsetLabel_);
    addAndMakeVisible(outputLabel_);

    bypass_.setAccessible(true);
    bypass_.setWantsKeyboardFocus(true);
    bypass_.setTitle("Bypass");
    bypass_.setDescription("Passes the input without HRTF processing. Mono input is duplicated to stereo.");
    addAndMakeVisible(bypass_);

    status_.setText("Ready", juce::dontSendNotification);
    status_.setTitle("Status");
    status_.setDescription("SpaceTrace processing status.");
    status_.setJustificationType(juce::Justification::centredLeft);
    status_.setAccessible(true);
    status_.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(status_);

    exactEntryError_.setTitle("Exact value entry error");
    exactEntryError_.setDescription("Exact value entry error message.");
    exactEntryError_.setJustificationType(juce::Justification::centredLeft);
    exactEntryError_.setAccessible(true);
    exactEntryError_.setInterceptsMouseClicks(false, false);
    exactEntryError_.setVisible(false);
    addAndMakeVisible(exactEntryError_);

    auto& apvts = processor_.parameters();
    azimuthAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, SpaceTraceAudioProcessor::paramAzimuth, azimuth_);
    elevationAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, SpaceTraceAudioProcessor::paramElevation, elevation_);
    distanceAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, SpaceTraceAudioProcessor::paramDistance, distance_);
    widthOffsetAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, SpaceTraceAudioProcessor::paramWidthOffset, widthOffset_);
    outputAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, SpaceTraceAudioProcessor::paramOutputDb, outputDb_);
    inputModeAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, SpaceTraceAudioProcessor::paramInputMode, inputMode_);
    airLossAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, SpaceTraceAudioProcessor::paramAirLoss, airLoss_);
    bypassAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, SpaceTraceAudioProcessor::paramBypass, bypass_);

    int focusOrder = 1;
    dataset_.setExplicitFocusOrder(focusOrder++);
    renderer_.setExplicitFocusOrder(focusOrder++);
    inputMode_.setExplicitFocusOrder(focusOrder++);
    compensation_.setExplicitFocusOrder(focusOrder++);
    loadCustomIR_.setExplicitFocusOrder(focusOrder++);
    azimuth_.setExplicitFocusOrder(focusOrder++);
    elevation_.setExplicitFocusOrder(focusOrder++);
    distance_.setExplicitFocusOrder(focusOrder++);
    widthOffset_.setExplicitFocusOrder(focusOrder++);
    airLoss_.setExplicitFocusOrder(focusOrder++);
    outputDb_.setExplicitFocusOrder(focusOrder++);
    bypass_.setExplicitFocusOrder(focusOrder++);

    refreshStructuralControls();
    refreshSpatialDisplay();

    setSize(900, 680);
    setResizeLimits(820, 648, 1280, 920);
    setResizable(true, true);
    if (resizableCorner != nullptr)
        resizableCorner->setAccessible(false);

    startTimerHz(4);
}

SpaceTraceAudioProcessorEditor::~SpaceTraceAudioProcessorEditor() {
    stopTimer();
    chooser_.reset();
    exactEditor_.reset();
}

void SpaceTraceAudioProcessorEditor::configureSlider(AccessibleParameterSlider& slider,
                                                       const juce::String& name,
                                                       const juce::String& description,
                                                       int decimalPlaces) {
    slider.setName(name);
    slider.setTitle(name);
    slider.setDescription(description);
    slider.setNumDecimalPlacesToDisplay(decimalPlaces);
    slider.setWantsKeyboardFocus(true);
    slider.onRequestExactEntry = [this](AccessibleParameterSlider& target) { beginExactEntry(target); };
}

void SpaceTraceAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
}

void SpaceTraceAudioProcessorEditor::resized() {
    auto area = getLocalBounds().reduced(18);
    title_.setBounds(area.removeFromTop(38));
    intro_.setBounds(area.removeFromTop(34));
    area.removeFromTop(6);

    auto rendererRow = area.removeFromTop(38);
    const int headWidth = juce::jmin(300, rendererRow.getWidth() * 40 / 100);
    dataset_.setBounds(rendererRow.removeFromLeft(headWidth));
    rendererRow.removeFromLeft(8);
    const int rendererWidth = juce::jmin(210, rendererRow.getWidth() * 48 / 100);
    renderer_.setBounds(rendererRow.removeFromLeft(rendererWidth));
    rendererRow.removeFromLeft(8);
    inputMode_.setBounds(rendererRow);
    area.removeFromTop(8);

    auto correctionRow = area.removeFromTop(38);
    const int compensationWidth = juce::jmin(250, correctionRow.getWidth() * 40 / 100);
    compensation_.setBounds(correctionRow.removeFromLeft(compensationWidth));
    correctionRow.removeFromLeft(10);
    loadCustomIR_.setBounds(correctionRow);
    area.removeFromTop(10);

    positionHeading_.setBounds(area.removeFromTop(26));
    area.removeFromTop(4);

    auto positionArea = area.removeFromTop(286);
    const int displayWidth = juce::jmin(360, positionArea.getWidth() * 44 / 100);
    spatialDisplay_.setBounds(positionArea.removeFromLeft(displayWidth));
    positionArea.removeFromLeft(14);

    auto makePositionRow = [this, &positionArea](juce::Label& label, AccessibleParameterSlider& control,
                                                 juce::ToggleButton* rowToggle = nullptr) {
        auto row = positionArea.removeFromTop(70);
        auto labelRow = row.removeFromTop(22);
        if (rowToggle != nullptr) {
            const int toggleWidth = juce::jmin(120, labelRow.getWidth() / 3);
            rowToggle->setBounds(labelRow.removeFromRight(toggleWidth));
        }
        label.setBounds(labelRow);
        const auto controlBounds = row.reduced(2, 7);
        control.setBounds(controlBounds);
        if (exactTarget_ == &control && exactEditor_ != nullptr)
            exactEditor_->setBounds(controlBounds);
    };
    makePositionRow(azimuthLabel_, azimuth_);
    makePositionRow(elevationLabel_, elevation_);
    makePositionRow(distanceLabel_, distance_, &airLoss_);
    makePositionRow(widthOffsetLabel_, widthOffset_);

    area.removeFromTop(8);
    auto outputRow = area.removeFromTop(48);
    outputLabel_.setBounds(outputRow.removeFromLeft(130));
    const auto outputBounds = outputRow.reduced(4, 7);
    outputDb_.setBounds(outputBounds);
    if (exactTarget_ == &outputDb_ && exactEditor_ != nullptr)
        exactEditor_->setBounds(outputBounds);

    if (exactEntryError_.isVisible())
        exactEntryError_.setBounds(area.removeFromTop(28));

    auto bottom = area.removeFromTop(40);
    bypass_.setBounds(bottom.removeFromLeft(120));
    status_.setBounds(bottom);
}

void SpaceTraceAudioProcessorEditor::refreshStructuralControls() {
    const bool papaMode = processor_.rendererMode() == RendererMode::PapaPan;
    renderer_.setSelectedId(papaMode ? 2 : 1, juce::dontSendNotification);
    inputMode_.setEnabled(!papaMode);
    const bool stereoPairActive = !papaMode && inputMode_.getSelectedId() == 2;
    widthOffset_.setEnabled(stereoPairActive);
    dataset_.setEnabled(!papaMode);
    elevation_.setEnabled(!papaMode);
    inputMode_.setDescription(papaMode
        ? "Input Mode is inactive in Papa Pan. Papa Pan remains a mono point-source historical renderer; the selected Modern HRTF input mode is retained for when you switch back."
        : "Mono Point Source folds stereo input to one source at the selected position. Stereo Pair spatializes input Left and Right as one linked binaural object around the selected center azimuth. Width Offset can move the pair symmetrically away from the calibrated 90/270 reference geometry, with shared Head, Elevation, Distance, Tone, Air Loss, and Output settings.");
    dataset_.setDescription(papaMode
        ? "Head selection is inactive in Papa Pan because the historical renderer uses the IRCAM LISTEN 1050 horizontal ring. Your Modern HRTF head selection is retained for when you switch back."
        : "Selects the external head for Modern HRTF rendering. IRCAM 1050 is the reference head. FABIAN HATO 0 produced the strongest vertical localization in current listening, including directly front and rear, while rear horizontal placement can be softer. This choice is stored with the project and is not host-automatable.");
    elevation_.setDescription(papaMode
        ? "Elevation is inactive in Papa Pan because the historical renderer is horizontal-only. Switch to Modern HRTF for vertical positioning."
        : "Vertical direction from minus 90 below to plus 90 above. Arrow keys move 1 degree; Page Up and Page Down move 10 degrees; Home and End jump to minus 90 and plus 90 degrees. Press Enter or F2 for exact entry.");

    const auto selectedDataset = processor_.selectedBuiltInDataset();
    dataset_.setSelectedId(selectedDataset == BuiltInDatasetId::MitKemarNormalPinna ? 2
                           : selectedDataset == BuiltInDatasetId::Sadie2D1Ku100 ? 3
                           : selectedDataset == BuiltInDatasetId::ThkKu100Full2Deg ? 4
                           : selectedDataset == BuiltInDatasetId::FabianHato0 ? 5 : 1,
                           juce::dontSendNotification);
    const auto mode = processor_.compensationMode();
    compensation_.setSelectedId(mode == CompensationMode::Raw ? 1
                                : mode == CompensationMode::CustomIR ? 3 : 2,
                                juce::dontSendNotification);

    const auto nextStatus = processor_.statusText();
    if (nextStatus != lastStatusText_) {
        status_.setText(nextStatus, juce::dontSendNotification);
        lastStatusText_ = nextStatus;
    }
}

void SpaceTraceAudioProcessorEditor::refreshSpatialDisplay() {
    spatialDisplay_.setViewState(static_cast<float>(azimuth_.getValue()),
                                 static_cast<float>(elevation_.getValue()),
                                 static_cast<float>(distance_.getValue()),
                                 static_cast<float>(widthOffset_.getValue()),
                                 renderer_.getSelectedId() == 2 ? RendererMode::PapaPan
                                                                : RendererMode::ModernHRTF,
                                 inputMode_.getSelectedId() == 2 ? InputMode::StereoPair
                                                                 : InputMode::MonoPointSource);
}

void SpaceTraceAudioProcessorEditor::timerCallback() {
    refreshStructuralControls();
    refreshSpatialDisplay();
}

void SpaceTraceAudioProcessorEditor::chooseCustomIR() {
    chooser_ = std::make_unique<juce::FileChooser>(
        "Load SpaceTrace custom correction IR", juce::File(), "*.wav", true);
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<SpaceTraceAudioProcessorEditor>(this)](const juce::FileChooser& chooser) {
            if (safe == nullptr) return;
            const auto file = chooser.getResult();
            if (!file.existsAsFile()) return;
            juce::String error;
            if (!safe->processor_.loadCustomCompensationIR(file, error)) {
                const auto message = "Custom Correction IR error: " + error;
                safe->status_.setText(message, juce::dontSendNotification);
                safe->lastStatusText_ = message;
            } else {
                safe->refreshStructuralControls();
            }
        });
}

void SpaceTraceAudioProcessorEditor::beginExactEntry(AccessibleParameterSlider& target) {
    if (exactEditor_ != nullptr) return;

    clearExactEntryError();
    exactTarget_ = &target;
    const auto exactName = target.getName() + " exact value";

    exactEditor_ = std::make_unique<juce::TextEditor>();
    exactEditor_->setName(exactName);
    exactEditor_->setTitle(exactName);
    exactEditor_->setDescription("Type an exact numeric value for " + target.getName() + ". Press Enter to apply or Escape to cancel.");
    exactEditor_->setAccessible(true);
    exactEditor_->setMultiLine(false);
    exactEditor_->setReturnKeyStartsNewLine(false);
    exactEditor_->setEscapeAndReturnKeysConsumed(false);
    exactEditor_->setSelectAllWhenFocused(true);
    exactEditor_->setJustification(juce::Justification::centred);
    exactEditor_->setExplicitFocusOrder(target.getExplicitFocusOrder());
    exactEditor_->setText(target.formatExactValue(), false);

    const auto safe = juce::Component::SafePointer<SpaceTraceAudioProcessorEditor>(this);
    exactEditor_->onReturnKey = [safe] {
        if (safe != nullptr) safe->queueExactFinish(true, true);
    };
    exactEditor_->onEscapeKey = [safe] {
        if (safe != nullptr) safe->queueExactFinish(false, true);
    };
    exactEditor_->onFocusLost = [safe] {
        if (safe != nullptr) safe->queueExactFinish(false, false);
    };

    addAndMakeVisible(*exactEditor_);
    target.setVisible(false);
    resized();
    exactEditor_->selectAll();
    exactEditor_->grabKeyboardFocus();
}

void SpaceTraceAudioProcessorEditor::queueExactFinish(bool apply, bool returnFocus) {
    if (exactEditor_ == nullptr || exactTarget_ == nullptr || exactFinishQueued_) return;
    exactFinishQueued_ = true;
    const auto safe = juce::Component::SafePointer<SpaceTraceAudioProcessorEditor>(this);
    juce::MessageManager::callAsync([safe, apply, returnFocus] {
        if (safe != nullptr) safe->finishExactEntry(apply, returnFocus);
    });
}

void SpaceTraceAudioProcessorEditor::finishExactEntry(bool apply, bool returnFocus) {
    if (exactEditor_ == nullptr || exactTarget_ == nullptr) return;

    exactFinishQueued_ = false;
    auto* target = exactTarget_;
    const auto enteredText = exactEditor_->getText();
    double parsed = target->getValue();
    const bool applied = apply && target->parseExactValue(enteredText, parsed);

    exactEditor_->setVisible(false);
    target->setVisible(true);
    if (returnFocus) target->grabKeyboardFocus();

    removeChildComponent(exactEditor_.get());
    exactEditor_.reset();
    exactTarget_ = nullptr;

    if (applied) {
        clearExactEntryError();
        target->setFromExactEntry(parsed);
    } else if (apply) {
        showExactEntryError(*target);
    }
    resized();
}

void SpaceTraceAudioProcessorEditor::clearExactEntryError() {
    exactEntryError_.setVisible(false);
    exactEntryError_.setText({}, juce::dontSendNotification);
}

void SpaceTraceAudioProcessorEditor::showExactEntryError(const AccessibleParameterSlider& target) {
    const auto minText = juce::String(target.getMinimum(), target.getNumDecimalPlacesToDisplay());
    const auto maxText = juce::String(target.getMaximum(), target.getNumDecimalPlacesToDisplay());
    const auto message = "Invalid " + target.getName() + ". Enter a numeric value from " + minText +
        " to " + maxText + ". The previous value was kept.";
    exactEntryError_.setText(message, juce::dontSendNotification);
    exactEntryError_.setVisible(true);
}

} // namespace spacetrace::plugin
