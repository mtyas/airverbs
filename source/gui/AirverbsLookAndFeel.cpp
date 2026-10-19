#include "AirverbsLookAndFeel.h"

namespace airverbs
{

juce::Colour palette::star (int s)
{
    static const juce::uint32 c[] = { 0xffffb347, 0xff4fd1ff, 0xffff5fa2, 0xff9cff6a };
    return juce::Colour (c[juce::jlimit (0, 3, s)]);
}

juce::Colour palette::family (int f)
{
    static const juce::uint32 c[] = { 0xffe8c37a, 0xffd99a6c, 0xff9ec9e8, 0xffb69cff, 0xffff8fb1, 0xfff4f1a1 };
    return juce::Colour (c[juce::jlimit (0, 5, f)]);
}

AirverbsLookAndFeel::AirverbsLookAndFeel()
{
    using namespace palette;
    setColour (juce::ResizableWindow::backgroundColourId, bg);
    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, panelHi);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::PopupMenu::headerTextColourId, textDim);
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::TextButton::buttonColourId, panel);
    setColour (juce::TextButton::buttonOnColourId, panelHi);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, accent);
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0c0d11));
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.3f));
    setColour (juce::TextEditor::outlineColourId, line);
    setColour (juce::TextEditor::focusedOutlineColourId, accent.withAlpha (0.6f));
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, text);
    setColour (juce::ScrollBar::thumbColourId, line);
    setColour (juce::TooltipWindow::backgroundColourId, panel);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, line);
}

juce::Font AirverbsLookAndFeel::font (float height, bool bold)
{
   #if JUCE_MAC
    static const juce::String face = "Avenir Next";
   #elif JUCE_WINDOWS
    static const juce::String face = "Segoe UI";
   #else
    static const juce::String face = juce::Font::getDefaultSansSerifFontName();
   #endif
    return juce::Font (juce::FontOptions (face, height, bold ? juce::Font::bold : juce::Font::plain));
}

juce::Font AirverbsLookAndFeel::mono (float height)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
}

void AirverbsLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (palette::panel);
    g.setColour (palette::line);
    g.drawRect (0, 0, w, h, 1);
}

void AirverbsLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                                             bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                                             const juce::String& shortcutKeyText, const juce::Drawable*, const juce::Colour* textColour)
{
    if (isSeparator)
    {
        g.setColour (palette::line);
        g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
        return;
    }
    auto r = area.reduced (2, 1);
    if (isHighlighted && isActive)
    {
        g.setColour (palette::panelHi);
        g.fillRoundedRectangle (r.toFloat(), 4.0f);
        g.setColour (palette::accent.withAlpha (0.6f));
        g.fillRoundedRectangle (r.removeFromLeft (3).toFloat(), 1.5f);
    }
    auto textArea = area.reduced (14, 0);
    if (isTicked)
    {
        g.setColour (palette::accent);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ (float) area.getX() + 8.0f, (float) area.getCentreY() }));
    }
    g.setColour (textColour != nullptr ? *textColour : (isActive ? palette::text : palette::textFaint));
    g.setFont (font (juce::jmin (15.0f, (float) area.getHeight() * 0.62f), isTicked));
    g.drawFittedText (text, textArea, juce::Justification::centredLeft, 1);
    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour (palette::textDim);
        g.setFont (font (12.0f));
        g.drawText (shortcutKeyText, textArea, juce::Justification::centredRight);
    }
    if (hasSubMenu)
    {
        juce::Path p;
        const float cx = (float) area.getRight() - 12.0f, cy = (float) area.getCentreY();
        p.addTriangle (cx - 3, cy - 4, cx - 3, cy + 4, cx + 2, cy);
        g.setColour (palette::textDim);
        g.fillPath (p);
    }
}

void AirverbsLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& text)
{
    g.setColour (palette::textDim);
    g.setFont (font (11.5f, true));
    g.drawText (text.toUpperCase(), area.reduced (10, 0), juce::Justification::bottomLeft);
}

void AirverbsLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (juce::Colour (0xff0c0d11));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (box.isMouseOver() ? palette::textFaint : palette::line);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    juce::Path p;
    const float cx = (float) w - 10.0f, cy = (float) h * 0.5f;
    p.addTriangle (cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
    g.setColour (palette::textDim);
    g.fillPath (p);
}

void AirverbsLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 0, box.getWidth() - 20, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

void AirverbsLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    auto accent = b.findColour (juce::TextButton::textColourOnId);
    juce::ColourGradient grad (juce::Colour (on ? 0xff1d2129 : 0xff17191f), 0, 0, juce::Colour (on ? 0xff121419 : 0xff0d0e12), 0, r.getHeight(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, 5.0f);
    if (on)
    {
        g.setColour (accent.withAlpha (0.16f));
        g.fillRoundedRectangle (r, 5.0f);
    }
    g.setColour (on ? accent.withAlpha (0.8f) : (over ? palette::textFaint : palette::line));
    g.drawRoundedRectangle (r, 5.0f, on ? 1.2f : 1.0f);
    if (down)
    {
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (r, 5.0f);
    }
    // top highlight
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 4, r.getRight() - 4);
}

void AirverbsLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool)
{
    g.setFont (getTextButtonFont (b, b.getHeight()));
    auto c = b.getToggleState() ? b.findColour (juce::TextButton::textColourOnId) : b.findColour (juce::TextButton::textColourOffId);
    g.setColour (c.withMultipliedAlpha (b.isEnabled() ? (over ? 1.0f : 0.9f) : 0.35f));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 1);
}

juce::Rectangle<int> AirverbsLookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    juce::AttributedString s;
    s.append (text, font (13.5f), palette::text);
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, 320.0f);
    const int w = (int) tl.getWidth() + 18, h = (int) tl.getHeight() + 12;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 12,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 18, w, h).constrainedWithin (parent);
}

void AirverbsLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (palette::panel.withAlpha (0.97f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (palette::accent.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    juce::AttributedString s;
    s.append (text, font (13.5f), palette::text);
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, (float) w - 18.0f);
    tl.draw (g, r.reduced (9.0f, 6.0f));
}

void AirverbsLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor&)
{
    g.setColour (juce::Colour (0xff0c0d11));
    g.fillRoundedRectangle (0, 0, (float) w, (float) h, 4.0f);
}

void AirverbsLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& e)
{
    g.setColour (e.hasKeyboardFocus (true) ? palette::accent.withAlpha (0.6f) : palette::line);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) w - 1.0f, (float) h - 1.0f, 4.0f, 1.0f);
}

void AirverbsLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                         int thumbStart, int thumbSize, bool over, bool down)
{
    auto thumb = vertical ? juce::Rectangle<int> (x + w / 2 - 2, thumbStart, 4, thumbSize)
                          : juce::Rectangle<int> (thumbStart, y + h / 2 - 2, thumbSize, 4);
    g.setColour ((over || down) ? palette::textFaint : palette::line);
    g.fillRoundedRectangle (thumb.toFloat(), 2.0f);
}

} // namespace airverbs
