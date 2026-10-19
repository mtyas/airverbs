#pragma once

#include "Widgets.h"

namespace airverbs
{

/** One "star": an Airwindows engine slot with its native controls. */
class StarPanel : public juce::Component
{
public:
    StarPanel (AirverbsProcessor&, int slot);
    ~StarPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void updateMeters();

private:
    class EngineButton;
    void engineChanged (int index);
    void showEngineMenu();
    void stepEngine (int delta);

    AirverbsProcessor& processor;
    const int slot;
    const juce::Colour colour;
    std::unique_ptr<EngineButton> engineButton;
    IconButton prevButton { IconButton::Icon::Left, "Previous engine" };
    IconButton nextButton { IconButton::Icon::Right, "Next engine" };
    IconButton diceButton { IconButton::Icon::Dice, "Randomise this star" };
    std::unique_ptr<GlowToggle> mute;
    juce::OwnedArray<AirKnob> natives;
    std::unique_ptr<AirKnob> level, pan, width, predelay;
    std::unique_ptr<juce::ParameterAttachment> engineAttachment;
    int engineIndex = 0;
    float energy = 0.0f, weight = 0.0f;
};

} // namespace airverbs
