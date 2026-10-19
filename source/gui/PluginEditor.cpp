#include "PluginEditor.h"
#include "../PluginProcessor.h"

namespace airverbs
{

class AirverbsEditor::PresetNameButton : public juce::Button
{
public:
    explicit PresetNameButton (AirverbsProcessor& p) : juce::Button ("preset"), processor (p)
    {
        setTooltip ("Open the preset browser");
    }
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colour (0xff07080a));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (over ? palette::textFaint : palette::line);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        // inner LCD glow
        g.setColour (palette::accent.withAlpha (0.05f));
        g.fillRoundedRectangle (r.reduced (3.0f), 3.0f);

        auto& pm = processor.presets;
        const int idx = pm.getCurrentIndex();
        juce::String cat = idx >= 0 ? pm.getAll()[(size_t) idx].category : juce::String ("Init");
        auto text = r.reduced (12.0f, 0.0f);
        g.setFont (AirverbsLookAndFeel::font (r.getHeight() * 0.32f, true).withExtraKerningFactor (0.12f));
        g.setColour (palette::accent.withAlpha (0.6f));
        g.drawText (cat.toUpperCase(), text, juce::Justification::centredRight);
        g.setFont (AirverbsLookAndFeel::font (r.getHeight() * 0.5f, true));
        g.setColour (palette::text);
        g.drawText (pm.getCurrentName() + (dirty ? " *" : ""), text.withTrimmedRight (110.0f), juce::Justification::centredLeft);
    }
    bool dirty = false;
private:
    AirverbsProcessor& processor;
};

