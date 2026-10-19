#include "StarPanel.h"
#include "../PluginProcessor.h"

namespace airverbs
{

class StarPanel::EngineButton : public juce::Button
{
public:
    EngineButton() : juce::Button ("engine") {}
    juce::String name, familyName;
    juce::Colour familyColour;

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (down ? 0xff181a20 : 0xff0c0d11));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (over ? palette::textFaint : palette::line);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        auto pill = r.reduced (4.0f).removeFromLeft (r.getHeight() * 0.32f);
        g.setColour (familyColour);
        g.fillRoundedRectangle (pill.reduced (0.0f, 2.0f), 2.0f);
        auto text = r.withTrimmedLeft (r.getHeight() * 0.55f).withTrimmedRight (r.getHeight() * 0.7f);
        g.setFont (AirverbsLookAndFeel::font (r.getHeight() * 0.56f, true));
        g.setColour (palette::text);
        g.drawFittedText (name, text.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        g.setFont (AirverbsLookAndFeel::font (r.getHeight() * 0.36f));
        g.setColour (familyColour.withAlpha (0.8f));
        const float nameW = juce::GlyphArrangement::getStringWidth (AirverbsLookAndFeel::font (r.getHeight() * 0.56f, true), name);
        g.drawFittedText (familyName.toUpperCase(), text.withTrimmedLeft (nameW + 8.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        juce::Path p;
        const float cx = r.getRight() - r.getHeight() * 0.4f, cy = r.getCentreY();
        p.addTriangle (cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
        g.setColour (palette::textDim);
        g.fillPath (p);
    }
};

StarPanel::StarPanel (AirverbsProcessor& p, int s)
    : processor (p), slot (s), colour (palette::star (s))
{
    engineButton = std::make_unique<EngineButton>();
    engineButton->setTooltip ("Choose the Airwindows algorithm for star " + ids::slotName (s));
    engineButton->onClick = [this] { showEngineMenu(); };
    addAndMakeVisible (*engineButton);

    for (auto* b : { &prevButton, &nextButton, &diceButton })
    {
        b->accent = colour;
        addAndMakeVisible (*b);
    }
    prevButton.onClick = [this] { stepEngine (-1); };
    nextButton.onClick = [this] { stepEngine (1); };
    diceButton.onClick = [this] { processor.randomise (slot); };

    mute = std::make_unique<GlowToggle> (processor, ids::mute (s), "MUTE", palette::warn);
    mute->setTooltip ("Mute this star. In Chain, Twin and Bloom a muted star becomes a bypass so the signal flows through.");
    addAndMakeVisible (*mute);

    for (int k = 0; k < EngineRegistry::maxControls; ++k)
        addAndMakeVisible (natives.add (new AirKnob (processor, ids::control (s, k), colour)));

    level    = std::make_unique<AirKnob> (processor, ids::level (s), colour.withMultipliedSaturation (0.5f));
    pan      = std::make_unique<AirKnob> (processor, ids::pan (s), colour.withMultipliedSaturation (0.5f), true);
    width    = std::make_unique<AirKnob> (processor, ids::slotWidth (s), colour.withMultipliedSaturation (0.5f));
    predelay = std::make_unique<AirKnob> (processor, ids::predelay (s), colour.withMultipliedSaturation (0.5f));
    level->setDescription ("Output level of this star. In series topologies it also sets how hard it drives the next star.");
    pan->setDescription ("Balance of this star's return.");
    width->setDescription ("Stereo width of this star's return: 0% mono, 100% as-is, 200% extra wide.");
    predelay->setDescription ("Delay before this star, up to 500 ms. Offset the stars to build early and late fields.");
    for (auto* k : { level.get(), pan.get(), width.get(), predelay.get() })
        addAndMakeVisible (*k);

    auto* engineParam = processor.apvts.getParameter (ids::engine (s));
    engineAttachment = std::make_unique<juce::ParameterAttachment> (*engineParam, [this] (float v) { engineChanged ((int) v); }, nullptr);
    engineAttachment->sendInitialUpdate();
}

StarPanel::~StarPanel() = default;

void StarPanel::engineChanged (int index)
{
    engineIndex = EngineRegistry::get().clampIndex (index);
    const auto& info = EngineRegistry::get().info (engineIndex);
    engineButton->name = info.name;
    engineButton->familyName = EngineRegistry::get().familyNames()[(size_t) info.family];
    engineButton->familyColour = palette::family (info.family);
    engineButton->setTooltip (juce::String (info.description) + "\n\nClick to choose another of the 53 Airwindows reverbs.");
    engineButton->repaint();

    for (int k = 0; k < natives.size(); ++k)
    {
        auto* knob = natives[k];
        if (k < (int) info.controls.size())
        {
            const auto c = info.controls[(size_t) k];
            knob->setLabel (c.name);
            knob->setValueFormatter ([c] (float v) { return juce::String (c.formatValue (v)); });
            juce::String role;
            switch (c.role)
            {
                case ParamRole::Size:     role = "Follows the global Size macro."; break;
                case ParamRole::Decay:
                case ParamRole::DecayInv: role = "Follows the global Decay macro."; break;
                case ParamRole::Tone:
                case ParamRole::ToneInv:  role = "Follows the global Tone macro."; break;
                default: break;
            }
            knob->setDescription ("Native " + juce::String (info.name) + " control. " + role);
            knob->setVisible (true);
        }
        else
            knob->setVisible (false);
    }
    resized();
    repaint();
}

void StarPanel::stepEngine (int delta)
{
    const int n = EngineRegistry::get().size();
    processor.setSlotEngine (slot, ((engineIndex + delta) % n + n) % n);
}

void StarPanel::showEngineMenu()
{
    const auto& reg = EngineRegistry::get();
    juce::PopupMenu menu;
    menu.addSectionHeader ("Star " + ids::slotName (slot) + " - Airwindows engines");
    for (size_t f = 0; f < reg.familyNames().size(); ++f)
    {
        juce::PopupMenu sub;
        bool containsCurrent = false;
        for (int i = 0; i < reg.size(); ++i)
            if (reg.info (i).family == (int) f)
            {
                sub.addItem (i + 1, reg.info (i).name, true, i == engineIndex);
                containsCurrent |= i == engineIndex;
            }
        menu.addSubMenu (reg.familyNames()[f], sub, true, nullptr, containsCurrent);
    }
    menu.addSeparator();
    menu.addItem (1000, "Random engine");
    juce::Component::SafePointer<StarPanel> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (engineButton.get()).withMinimumWidth (engineButton->getWidth()),
                        [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        if (r == 1000) r = juce::Random::getSystemRandom().nextInt (EngineRegistry::get().size()) + 1;
        safe->processor.setSlotEngine (safe->slot, r - 1);
    });
}

void StarPanel::resized()
{
    const float s = (float) getWidth() / 330.0f;
    auto R = [s] (float x, float y, float w, float h) { return juce::Rectangle<float> (x * s, y * s, w * s, h * s).toNearestInt(); };

    engineButton->setBounds (R (44, 14, 176, 26));
    prevButton.setBounds (R (224, 14, 26, 26));
    nextButton.setBounds (R (252, 14, 26, 26));
    diceButton.setBounds (R (288, 14, 28, 26));

    int visible = 0;
    for (auto* k : natives) visible += k->isVisible() ? 1 : 0;
    const float kw = 51.0f;
    const float startX = 12.0f + (6 - visible) * kw * 0.5f;
    int idx = 0;
    for (auto* k : natives)
        if (k->isVisible())
            k->setBounds (R (startX + kw * (float) idx++, 62, kw, 64));

    const float bw = 56.0f;
    level->setBounds (R (12, 146, bw, 66));
    pan->setBounds (R (12 + bw, 146, bw, 66));
    width->setBounds (R (12 + bw * 2, 146, bw, 66));
    predelay->setBounds (R (12 + bw * 3, 146, bw, 66));
    mute->setBounds (R (250, 168, 68, 24));
}

void StarPanel::updateMeters()
{
    const float e = processor.network.telemetry.energy[(size_t) slot].load();
    const float w = processor.network.telemetry.weight[(size_t) slot].load();
    if (std::abs (e - energy) > 0.002f || std::abs (w - weight) > 0.01f)
    {
        energy = e; weight = w;
        repaint (getLocalBounds().removeFromTop (juce::roundToInt (50.0f * (float) getWidth() / 330.0f)));
        repaint (getLocalBounds().removeFromRight (juce::roundToInt (18.0f * (float) getWidth() / 330.0f)));
    }
}

void StarPanel::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / 330.0f;
    // badge
    auto badge = juce::Rectangle<float> (12 * s, 14 * s, 26 * s, 26 * s);
    const float glow = juce::jlimit (0.0f, 1.0f, std::sqrt (energy) * 2.5f);
    for (int i = 3; i > 0; --i)
    {
        g.setColour (colour.withAlpha (0.08f * glow * (float) i));
        g.fillEllipse (badge.expanded ((float) i * 3.0f * s));
    }
    g.setColour (colour.withAlpha (0.18f + 0.5f * weight));
    g.fillEllipse (badge);
    g.setColour (colour);
    g.drawEllipse (badge.reduced (0.75f), 1.5f);
    g.setFont (AirverbsLookAndFeel::font (16.0f * s, true));
    g.setColour (juce::Colours::white);
    g.drawText (ids::slotName (slot), badge, juce::Justification::centred);

    // description
    const auto& info = EngineRegistry::get().info (engineIndex);
    g.setFont (AirverbsLookAndFeel::font (11.0f * s));
    g.setColour (palette::textDim);
    g.drawFittedText (info.description, juce::Rectangle<float> (14 * s, 42 * s, 302 * s, 16 * s).toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
    if (info.controls.empty())
    {
        g.setFont (AirverbsLookAndFeel::font (12.0f * s));
        g.setColour (palette::textFaint);
        g.drawText ("A calibrated reference space with no controls.", juce::Rectangle<float> (12 * s, 62 * s, 306 * s, 64 * s), juce::Justification::centred);
    }

    g.setColour (palette::line.withAlpha (0.8f));
    g.fillRect (juce::Rectangle<float> (14 * s, 136 * s, 302 * s, 1.0f));
    drawSectionTitle (g, { 250 * s, 146 * s, 68 * s, 14 * s }, "MIX", colour);

    // energy meter
    auto meter = juce::Rectangle<float> (316 * s, 62 * s, 4 * s, 64 * s);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (meter, 2.0f);
    const float lvl = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (energy, -60.0f) + 60.0f) / 60.0f);
    g.setColour (colour.withAlpha (0.85f));
    g.fillRoundedRectangle (meter.withTrimmedTop (meter.getHeight() * (1.0f - lvl)), 2.0f);
}

} // namespace airverbs
