#include "Widgets.h"
#include "../PluginProcessor.h"
#include "BinaryData.h"

namespace airverbs
{

// ------------------------------------------------------------------------------------------ Layout
static const juce::var& layoutJson()
{
    static const juce::var v = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::layout_json, BinaryData::layout_jsonSize));
    return v;
}

static juce::Rectangle<float> rectFrom (const juce::var& a)
{
    if (! a.isArray() || a.size() < 4) return {};
    return { (float) a[0], (float) a[1], (float) a[2], (float) a[3] };
}

juce::Rectangle<float> Layout::get (const juce::String& name) { return rectFrom (layoutJson()[juce::Identifier (name)]); }
juce::Rectangle<float> Layout::star (int s)
{
    return rectFrom (layoutJson()["stars"][juce::Identifier (juce::String::charToString ((juce::juce_wchar) ('A' + s)))]);
}
juce::Point<float> Layout::size() { auto a = layoutJson()["size"]; return { (float) a[0], (float) a[1] }; }

juce::Image getKnobStrip()
{
    static juce::Image img = juce::ImageCache::getFromMemory (BinaryData::knob_png, BinaryData::knob_pngSize);
    return img;
}

juce::Image getChassis()
{
    static juce::Image img = juce::ImageCache::getFromMemory (BinaryData::chassis_png, BinaryData::chassis_pngSize);
    return img;
}

void drawSectionTitle (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title, juce::Colour accent)
{
    g.setFont (AirverbsLookAndFeel::font (area.getHeight() * 0.78f, true).withExtraKerningFactor (0.18f));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawText (title, area.translated (0, 1), juce::Justification::centredLeft);
    g.setColour (palette::textDim);
    g.drawText (title, area, juce::Justification::centredLeft);
    const float tw = juce::GlyphArrangement::getStringWidth (AirverbsLookAndFeel::font (area.getHeight() * 0.78f, true).withExtraKerningFactor (0.18f), title);
    g.setColour (accent.withAlpha (0.5f));
    g.fillRect (area.getX() + tw + 8.0f, area.getCentreY(), juce::jmax (0.0f, area.getWidth() - tw - 8.0f), 1.0f);
}

// ------------------------------------------------------------------------------------------ AirKnob
AirKnob::AirKnob (AirverbsProcessor& p, const juce::String& id, juce::Colour a, bool bi)
    : processor (p), paramID (id), accent (a), bipolar (bi)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    setMouseDragSensitivity (220);
    setScrollWheelEnabled (true);
    param = processor.apvts.getParameter (id);
    jassert (param != nullptr);
    label = param->getName (32);
    for (auto prefix : { "Star A ", "Star B ", "Star C ", "Star D " })
        label = label.replace (prefix, "");
    attachment = std::make_unique<juce::SliderParameterAttachment> (*param, *this, nullptr);
    setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    updateTooltip();
}

AirKnob::~AirKnob() { stopTimer(); }

juce::String AirKnob::valueText() const
{
    if (formatter) return formatter ((float) param->getValue());
    return param->getCurrentValueAsText();
}

void AirKnob::updateTooltip()
{
    juce::String t = label + ": " + valueText();
    if (description.isNotEmpty()) t << "\n" << description;
    const auto midi = processor.midiLearn.describeMapping (paramID);
    if (midi.isNotEmpty()) t << "\nMIDI: " << midi;
    t << "\nRight-click for MIDI learn.";
    setTooltip (t);
}

void AirKnob::valueChanged() { updateTooltip(); repaint(); }