AirverbsEditor::AirverbsEditor (AirverbsProcessor& p) : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lnf);
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);

    for (int s = 0; s < numSlots; ++s)
        addAndMakeVisible (stars.add (new StarPanel (processor, s)));
    view = std::make_unique<ConstellationView> (processor);
    addAndMakeVisible (*view);

    // ---- top bar
    presetName = std::make_unique<PresetNameButton> (processor);
    presetName->onClick = [this] { openBrowser(); };
    addAndMakeVisible (*presetName);
    prevPreset.onClick = [this] { processor.presets.loadNext (-1); };
    nextPreset.onClick = [this] { processor.presets.loadNext (1); };
    savePreset.onClick = [this] { openBrowser (true); };
    undo.onClick = [this] { processor.history.undo(); };
    redo.onClick = [this] { processor.history.redo(); };
    historyButton.onClick = [this] { showHistoryMenu(); };
    dice.onClick = [this] { processor.randomise (-1); };
    midi.onClick = [this] { showMidiMenu(); };
    info.onClick = [this] { showAbout(); };
    for (auto* b : { &prevPreset, &nextPreset, &savePreset, &undo, &redo, &historyButton, &dice, &midi, &info })
        addAndMakeVisible (*b);

    // ---- topology
    for (int t = 0; t < (int) Topology::count; ++t)
    {
        auto* b = topoButtons.add (new juce::TextButton (topologyName (t)));
        b->setClickingTogglesState (false);
        b->setTooltip (topologyDescription (t));
        b->setColour (juce::TextButton::textColourOnId, palette::accent);
        b->onClick = [this, t]
        {
            if (auto* prm = processor.apvts.getParameter (ids::topology))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) t));
                prm->endChangeGesture();
            }
        };
        addAndMakeVisible (b);
    }
    morph = std::make_unique<AirKnob> (processor, ids::morph, palette::accent);
    addAndMakeVisible (*morph);
    topoAttachment = std::make_unique<juce::ParameterAttachment> (*processor.apvts.getParameter (ids::topology),
                                                                  [this] (float v) { updateTopology ((int) v); }, nullptr);
    topoAttachment->sendInitialUpdate();

    // ---- motion
    const auto motionCol = juce::Colour (0xffc9b6ff);
    orbitRate  = std::make_unique<AirKnob> (processor, ids::orbitRate, motionCol);
    orbitDepth = std::make_unique<AirKnob> (processor, ids::orbitDepth, motionCol);
    vecX       = std::make_unique<AirKnob> (processor, ids::vecX, motionCol, true);
    vecY       = std::make_unique<AirKnob> (processor, ids::vecY, motionCol, true);
    orbitRate->setLabel ("Rate"); orbitDepth->setLabel ("Depth"); vecX->setLabel ("X"); vecY->setLabel ("Y");
    orbitRate->setDescription ("Speed of the listener's orbit around its position.");
    orbitDepth->setDescription ("Size of the orbit. At 0 the listener stays where you put it.");
    vecX->setDescription ("Listener position, left (A/C) to right (B/D). Drag the puck in the 3D view.");
    vecY->setDescription ("Listener position, top (A/B) to bottom (C/D). Drag the puck in the 3D view.");
    orbitShape.addItemList ({ "Circle", "Figure 8", "Lissajous", "Drift", "Pendulum" }, 1);
    orbitShapeAttachment = std::make_unique<juce::ComboBoxParameterAttachment> (*processor.apvts.getParameter (ids::orbitShape), orbitShape, nullptr);
    orbitShape.setTooltip ("Path the listener orbits along.");
    recentre.setTooltip ("Put the listener back in the centre, all four stars equal.");
    recentre.onClick = [this]
    {
        processor.history.beginAction ("Centre listener");
        for (auto& id : { ids::vecX, ids::vecY })
            if (auto* prm = processor.apvts.getParameter (id)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (0.5f); prm->endChangeGesture(); }
        processor.history.commitNow();
    };
    for (auto* c : std::initializer_list<juce::Component*> { orbitRate.get(), orbitDepth.get(), vecX.get(), vecY.get(), &orbitShape, &recentre })
        addAndMakeVisible (c);

    // ---- space
    const auto spaceCol = juce::Colour (0xff9fe7ff);
    size  = std::make_unique<AirKnob> (processor, ids::size, spaceCol, true);
    decay = std::make_unique<AirKnob> (processor, ids::decay, spaceCol, true);
    tone  = std::make_unique<AirKnob> (processor, ids::tone, spaceCol, true);
    regen = std::make_unique<AirKnob> (processor, ids::feedback, juce::Colour (0xffffc46b));
    size->setDescription ("Macro: pushes every star's size-type controls (Bigness, RmSize, Delay...) up or down together.");
    decay->setDescription ("Macro: pushes every star's decay-type controls (Regen, Sustain, RT60...) up or down together.");
    tone->setDescription ("Macro: brightens or darkens every star's tone controls (Filter, Darkness, Damping...) together.");
    regen->setDescription ("Outer feedback loop: the whole constellation's output flows back into its input. Turn it up for blooming, evolving tails.");
    freeze = std::make_unique<GlowToggle> (processor, ids::freeze, "FREEZE", palette::freeze);
    freeze->setTooltip ("Freeze: mutes the input and holds the current sound in the feedback loop indefinitely.");
    for (auto* c : std::initializer_list<juce::Component*> { size.get(), decay.get(), tone.get(), regen.get(), freeze.get() })
        addAndMakeVisible (c);

    // ---- output
    const auto outCol = juce::Colour (0xffdfe4ec);
    duck    = std::make_unique<AirKnob> (processor, ids::duck, outCol);
    width   = std::make_unique<AirKnob> (processor, ids::width, outCol);
    lowCut  = std::make_unique<AirKnob> (processor, ids::lowCut, outCol);
    highCut = std::make_unique<AirKnob> (processor, ids::highCut, outCol);
    input   = std::make_unique<AirKnob> (processor, ids::input, outCol, true);
    mix     = std::make_unique<AirKnob> (processor, ids::mix, palette::accent);
    output  = std::make_unique<AirKnob> (processor, ids::output, outCol, true);
    duck->setDescription ("Ducks the reverb while the dry signal is playing, so the tail blooms in the gaps.");
    width->setDescription ("Stereo width of the whole wet signal.");
    lowCut->setDescription ("High-pass on the wet signal (24 dB/oct).");
    highCut->setDescription ("Low-pass on the wet signal (24 dB/oct).");
    mix->setDescription ("Dry/wet balance (equal power).");
    for (auto* c : { duck.get(), width.get(), lowCut.get(), highCut.get(), input.get(), mix.get(), output.get() })
        addAndMakeVisible (c);

    processor.history.onChange = [this] { triggerAsyncUpdate(); };
    processor.presets.onChange = [this] { triggerAsyncUpdate(); };
    processor.midiLearn.onMappingsChanged = [this] { triggerAsyncUpdate(); };

    auto& ui = processor.uiState;
    const int w = juce::jlimit (840, 1800, (int) ui.getProperty ("width", 1200));
    setResizable (true, true);
    setResizeLimits (840, 546, 1800, 1170);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (1200.0 / 780.0);
    setSize (w, juce::roundToInt (w * 780.0 / 1200.0));
    handleAsyncUpdate();
    startTimerHz (30);
}

