#include "SpatialDisplay.h"

#include <algorithm>
#include <cmath>

namespace spacetrace::plugin {

SpatialDisplay::SpatialDisplay() {
    setAccessible(false);
    setWantsKeyboardFocus(false);
    setInterceptsMouseClicks(false, false);
}

void SpatialDisplay::setViewState(float azimuthDegrees,
                                  float elevationDegrees,
                                  float distanceMetres,
                                  float widthOffsetDegrees,
                                  RendererMode rendererMode,
                                  InputMode inputMode) {
    const auto azimuth = std::fmod(std::max(0.0f, azimuthDegrees), 360.0f);
    const auto elevation = juce::jlimit(-90.0f, 90.0f, elevationDegrees);
    const auto distance = juce::jlimit(0.25f, 20.0f, distanceMetres);
    const auto widthOffset = juce::jlimit(-45.0f, 45.0f, widthOffsetDegrees);

    if (azimuth == azimuthDegrees_ && elevation == elevationDegrees_ &&
        distance == distanceMetres_ && widthOffset == widthOffsetDegrees_ && rendererMode == rendererMode_ && inputMode == inputMode_)
        return;

    azimuthDegrees_ = azimuth;
    elevationDegrees_ = elevation;
    distanceMetres_ = distance;
    widthOffsetDegrees_ = widthOffset;
    rendererMode_ = rendererMode;
    inputMode_ = inputMode;
    repaint();
}

float SpatialDisplay::distanceToRadius01(float distanceMetres) noexcept {
    // Mirror the perceptual intent of the Distance parameter without becoming
    // another parameter mapping or input path. The logarithmic display keeps
    // near-field changes visible across the full 0.25 m..20 m range.
    constexpr float minDistance = 0.25f;
    constexpr float maxDistance = 20.0f;
    const auto d = juce::jlimit(minDistance, maxDistance, distanceMetres);
    return std::log(d / minDistance) / std::log(maxDistance / minDistance);
}

void SpatialDisplay::paint(juce::Graphics& g) {
    auto bounds = getLocalBounds().toFloat().reduced(8.0f);
    if (bounds.getWidth() < 80.0f || bounds.getHeight() < 80.0f)
        return;

    const auto textColour = findColour(juce::Label::textColourId);
    const auto outlineColour = textColour.withAlpha(0.45f);
    const auto guideColour = textColour.withAlpha(0.20f);
    const auto sourceColour = findColour(juce::Slider::thumbColourId);

    const float elevationWidth = juce::jmin(52.0f, bounds.getWidth() * 0.16f);
    auto elevationArea = bounds.removeFromRight(elevationWidth);
    bounds.removeFromRight(8.0f);

    const float labelBand = 22.0f;
    auto fieldArea = bounds.reduced(labelBand, labelBand);
    const float diameter = juce::jmin(fieldArea.getWidth(), fieldArea.getHeight());
    auto circle = juce::Rectangle<float>(diameter, diameter).withCentre(fieldArea.getCentre());
    const auto centre = circle.getCentre();
    const float radius = circle.getWidth() * 0.5f;

    g.setColour(guideColour);
    g.drawLine(centre.x, circle.getY(), centre.x, circle.getBottom(), 1.0f);
    g.drawLine(circle.getX(), centre.y, circle.getRight(), centre.y, 1.0f);

    g.setColour(outlineColour);
    g.drawEllipse(circle, 1.5f);
    g.fillEllipse(centre.x - 4.0f, centre.y - 4.0f, 8.0f, 8.0f);

    g.setColour(textColour);
    g.setFont(13.0f);
    g.drawText("Front", juce::Rectangle<float>(circle.getX(), circle.getY() - labelBand, circle.getWidth(), labelBand).toNearestInt(),
               juce::Justification::centred, false);
    g.drawText("Rear", juce::Rectangle<float>(circle.getX(), circle.getBottom(), circle.getWidth(), labelBand).toNearestInt(),
               juce::Justification::centred, false);
    g.drawText("Left", juce::Rectangle<float>(circle.getX() - labelBand * 2.0f, centre.y - labelBand * 0.5f,
                                              labelBand * 2.0f, labelBand).toNearestInt(),
               juce::Justification::centredRight, false);
    g.drawText("Right", juce::Rectangle<float>(circle.getRight(), centre.y - labelBand * 0.5f,
                                               labelBand * 2.0f, labelBand).toNearestInt(),
               juce::Justification::centredLeft, false);

    const float radial = radius * (0.10f + 0.82f * distanceToRadius01(distanceMetres_));
    const auto pointForAzimuth = [centre, radial](float azimuth) {
        const float radians = juce::degreesToRadians(azimuth);
        return juce::Point<float> {
            centre.x - std::sin(radians) * radial,
            centre.y - std::cos(radians) * radial
        };
    };

    // SpaceTrace convention: 0 front, 90 left, 180 rear, 270 right.
    if (rendererMode_ == RendererMode::ModernHRTF && inputMode_ == InputMode::StereoPair) {
        const auto geometry = stereoPairGeometry(static_cast<double>(azimuthDegrees_),
                                                 static_cast<double>(widthOffsetDegrees_));
        const auto leftSource = pointForAzimuth(static_cast<float>(geometry.leftAzimuthDegrees));
        const auto rightSource = pointForAzimuth(static_cast<float>(geometry.rightAzimuthDegrees));
        for (const auto source : {leftSource, rightSource}) {
            g.setColour(sourceColour.withAlpha(0.35f));
            g.drawLine(centre.x, centre.y, source.x, source.y, 1.5f);
            g.setColour(sourceColour);
            g.fillEllipse(source.x - 7.0f, source.y - 7.0f, 14.0f, 14.0f);
        }
        g.setColour(textColour);
        g.setFont(11.0f);
        g.drawText("L", juce::Rectangle<float>(leftSource.x - 12.0f, leftSource.y - 24.0f, 24.0f, 14.0f).toNearestInt(),
                   juce::Justification::centred, false);
        g.drawText("R", juce::Rectangle<float>(rightSource.x - 12.0f, rightSource.y - 24.0f, 24.0f, 14.0f).toNearestInt(),
                   juce::Justification::centred, false);
    } else {
        const auto source = pointForAzimuth(azimuthDegrees_);
        g.setColour(sourceColour.withAlpha(0.35f));
        g.drawLine(centre.x, centre.y, source.x, source.y, 1.5f);
        g.setColour(sourceColour);
        g.fillEllipse(source.x - 7.0f, source.y - 7.0f, 14.0f, 14.0f);
    }

    auto elevationTrack = elevationArea.reduced(20.0f, 24.0f);
    const float trackX = elevationTrack.getCentreX();
    g.setColour(guideColour);
    g.drawLine(trackX, elevationTrack.getY(), trackX, elevationTrack.getBottom(), 2.0f);

    g.setColour(textColour);
    g.setFont(12.0f);
    g.drawText("+90", elevationArea.removeFromTop(20.0f).toNearestInt(), juce::Justification::centred, false);
    g.drawText("-90", elevationArea.removeFromBottom(20.0f).toNearestInt(), juce::Justification::centred, false);

    if (rendererMode_ == RendererMode::PapaPan) {
        g.setColour(outlineColour);
        g.drawFittedText("Not\nused", elevationArea.toNearestInt(), juce::Justification::centred, 2);
    } else {
        const float normalizedElevation = (elevationDegrees_ + 90.0f) / 180.0f;
        const float y = elevationTrack.getBottom() - normalizedElevation * elevationTrack.getHeight();
        g.setColour(sourceColour);
        g.fillEllipse(trackX - 5.0f, y - 5.0f, 10.0f, 10.0f);
    }
}

} // namespace spacetrace::plugin