void AirKnob::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float labelH = juce::jmax (9.0f, b.getWidth() * 0.2f);
    auto textArea = b.removeFromBottom (labelH);
    const float side = juce::jmin (b.getWidth(), b.getHeight());
    auto knobArea = juce::Rectangle<float> (side, side).withCentre (b.getCentre());

    const float norm = (float) valueToProportionOfLength (getValue());
    const auto& rp = getRotaryParameters();
    const float a0 = rp.startAngleRadians, a1 = rp.endAngleRadians;
    const float aV = a0 + norm * (a1 - a0);
    const float aFrom = bipolar ? (a0 + a1) * 0.5f : a0;
    const auto centre = knobArea.getCentre();
    const float rArc = side * 0.5f - 1.5f;

    // track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, rArc, rArc, 0.0f, a0, a1, true);
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.strokePath (track, juce::PathStrokeType (side * 0.06f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc with glow
    if (std::abs (aV - aFrom) > 0.001f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, rArc, rArc, 0.0f, std::min (aFrom, aV), std::max (aFrom, aV), true);
        g.setColour (accent.withAlpha (0.18f));
        g.strokePath (arc, juce::PathStrokeType (side * 0.13f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent.withAlpha (isEnabled() ? 0.95f : 0.4f));
        g.strokePath (arc, juce::PathStrokeType (side * 0.045f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 3D knob body from the filmstrip
    auto strip = getKnobStrip();
    if (strip.isValid())
    {
        const int frameSize = strip.getWidth();
        const int frames = strip.getHeight() / frameSize;
        const int f = juce::jlimit (0, frames - 1, juce::roundToInt (norm * (float) (frames - 1)));
        auto dst = knobArea.reduced (side * 0.07f);
        g.drawImage (strip, dst.getX(), dst.getY(), dst.getWidth(), dst.getHeight(), 0, f * frameSize, frameSize, frameSize);
    }
    else
    {
        g.setColour (palette::panelHi);
        g.fillEllipse (knobArea.reduced (side * 0.12f));
    }

    // MIDI learn pulse / mapped badge
    if (processor.midiLearn.getLearningParam() == paramID)
    {
        g.setColour (palette::warn.withAlpha (0.35f + 0.35f * std::sin (learnPulse)));
        g.drawEllipse (knobArea.reduced (1.0f), 2.5f);
    }
    else if (processor.midiLearn.describeMapping (paramID).isNotEmpty())
    {
        auto dot = juce::Rectangle<float> (side * 0.16f, side * 0.16f).withCentre ({ knobArea.getRight() - side * 0.1f, knobArea.getY() + side * 0.1f });
        g.setColour (palette::warn.withAlpha (0.9f));
        g.fillEllipse (dot);
    }

    // label / value
    const bool showValue = hovering || isMouseButtonDown();
    g.setFont (AirverbsLookAndFeel::font (labelH * 0.78f, ! showValue));
    g.setColour (showValue ? accent.brighter (0.3f) : palette::textDim);
    g.drawFittedText (showValue ? valueText() : label.toUpperCase(), textArea.toNearestInt(), juce::Justification::centred, 1, 0.8f);
}

void AirKnob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showContextMenu(); return; }
    setMouseDragSensitivity (e.mods.isShiftDown() ? 1400 : 220);
    juce::Slider::mouseDown (e);
}

void AirKnob::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu()) juce::Slider::mouseUp (e);
    repaint();
}

void AirKnob::mouseEnter (const juce::MouseEvent& e) { hovering = true; juce::Slider::mouseEnter (e); repaint(); }
void AirKnob::mouseExit (const juce::MouseEvent& e)  { hovering = false; juce::Slider::mouseExit (e); repaint(); }

void AirKnob::timerCallback()
{
    learnPulse += 0.35f;
    if (processor.midiLearn.getLearningParam() != paramID) { stopTimer(); updateTooltip(); }
    repaint();
}

void AirKnob::showContextMenu()
{
    juce::PopupMenu m;
    const bool learning = processor.midiLearn.getLearningParam() == paramID;
    const auto mapping = processor.midiLearn.describeMapping (paramID);
    m.addSectionHeader (label);
    m.addItem (1, learning ? "Cancel MIDI Learn" : "MIDI Learn");
    m.addItem (2, mapping.isEmpty() ? juce::String ("Forget MIDI") : "Forget MIDI (" + mapping + ")", mapping.isNotEmpty());
    m.addSeparator();
    m.addItem (3, "Reset to default");
    juce::Component::SafePointer<AirKnob> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
    {
        if (safe == nullptr) return;
        auto& self = *safe;
        if (r == 1)
        {
            if (self.processor.midiLearn.getLearningParam() == self.paramID) self.processor.midiLearn.cancelLearning();
            else { self.processor.midiLearn.startLearning (self.paramID); self.startTimerHz (20); }
        }
        else if (r == 2) self.processor.midiLearn.clearMapping (self.paramID);
        else if (r == 3)
        {
            self.param->beginChangeGesture();
            self.param->setValueNotifyingHost (self.param->getDefaultValue());
            self.param->endChangeGesture();
        }
        self.updateTooltip();
        self.repaint();
    });
}