AirverbsEditor::~AirverbsEditor()
{
    stopTimer();
    processor.history.onChange = nullptr;
    processor.presets.onChange = nullptr;
    processor.midiLearn.onMappingsChanged = nullptr;
    browser.reset();
    view.reset();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

juce::Rectangle<int> AirverbsEditor::L (float x, float y, float w, float h) const
{
    const float s = (float) getWidth() / 1200.0f;
    return juce::Rectangle<float> (x * s, y * s, w * s, h * s).toNearestInt();
}

void AirverbsEditor::updateTopology (int t)
{
    topology = juce::jlimit (0, (int) Topology::count - 1, t);
    for (int i = 0; i < topoButtons.size(); ++i)
        topoButtons[i]->setToggleState (i == topology, juce::dontSendNotification);
    morph->setLabel (topologyMorphName (topology));
    morph->setDescription (topologyDescription (topology));
    repaint (L (12, 532, 388, 236));
}

void AirverbsEditor::resized()
{
    processor.uiState.setProperty ("width", getWidth(), nullptr);
    for (int s = 0; s < numSlots; ++s)
    {
        auto r = Layout::star (s);
        stars[s]->setBounds (L (r.getX(), r.getY(), r.getWidth(), r.getHeight()));
    }
    auto v = Layout::get ("view");
    view->setBounds (L (v.getX(), v.getY(), v.getWidth(), v.getHeight()));

    // top bar
    prevPreset.setBounds (L (400, 14, 28, 28));
    presetName->setBounds (L (432, 14, 320, 28));
    nextPreset.setBounds (L (756, 14, 28, 28));
    savePreset.setBounds (L (790, 14, 28, 28));
    undo.setBounds (L (850, 14, 28, 28));
    redo.setBounds (L (882, 14, 28, 28));
    historyButton.setBounds (L (914, 14, 28, 28));
    dice.setBounds (L (968, 14, 28, 28));
    midi.setBounds (L (1000, 14, 28, 28));
    info.setBounds (L (1156, 14, 28, 28));

    // topology
    for (int t = 0; t < topoButtons.size(); ++t)
    {
        const int row = t < 4 ? 0 : 1, col = t < 4 ? t : t - 4;
        topoButtons[t]->setBounds (L (24.0f + (float) col * 91.0f, 568.0f + (float) row * 34.0f, 86, 28));
    }
    morph->setBounds (L (22, 640, 76, 92));

    // motion
    orbitRate->setBounds (L (418, 564, 70, 84));
    orbitDepth->setBounds (L (490, 564, 70, 84));
    orbitShape.setBounds (L (564, 592, 66, 26));
    vecX->setBounds (L (418, 660, 70, 84));
    vecY->setBounds (L (490, 660, 70, 84));
    recentre.setBounds (L (564, 690, 66, 26));

    // space
    size->setBounds (L (656, 562, 80, 96));
    decay->setBounds (L (734, 562, 80, 96));
    tone->setBounds (L (812, 562, 80, 96));
    regen->setBounds (L (656, 664, 80, 96));
    freeze->setBounds (L (746, 694, 140, 34));

    // output
    duck->setBounds (L (916, 562, 64, 80));
    width->setBounds (L (982, 562, 64, 80));
    lowCut->setBounds (L (1048, 562, 64, 80));
    highCut->setBounds (L (1114, 562, 64, 80));
    input->setBounds (L (916, 656, 64, 80));
    mix->setBounds (L (984, 648, 84, 100));
    output->setBounds (L (1070, 656, 64, 80));

    if (browser != nullptr) browser->setBounds (getLocalBounds());
}

void AirverbsEditor::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / 1200.0f;
    auto chassis = getChassis();
    if (chassis.isValid())
        g.drawImage (chassis, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    else
        g.fillAll (palette::bg);

    if (! view->isVisible())
    {
        if (frozenView.isValid()) g.drawImage (frozenView, view->getBounds().toFloat());
        else { g.setColour (juce::Colours::black); g.fillRect (view->getBounds()); }
    }

    auto title = [&] (float x, float y, float w, const juce::String& t, juce::Colour c) { drawSectionTitle (g, { x * s, y * s, w * s, 14 * s }, t, c); };
    title (24, 544, 364, "TOPOLOGY", palette::accent);
    title (420, 544, 208, "ORBIT", juce::Colour (0xffc9b6ff));
    title (660, 544, 228, "SPACE", palette::accent);
    title (920, 544, 256, "OUTPUT", palette::text);

    // morph description
    g.setFont (AirverbsLookAndFeel::font (12.5f * s));
    g.setColour (palette::textDim);
    g.drawFittedText (topologyDescription (topology), L (108, 646, 280, 80), juce::Justification::topLeft, 5, 0.9f);

    g.setFont (AirverbsLookAndFeel::font (10.5f * s, true).withExtraKerningFactor (0.15f));
    g.setColour (palette::textFaint);
    g.drawText ("SHAPE", L (564, 574, 66, 14), juce::Justification::centredLeft);
}

void AirverbsEditor::paintOverChildren (juce::Graphics& g)
{
    // output meters
    const float s = (float) getWidth() / 1200.0f;
    auto meterArea = juce::Rectangle<float> (1146 * s, 662 * s, 30 * s, 66 * s);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto bar = meterArea.withWidth (10 * s).translated ((float) ch * 14 * s, 0);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (bar, 2.0f);
        const float lvl = ch == 0 ? meterL : meterR;
        const float norm = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (lvl, -60.0f) + 60.0f) / 60.0f);
        auto fill = bar.withTrimmedTop (bar.getHeight() * (1.0f - norm));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffff5f5f), bar.getX(), bar.getY(), palette::accent, bar.getX(), bar.getBottom(), false));
        g.fillRoundedRectangle (fill, 2.0f);
    }
    if (processor.midiLearn.isLearning())
    {
        g.setFont (AirverbsLookAndFeel::font (12.0f * s, true));
        g.setColour (palette::warn);
        g.drawText ("MIDI LEARN: move a controller...", L (250, 18, 145, 20), juce::Justification::centredRight);
    }
}

