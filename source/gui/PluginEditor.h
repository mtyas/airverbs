#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "AirverbsLookAndFeel.h"
#include "Widgets.h"
#include "StarPanel.h"
#include "ConstellationView.h"
#include "PresetBrowser.h"

namespace airverbs
{

class AirverbsProcessor;

class AirverbsEditor : public juce::AudioProcessorEditor,
                       private juce::Timer,
                       private juce::AsyncUpdater
{
public:
    explicit AirverbsEditor (AirverbsProcessor&);
    ~AirverbsEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    class PresetNameButton;
    void timerCallback() override;
    void handleAsyncUpdate() override;
    void openBrowser (bool save = false);
    void closeBrowser();
    void showHistoryMenu();
    void showMidiMenu();
    void showAbout();
    void updateTopology (int t);
    juce::Rectangle<int> L (float x, float y, float w, float h) const;

    AirverbsProcessor& processor;
    AirverbsLookAndFeel lnf;
    // Tooltips live in their own desktop window: a child tooltip would be drawn underneath
    // the native OpenGL surface of the 3D view.
    juce::TooltipWindow tooltips { nullptr, 600 };

    juce::OwnedArray<StarPanel> stars;
    std::unique_ptr<ConstellationView> view;

    // top bar
    IconButton prevPreset { IconButton::Icon::Left, "Previous preset" }, nextPreset { IconButton::Icon::Right, "Next preset" },
               savePreset { IconButton::Icon::Save, "Save preset" },
               undo { IconButton::Icon::Undo, "Undo" }, redo { IconButton::Icon::Redo, "Redo" },
               historyButton { IconButton::Icon::History, "Undo history" },
               dice { IconButton::Icon::Dice, "Randomise the whole constellation" },
               midi { IconButton::Icon::Midi, "MIDI learn: right-click any control to learn. Click for mappings." },
               info { IconButton::Icon::Info, "About Airverbs" };
    std::unique_ptr<PresetNameButton> presetName;

    // topology
    juce::OwnedArray<juce::TextButton> topoButtons;
    std::unique_ptr<juce::ParameterAttachment> topoAttachment;
    std::unique_ptr<AirKnob> morph;
    int topology = 0;

    // motion
    std::unique_ptr<AirKnob> orbitRate, orbitDepth, vecX, vecY;
    juce::ComboBox orbitShape;
    std::unique_ptr<juce::ComboBoxParameterAttachment> orbitShapeAttachment;
    juce::TextButton recentre { "Centre" };

    // space
    std::unique_ptr<AirKnob> size, decay, tone, regen;
    std::unique_ptr<GlowToggle> freeze;

    // output
    std::unique_ptr<AirKnob> duck, width, lowCut, highCut, input, mix, output;

    std::unique_ptr<PresetBrowser> browser;
    // The GL view is a native surface that sits above every JUCE child, so while an overlay is
    // open we hide it and paint a frozen frame of it instead.
    juce::Image frozenView;
    int pendingBrowser = -1; // -1 none, else ticks waited for the frozen frame
    bool pendingBrowserSave = false;
    void showBrowserNow();
    float meterL = 0.0f, meterR = 0.0f;
    int snapshotTicks = 0; // AIRVERBS_SNAPSHOT dev aid

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AirverbsEditor)
};

} // namespace airverbs
