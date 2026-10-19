#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "AirverbsLookAndFeel.h"

namespace airverbs
{

class AirverbsProcessor;

/** Rectangles from assets/layout.json (shared with the Blender chassis scene). */
struct Layout
{
    static juce::Rectangle<float> get (const juce::String& name);
    static juce::Rectangle<float> star (int slot);
    static juce::Point<float> size();
};

/** Shared filmstrip of the Blender-rendered knob. */
juce::Image getKnobStrip();
juce::Image getChassis();

/**
    Rotary control drawn from the 3D-rendered filmstrip, with a glowing value arc.
    Right-click: MIDI learn / forget, reset. Double-click: reset. Shift-drag: fine.
*/
class AirKnob : public juce::Slider,
                private juce::Timer
{
public:
    AirKnob (AirverbsProcessor&, const juce::String& paramID, juce::Colour accent, bool bipolar = false);
    ~AirKnob() override;

    void setLabel (const juce::String& l) { label = l; repaint(); }
    void setAccent (juce::Colour c)       { accent = c; repaint(); }
    void setValueFormatter (std::function<juce::String (float)> f) { formatter = std::move (f); repaint(); }
    void setDescription (const juce::String& d) { description = d; updateTooltip(); }
    const juce::String& getParamID() const { return paramID; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void valueChanged() override;

private:
    void timerCallback() override;
    void showContextMenu();
    void updateTooltip();
    juce::String valueText() const;

    AirverbsProcessor& processor;
    juce::String paramID, label, description;
    juce::Colour accent;
    bool bipolar;
    std::function<juce::String (float)> formatter;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::SliderParameterAttachment> attachment;
    bool hovering = false;
    float learnPulse = 0.0f;
};

/** Pill toggle bound to a bool parameter (Mute, Freeze...). */
class GlowToggle : public juce::Button
{
public:
    GlowToggle (AirverbsProcessor&, const juce::String& paramID, const juce::String& text, juce::Colour accent);
    void paintButton (juce::Graphics&, bool over, bool down) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    AirverbsProcessor& processor;
    juce::String paramID;
    juce::Colour accent;
    std::unique_ptr<juce::ButtonParameterAttachment> attachment;
};

/** Small vector-drawn icon button for the top bar. */
class IconButton : public juce::Button
{
public:
    enum class Icon { Undo, Redo, History, Dice, Left, Right, Save, Midi, Info, Folder, Star, Init, Compare };
    IconButton (Icon i, const juce::String& tip) : juce::Button (tip), icon (i) { setTooltip (tip); }
    void paintButton (juce::Graphics&, bool over, bool down) override;
    static void drawIcon (juce::Graphics&, Icon, juce::Rectangle<float>, juce::Colour);
    juce::Colour accent = palette::accent;
private:
    Icon icon;
};

/** Engraved section title. */
void drawSectionTitle (juce::Graphics&, juce::Rectangle<float> area, const juce::String& title, juce::Colour accent);

} // namespace airverbs