void AirverbsEditor::timerCallback()
{
    if (pendingBrowser >= 0)
    {
        auto img = view->takeCapture();
        if (img.isValid()) frozenView = img;
        if (img.isValid() || ++pendingBrowser > 6) // never wait more than ~200 ms
            showBrowserNow();
    }

    // Dev aid: AIRVERBS_SNAPSHOT=/path/out.png renders the whole GUI (incl. the GL scene) to a PNG.
    static const juce::String snapshotPath = juce::SystemStats::getEnvironmentVariable ("AIRVERBS_SNAPSHOT", {});
    if (snapshotPath.isNotEmpty() && snapshotTicks >= 0)
    {
        if (snapshotTicks == 10 && juce::SystemStats::getEnvironmentVariable ("AIRVERBS_SNAPSHOT_BROWSER", {}).isNotEmpty())
        {
            processor.presets.load (5);
            openBrowser();
        }
        if (++snapshotTicks == 90 && view->isVisible()) view->requestCapture();
        if (snapshotTicks > 90)
        {
            auto gl = view->isVisible() ? view->takeCapture() : frozenView;
            if (gl.isValid())
            {
                const float sc = 2.0f;
                juce::Image out (juce::Image::ARGB, juce::roundToInt (getWidth() * sc), juce::roundToInt (getHeight() * sc), true);
                {
                    juce::Graphics g (out);
                    g.addTransform (juce::AffineTransform::scale (sc));
                    paintEntireComponent (g, true);
                    if (view->isVisible())
                    {
                        g.drawImage (gl, view->getBounds().toFloat());
                        g.setOrigin (view->getPosition());
                        view->paintEntireComponent (g, true);
                    }
                }
                juce::File f (snapshotPath);
                f.deleteFile();
                juce::FileOutputStream os (f);
                juce::PNGImageFormat().writeImageToStream (out, os);
                snapshotTicks = -1;
            }
        }
    }

    for (auto* s : stars) s->updateMeters();
    const float l = processor.network.telemetry.outPeakL.load(), r = processor.network.telemetry.outPeakR.load();
    if (std::abs (l - meterL) > 0.001f || std::abs (r - meterR) > 0.001f)
    {
        meterL = l; meterR = r;
        repaint (L (1140, 656, 40, 80));
    }
    const bool dirty = processor.presets.isDirty();
    if (dirty != presetName->dirty) { presetName->dirty = dirty; presetName->repaint(); }
    if (processor.midiLearn.isLearning()) repaint (L (250, 18, 145, 20));
}

void AirverbsEditor::handleAsyncUpdate()
{
    undo.setEnabled (processor.history.canUndo());
    redo.setEnabled (processor.history.canRedo());
    undo.setTooltip (processor.history.canUndo() ? "Undo " + processor.history.getUndoLabel() : juce::String ("Nothing to undo"));
    redo.setTooltip (processor.history.canRedo() ? "Redo " + processor.history.getRedoLabel() : juce::String ("Nothing to redo"));
    const int n = processor.midiLearn.numMappings();
    midi.setToggleState (n > 0 || processor.midiLearn.isLearning(), juce::dontSendNotification);
    midi.setTooltip ("MIDI learn: right-click any control and choose MIDI Learn. " + juce::String (n) + " mapping(s). Click for the list.");
    presetName->repaint();
    if (browser != nullptr) browser->refresh();
    repaint();
}

