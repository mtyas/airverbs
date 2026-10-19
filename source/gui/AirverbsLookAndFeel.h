#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace airverbs
{

namespace palette
{
    const juce::Colour bg       { 0xff08090b };
    const juce::Colour panel    { 0xff111318 };
    const juce::Colour panelHi  { 0xff1b1e25 };
    const juce::Colour line     { 0xff2a2e37 };
    const juce::Colour text     { 0xffdfe4ec };
    const juce::Colour textDim  { 0xff8a93a3 };
    const juce::Colour textFaint{ 0xff535a67 };
    const juce::Colour accent   { 0xff9fe7ff };
    const juce::Colour warn     { 0xffff7a59 };
    const juce::Colour freeze   { 0xffbfe9ff };

    juce::Colour star (int slot);
    juce::Colour family (int family);
}

class AirverbsLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AirverbsLookAndFeel();

    static juce::Font font (float height, bool bold = false);
    static juce::Font mono (float height);

    juce::Font getPopupMenuFont() override { return font (15.0f); }
    juce::Font getComboBoxFont (juce::ComboBox& b) override { return font (juce::jmin (15.0f, (float) b.getHeight() * 0.5f)); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (juce::jmin (14.0f, (float) h * 0.48f), true); }
    juce::Font getLabelFont (juce::Label& l) override { return font (l.getFont().getHeight()); }

    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String&, juce::Point<int>, juce::Rectangle<int>) override;
    void fillTextEditorBackground (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int w, int h, juce::TextEditor&) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down) override;
};

} // namespace airverbs
