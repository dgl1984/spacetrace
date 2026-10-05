#pragma once

#include "PluginProcessor.h"
#include "StereoPairGeometry.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace spacetrace::plugin {

// Read-only visualisation of the existing spatial parameters.
// Universal-access contract: this component is not a control, never takes
// keyboard focus, is excluded from the accessibility hierarchy, and never
// writes parameter or processor state.
class SpatialDisplay final : public juce::Component {
public:
    SpatialDisplay();

    void setViewState(float azimuthDegrees,
                      float elevationDegrees,
                      float distanceMetres,
                      float widthOffsetDegrees,
                      RendererMode rendererMode,
                      InputMode inputMode);

    void paint(juce::Graphics&) override;

private:
    static float distanceToRadius01(float distanceMetres) noexcept;

    float azimuthDegrees_ = 0.0f;
    float elevationDegrees_ = 0.0f;
    float distanceMetres_ = 1.0f;
    float widthOffsetDegrees_ = 0.0f;
    RendererMode rendererMode_ = RendererMode::ModernHRTF;
    InputMode inputMode_ = InputMode::MonoPointSource;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpatialDisplay)
};

} // namespace spacetrace::plugin