void AirverbsEditor::openBrowser (bool save)
{
    if (browser != nullptr)
    {
        if (save) browser->showSaveDialog();
        return;
    }
    pendingBrowserSave = pendingBrowserSave || save;
    if (pendingBrowser >= 0) return;
    // Grab the current 3D frame first; the timer opens the browser once it has arrived.
    view->requestCapture();
    pendingBrowser = 0;
}

void AirverbsEditor::showBrowserNow()
{
    pendingBrowser = -1;
    view->setVisible (false);
    processor.presets.rescan();
    browser = std::make_unique<PresetBrowser> (processor);
    browser->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<AirverbsEditor> (this)] { if (safe != nullptr) safe->closeBrowser(); }); };
    addAndMakeVisible (*browser);
    browser->setBounds (getLocalBounds());
    browser->grabKeyboardFocus();
    if (std::exchange (pendingBrowserSave, false)) browser->showSaveDialog();
    repaint();
}

void AirverbsEditor::closeBrowser()
{
    browser.reset();
    view->setVisible (true);
    frozenView = {};
    grabKeyboardFocus();
    repaint();
}

void AirverbsEditor::showHistoryMenu()
{
    processor.history.commitNow();
    juce::PopupMenu m;
    m.addSectionHeader ("Undo history");
    const auto labels = processor.history.getLabels();
    const int pos = processor.history.getPosition();
    for (int i = labels.size() - 1; i >= juce::jmax (0, labels.size() - 40); --i)
        m.addItem (i + 1, labels[i], true, i == pos);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&historyButton), [this] (int r)
    {
        if (r > 0) processor.history.jumpTo (r - 1);
    });
}

void AirverbsEditor::showMidiMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("MIDI mappings");
    int count = 0;
    for (auto* prm : processor.getParameters())
        if (auto* wp = dynamic_cast<juce::AudioProcessorParameterWithID*> (prm))
        {
            const auto map = processor.midiLearn.describeMapping (wp->paramID);
            if (map.isNotEmpty()) { m.addItem (-1, map + "  >  " + wp->getName (40), false); ++count; }
        }
    if (count == 0) m.addItem (-1, "No mappings yet. Right-click a control and choose MIDI Learn.", false);
    m.addSeparator();
    if (processor.midiLearn.isLearning()) m.addItem (2, "Cancel MIDI learn");
    m.addItem (1, "Clear all mappings", count > 0);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&midi), [this] (int r)
    {
        if (r == 1) processor.midiLearn.clearAll();
        if (r == 2) processor.midiLearn.cancelLearning();
        repaint();
    });
}

void AirverbsEditor::showAbout()
{
    juce::NativeMessageBox::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::InfoIcon)
        .withTitle ("Airverbs " JucePlugin_VersionString " by mtyas")
        .withMessage ("A constellation of the Airwindows reverbs.\n\n"
                      "53 reverb algorithms by Chris Johnson / Airwindows (MIT licence), hosted unmodified, wired together "
                      "by seven topologies, a vector-morphing listener with orbits, a regenerative outer loop and freeze.\n\n"
                      "Tips:\n"
                      "  - Drag the listener in the 3D view to blend the stars; right-drag orbits the camera.\n"
                      "  - Right-click any control for MIDI learn.\n"
                      "  - Shift-drag for fine adjustment, double-click to reset.\n"
                      "  - Cmd/Ctrl+Z undo, Cmd/Ctrl+Shift+Z redo.\n\n"
                      "Airverbs is free and open source, released into the public domain (The Unlicense).\n"
                      "Support Airwindows: patreon.com/airwindows")
        .withButton ("OK").withAssociatedComponent (this), nullptr);
}

bool AirverbsEditor::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    if (mods.isCommandDown() && k.getKeyCode() == 'Z')
    {
        if (mods.isShiftDown()) processor.history.redo(); else processor.history.undo();
        return true;
    }
    if (mods.isCommandDown() && k.getKeyCode() == 'Y') { processor.history.redo(); return true; }
    if (mods.isCommandDown() && k.getKeyCode() == 'S') { openBrowser (true); return true; }
    if (k == juce::KeyPress::leftKey && browser == nullptr) { processor.presets.loadNext (-1); return true; }
    if (k == juce::KeyPress::rightKey && browser == nullptr) { processor.presets.loadNext (1); return true; }
    return false;
}

} // namespace airverbs