// ------------------------------------------------------------------------------------------ GlowToggle
GlowToggle::GlowToggle (AirverbsProcessor& p, const juce::String& id, const juce::String& text, juce::Colour a)
    : juce::Button (text), processor (p), paramID (id), accent (a)
{
    setClickingTogglesState (true);
    if (auto* prm = processor.apvts.getParameter (id))
        attachment = std::make_unique<juce::ButtonParameterAttachment> (*prm, *this, nullptr);
}

void GlowToggle::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        juce::PopupMenu m;
        const auto mapping = processor.midiLearn.describeMapping (paramID);
        m.addItem (1, processor.midiLearn.getLearningParam() == paramID ? "Cancel MIDI Learn" : "MIDI Learn");
        m.addItem (2, mapping.isEmpty() ? juce::String ("Forget MIDI") : "Forget MIDI (" + mapping + ")", mapping.isNotEmpty());
        juce::Component::SafePointer<GlowToggle> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
        {
            if (safe == nullptr) return;
            if (r == 1)
            {
                if (safe->processor.midiLearn.getLearningParam() == safe->paramID) safe->processor.midiLearn.cancelLearning();
                else safe->processor.midiLearn.startLearning (safe->paramID);
            }
            if (r == 2) safe->processor.midiLearn.clearMapping (safe->paramID);
            safe->repaint();
        });
        return;
    }
    juce::Button::mouseDown (e);
}

void GlowToggle::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();
    const float rad = r.getHeight() * 0.5f;
    g.setColour (juce::Colour (0xff0b0c10));
    g.fillRoundedRectangle (r, rad);
    if (on)
    {
        g.setColour (accent.withAlpha (0.22f));
        g.fillRoundedRectangle (r, rad);
        g.setColour (accent.withAlpha (0.12f));
        g.fillRoundedRectangle (r.expanded (2.0f), rad + 2.0f);
    }
    g.setColour (on ? accent : (over ? palette::textFaint : palette::line));
    g.drawRoundedRectangle (r, rad, on ? 1.4f : 1.0f);
    auto led = juce::Rectangle<float> (r.getHeight() * 0.32f, r.getHeight() * 0.32f).withCentre ({ r.getX() + rad, r.getCentreY() });
    g.setColour (on ? accent : palette::textFaint.withAlpha (0.5f));
    g.fillEllipse (led);
    g.setFont (AirverbsLookAndFeel::font (r.getHeight() * 0.5f, true).withExtraKerningFactor (0.1f));
    g.setColour (on ? accent.brighter (0.4f) : palette::textDim);
    g.drawText (getButtonText(), r.withTrimmedLeft (rad * 1.4f), juce::Justification::centred);
    if (down) { g.setColour (juce::Colours::black.withAlpha (0.2f)); g.fillRoundedRectangle (r, rad); }
    if (processor.midiLearn.getLearningParam() == paramID)
    {
        g.setColour (palette::warn);
        g.drawRoundedRectangle (r.expanded (1.0f), rad, 2.0f);
    }
}

// ------------------------------------------------------------------------------------------ IconButton
void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (down ? 0xff1d2027 : (over ? 0xff181a20 : 0xff111318)));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (getToggleState() ? accent : (over ? palette::textFaint : palette::line));
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    drawIcon (g, icon, r.reduced (r.getHeight() * 0.27f), isEnabled() ? (over || getToggleState() ? accent : palette::text) : palette::textFaint);
}

void IconButton::drawIcon (juce::Graphics& g, Icon icon, juce::Rectangle<float> r, juce::Colour c)
{
    juce::Path p;
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    const float st = juce::jmax (1.3f, w * 0.11f);
    juce::PathStrokeType stroke (st, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (c);
    switch (icon)
    {
        case Icon::Undo:
        case Icon::Redo:
        {
            const bool redo = icon == Icon::Redo;
            juce::Path arc;
            arc.addCentredArc (x + w * 0.5f, y + h * 0.58f, w * 0.38f, h * 0.36f, 0.0f,
                               redo ? -juce::MathConstants<float>::halfPi * 1.3f : juce::MathConstants<float>::halfPi * 1.3f, 0.0f, true);
            g.strokePath (arc, stroke);
            const float ax = redo ? x + w * 0.82f : x + w * 0.18f;
            p.addTriangle (ax - w * 0.16f, y + h * 0.42f, ax + w * 0.16f, y + h * 0.42f, ax, y + h * 0.7f);
            g.fillPath (p);
            break;
        }
        case Icon::History:
            g.drawEllipse (r.reduced (w * 0.05f), st);
            p.startNewSubPath (x + w * 0.5f, y + h * 0.22f); p.lineTo (x + w * 0.5f, y + h * 0.52f); p.lineTo (x + w * 0.7f, y + h * 0.64f);
            g.strokePath (p, stroke);
            break;
        case Icon::Dice:
            g.drawRoundedRectangle (r.reduced (w * 0.04f), w * 0.18f, st);
            for (auto pt : { juce::Point<float> (0.3f, 0.3f), { 0.7f, 0.7f }, { 0.5f, 0.5f }, { 0.7f, 0.3f }, { 0.3f, 0.7f } })
                g.fillEllipse (juce::Rectangle<float> (w * 0.15f, h * 0.15f).withCentre ({ x + w * pt.x, y + h * pt.y }));
            break;
        case Icon::Left:
        case Icon::Right:
        {
            const bool right = icon == Icon::Right;
            p.startNewSubPath (x + w * (right ? 0.35f : 0.65f), y + h * 0.15f);
            p.lineTo (x + w * (right ? 0.68f : 0.32f), y + h * 0.5f);
            p.lineTo (x + w * (right ? 0.35f : 0.65f), y + h * 0.85f);
            g.strokePath (p, stroke);
            break;
        }
        case Icon::Save:
            p.addRoundedRectangle (r.reduced (w * 0.05f), w * 0.1f);
            g.strokePath (p, stroke);
            g.fillRect (juce::Rectangle<float> (x + w * 0.3f, y + h * 0.1f, w * 0.4f, h * 0.25f));
            g.drawRect (juce::Rectangle<float> (x + w * 0.25f, y + h * 0.55f, w * 0.5f, h * 0.35f), st * 0.8f);
            break;
        case Icon::Midi:
        {
            g.drawEllipse (r.reduced (w * 0.04f), st);
            for (int i = 0; i < 5; ++i)
            {
                const float a = juce::MathConstants<float>::pi * (0.85f + 0.325f * (float) i);
                g.fillEllipse (juce::Rectangle<float> (w * 0.13f, h * 0.13f).withCentre ({ x + w * 0.5f + std::cos (a) * w * 0.27f, y + h * 0.5f + std::sin (a) * h * 0.27f }));
            }
            break;
        }
        case Icon::Info:
            g.drawEllipse (r.reduced (w * 0.04f), st);
            g.fillEllipse (juce::Rectangle<float> (w * 0.14f, h * 0.14f).withCentre ({ x + w * 0.5f, y + h * 0.3f }));
            g.fillRoundedRectangle (juce::Rectangle<float> (w * 0.12f, h * 0.34f).withCentre ({ x + w * 0.5f, y + h * 0.62f }), w * 0.05f);
            break;
        case Icon::Folder:
            p.startNewSubPath (x, y + h * 0.2f); p.lineTo (x + w * 0.38f, y + h * 0.2f); p.lineTo (x + w * 0.48f, y + h * 0.32f);
            p.lineTo (x + w, y + h * 0.32f); p.lineTo (x + w, y + h * 0.85f); p.lineTo (x, y + h * 0.85f); p.closeSubPath();
            g.strokePath (p, stroke);
            break;
        case Icon::Star:
            p.addStar (r.getCentre(), 5, w * 0.22f, w * 0.5f, 0.0f);
            g.strokePath (p, stroke);
            break;
        case Icon::Init:
            g.drawEllipse (r.reduced (w * 0.1f), st);
            p.startNewSubPath (x + w * 0.5f, y); p.lineTo (x + w * 0.5f, y + h * 0.45f);
            g.strokePath (p, stroke);
            break;
        case Icon::Compare:
            g.setFont (AirverbsLookAndFeel::font (h * 0.95f, true));
            g.drawText ("A/B", r.expanded (w * 0.3f, 0), juce::Justification::centred);
            break;
    }
}

} // namespace airverbs
