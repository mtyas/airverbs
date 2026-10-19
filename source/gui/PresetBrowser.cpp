#include "PresetBrowser.h"
#include "../PluginProcessor.h"

namespace airverbs
{

static const juce::String favouritesLabel = "Favourites";

// ------------------------------------------------------------------------------------- category list
class PresetBrowser::CategoryModel : public juce::ListBoxModel
{
public:
    explicit CategoryModel (PresetBrowser& b) : owner (b) {}
    int getNumRows() override { return owner.categories.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        const auto name = owner.categories[row];
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (4, 1);
        if (selected)
        {
            g.setColour (palette::panelHi);
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (palette::accent);
            g.fillRoundedRectangle (r.withWidth (3.0f), 1.5f);
        }
        g.setFont (AirverbsLookAndFeel::font ((float) h * 0.48f, selected));
        g.setColour (selected ? palette::text : palette::textDim);
        g.drawText (name == favouritesLabel ? juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 ")) + name : name,
                    r.withTrimmedLeft (10), juce::Justification::centredLeft);
    }
    void selectedRowsChanged (int row) override
    {
        if (row >= 0) { owner.currentCategory = owner.categories[row]; owner.rebuildFilter(); }
    }
private:
    PresetBrowser& owner;
};

// ------------------------------------------------------------------------------------- preset list
class PresetBrowser::PresetModel : public juce::ListBoxModel
{
public:
    explicit PresetModel (PresetBrowser& b) : owner (b) {}
    int getNumRows() override { return (int) owner.filtered.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (row < 0 || row >= (int) owner.filtered.size()) return;
        const int idx = owner.filtered[(size_t) row];
        const auto& p = owner.presets.getAll()[(size_t) idx];
        const bool current = idx == owner.presets.getCurrentIndex();
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (4, 1);
        if (selected || current)
        {
            g.setColour (selected ? palette::panelHi : palette::panelHi.withAlpha (0.5f));
            g.fillRoundedRectangle (r, 4.0f);
        }
        if (current)
        {
            g.setColour (palette::accent);
            g.fillRoundedRectangle (r.withWidth (3.0f), 1.5f);
        }
        // favourite star
        auto starArea = r.removeFromLeft ((float) h).reduced ((float) h * 0.25f);
        juce::Path star;
        star.addStar (starArea.getCentre(), 5, starArea.getWidth() * 0.22f, starArea.getWidth() * 0.5f);
        if (owner.presets.isFavourite (p)) { g.setColour (juce::Colour (0xffffd166)); g.fillPath (star); }
        else { g.setColour (palette::textFaint); g.strokePath (star, juce::PathStrokeType (1.0f)); }

        g.setFont (AirverbsLookAndFeel::font ((float) h * 0.5f, current));
        g.setColour (current ? palette::accent : palette::text);
        g.drawText (p.name, r.withTrimmedRight (170), juce::Justification::centredLeft);
        g.setFont (AirverbsLookAndFeel::font ((float) h * 0.38f));
        g.setColour (palette::textDim);
        g.drawText (p.category, r.removeFromRight (165).withTrimmedRight (60), juce::Justification::centredRight);
        g.setColour (p.factory ? palette::textFaint : palette::star (3).withAlpha (0.8f));
        g.drawText (p.factory ? "FACTORY" : "USER", r.removeFromRight (58), juce::Justification::centredRight);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        if (row < 0 || row >= (int) owner.filtered.size()) return;
        const int idx = owner.filtered[(size_t) row];
        if (e.x < owner.presetList.getRowHeight() + 4)
        {
            const auto& p = owner.presets.getAll()[(size_t) idx];
            owner.presets.setFavourite (p, ! owner.presets.isFavourite (p));
            owner.presetList.repaint();
            return;
        }
        owner.selectedPresetChanged (row, true);
    }
    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override { if (owner.onClose) owner.onClose(); }
    void selectedRowsChanged (int row) override { owner.selectedPresetChanged (row, false); }
    void returnKeyPressed (int row) override { owner.selectedPresetChanged (row, true); }

private:
    PresetBrowser& owner;
};

// ------------------------------------------------------------------------------------- save dialog
class PresetBrowser::SaveDialog : public juce::Component
{
public:
    explicit SaveDialog (PresetBrowser& b) : owner (b)
    {
        for (auto* l : { &nameLabel, &catLabel, &descLabel })
        {
            l->setFont (AirverbsLookAndFeel::font (12.0f, true));
            l->setColour (juce::Label::textColourId, palette::textDim);
            addAndMakeVisible (*l);
        }
        name.setText (owner.presets.getCurrentName() == "Init" ? juce::String ("My Constellation") : owner.presets.getCurrentName());
        name.setSelectAllWhenFocused (true);
        category.setEditableText (true);
        for (auto& c : owner.presets.getCategories())
            if (c != favouritesLabel) category.addItem (c, category.getNumItems() + 1);
        const int cur = owner.presets.getCurrentIndex();
        category.setText (cur >= 0 && ! owner.presets.getAll()[(size_t) cur].factory ? owner.presets.getAll()[(size_t) cur].category : juce::String ("User"));
        description.setMultiLine (true);
        description.setReturnKeyStartsNewLine (true);
        for (auto* c : std::initializer_list<juce::Component*> { &name, &category, &description, &save, &cancel })
            addAndMakeVisible (c);
        save.onClick = [this] { commit(); };
        cancel.onClick = [this] { owner.saveDialog.reset(); };
        name.onReturnKey = [this] { commit(); };
    }
    void visibilityChanged() override { if (isShowing()) name.grabKeyboardFocus(); }
    void parentHierarchyChanged() override { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<juce::TextEditor> (&name)] { if (safe != nullptr && safe->isShowing()) safe->grabKeyboardFocus(); }); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (palette::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (palette::accent.withAlpha (0.5f));
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        drawSectionTitle (g, { 18, 12, r.getWidth() - 36, 18 }, "SAVE PRESET", palette::accent);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (18).withTrimmedTop (28);
        auto row = [&] (juce::Label& l, juce::Component& c, int h)
        {
            l.setBounds (r.removeFromTop (18));
            c.setBounds (r.removeFromTop (h));
            r.removeFromTop (8);
        };
        row (nameLabel, name, 30);
        row (catLabel, category, 30);
        row (descLabel, description, 60);
        auto buttons = r.removeFromBottom (32);
        save.setBounds (buttons.removeFromRight (110));
        buttons.removeFromRight (8);
        cancel.setBounds (buttons.removeFromRight (100));
    }
    void commit()
    {
        if (name.getText().trim().isEmpty()) { name.grabKeyboardFocus(); return; }
        auto& pm = owner.presets;
        auto target = PresetManager::getUserDirectory()
                          .getChildFile (juce::File::createLegalFileName (category.getText().trim().isEmpty() ? juce::String ("User") : category.getText().trim()))
                          .getChildFile (juce::File::createLegalFileName (name.getText().trim()) + PresetManager::extension);
        auto doSave = [safeOwner = juce::Component::SafePointer<PresetBrowser> (&owner), n = name.getText(), c = category.getText(), d = description.getText()]
        {
            if (safeOwner == nullptr) return;
            safeOwner->presets.saveUser (n, c, d);
            safeOwner->saveDialog.reset();
            safeOwner->refresh();
        };
        if (target.existsAsFile())
        {
            juce::NativeMessageBox::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::QuestionIcon)
                                                   .withTitle ("Overwrite preset?").withMessage ("\"" + name.getText() + "\" already exists. Replace it?")
                                                   .withButton ("Replace").withButton ("Cancel").withAssociatedComponent (this),
                                               [doSave] (int result) { if (result == 0) doSave(); });
            return;
        }
        juce::ignoreUnused (pm);
        doSave();
    }

private:
    PresetBrowser& owner;
    juce::Label nameLabel { {}, "NAME" }, catLabel { {}, "CATEGORY" }, descLabel { {}, "NOTES" };
    juce::TextEditor name, description;
    juce::ComboBox category;
    juce::TextButton save { "Save" }, cancel { "Cancel" };
};

// ------------------------------------------------------------------------------------- browser
PresetBrowser::PresetBrowser (AirverbsProcessor& p) : processor (p), presets (p.presets)
{
    setWantsKeyboardFocus (true);
    categoryModel = std::make_unique<CategoryModel> (*this);
    presetModel = std::make_unique<PresetModel> (*this);
    categoryList.setModel (categoryModel.get());
    presetList.setModel (presetModel.get());
    categoryList.setRowHeight (28);
    presetList.setRowHeight (28);
    search.setTextToShowWhenEmpty ("Search presets, engines, categories...", palette::textFaint);
    search.addListener (this);
    search.setFont (AirverbsLookAndFeel::font (15.0f));
    search.setIndents (10, 6);
    for (auto* c : std::initializer_list<juce::Component*> { &search, &categoryList, &presetList, &saveButton, &renameButton, &deleteButton,
                                                             &importButton, &exportButton, &folderButton, &initButton, &closeButton })
        addAndMakeVisible (c);

    saveButton.onClick = [this] { showSaveDialog(); };
    closeButton.onClick = [this] { if (onClose) onClose(); };
    initButton.onClick = [this] { presets.loadInit(); refresh(); };
    folderButton.onClick = [] { PresetManager::getUserDirectory().revealToUser(); };
    deleteButton.onClick = [this]
    {
        const int idx = selectedIndex();
        if (idx < 0 || presets.getAll()[(size_t) idx].factory) return;
        juce::Component::SafePointer<PresetBrowser> safe (this);
        juce::NativeMessageBox::showAsync (juce::MessageBoxOptions().withIconType (juce::MessageBoxIconType::WarningIcon)
                                               .withTitle ("Delete preset?").withMessage ("Move \"" + presets.getAll()[(size_t) idx].name + "\" to the trash?")
                                               .withButton ("Delete").withButton ("Cancel").withAssociatedComponent (this),
                                           [safe, idx] (int result) { if (safe != nullptr && result == 0) { safe->presets.deleteUser (idx); safe->refresh(); } });
    };
    renameButton.onClick = [this]
    {
        const int idx = selectedIndex();
        if (idx < 0 || presets.getAll()[(size_t) idx].factory) return;
        auto* aw = new juce::AlertWindow ("Rename preset", {}, juce::MessageBoxIconType::NoIcon, this);
        aw->addTextEditor ("name", presets.getAll()[(size_t) idx].name);
        aw->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<PresetBrowser> safe (this);
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe, aw, idx] (int r)
        {
            if (safe != nullptr && r == 1) { safe->presets.renameUser (idx, aw->getTextEditorContents ("name")); safe->refresh(); }
        }), true);
    };
    importButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Import Airverbs presets", juce::File::getSpecialLocation (juce::File::userHomeDirectory), "*.airverb");
        juce::Component::SafePointer<PresetBrowser> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                              [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            for (auto& f : fc.getResults()) safe->presets.importFile (f);
            safe->refresh();
        });
    };
    exportButton.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> ("Export current preset",
                                                       juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile (presets.getCurrentName() + ".airverb"), "*.airverb");
        juce::Component::SafePointer<PresetBrowser> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe] (const juce::FileChooser& fc)
        {
            if (safe != nullptr && fc.getResult() != juce::File()) safe->presets.exportCurrent (fc.getResult());
        });
    };
    refresh();
}

PresetBrowser::~PresetBrowser()
{
    categoryList.setModel (nullptr);
    presetList.setModel (nullptr);
}

void PresetBrowser::refresh()
{
    categories.clear();
    categories.add ("All");
    categories.add (favouritesLabel);
    for (auto& c : presets.getCategories()) categories.addIfNotAlreadyThere (c);
    categoryList.updateContent();
    const int catRow = juce::jmax (0, categories.indexOf (currentCategory));
    categoryList.selectRow (catRow, true, true);
    currentCategory = categories[catRow];
    rebuildFilter();
}

void PresetBrowser::rebuildFilter()
{
    filtered.clear();
    const auto query = search.getText().trim().toLowerCase();
    const auto& all = presets.getAll();
    for (size_t i = 0; i < all.size(); ++i)
    {
        const auto& p = all[i];
        if (currentCategory == favouritesLabel && ! presets.isFavourite (p)) continue;
        if (currentCategory != "All" && currentCategory != favouritesLabel && p.category != currentCategory) continue;
        if (query.isNotEmpty())
        {
            juce::String hay = (p.name + " " + p.category + " " + p.description + " " + p.author).toLowerCase();
            for (auto& [id, v] : p.values)
                if (id.endsWith ("_engine"))
                    hay << " " << juce::String (EngineRegistry::get().info ((int) v).name).toLowerCase();
            bool all_ = true;
            for (auto& word : juce::StringArray::fromTokens (query, " ", ""))
                all_ &= hay.contains (word);
            if (! all_) continue;
        }
        filtered.push_back ((int) i);
    }
    presetList.updateContent();
    presetList.deselectAllRows();
    for (size_t r = 0; r < filtered.size(); ++r)
        if (filtered[r] == presets.getCurrentIndex())
        {
            presetList.selectRow ((int) r, false, true);
            presetList.scrollToEnsureRowIsOnscreen ((int) r);
        }
    presetList.repaint();
    repaint();
}

int PresetBrowser::selectedIndex() const
{
    const int row = presetList.getSelectedRow();
    return row >= 0 && row < (int) filtered.size() ? filtered[(size_t) row] : -1;
}

void PresetBrowser::selectedPresetChanged (int row, bool load)
{
    if (row >= 0 && row < (int) filtered.size() && load && filtered[(size_t) row] != presets.getCurrentIndex())
        presets.load (filtered[(size_t) row]);
    const int idx = selectedIndex();
    const bool user = idx >= 0 && ! presets.getAll()[(size_t) idx].factory;
    renameButton.setEnabled (user);
    deleteButton.setEnabled (user);
    presetList.repaint();
    repaint();
}

void PresetBrowser::showSaveDialog()
{
    saveDialog = std::make_unique<SaveDialog> (*this);
    addAndMakeVisible (*saveDialog);
    resized();
}

bool PresetBrowser::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey)
    {
        if (saveDialog != nullptr) saveDialog.reset();
        else if (onClose) onClose();
        return true;
    }
    return false;
}

void PresetBrowser::mouseDown (const juce::MouseEvent& e)
{
    if (! panelBounds.contains (e.position) && saveDialog == nullptr && onClose) onClose();
}

void PresetBrowser::resized()
{
    const float s = (float) getWidth() / 1200.0f;
    panelBounds = juce::Rectangle<float> (150 * s, 70 * s, 900 * s, 620 * s);
    auto r = panelBounds.reduced (20 * s).toNearestInt();
    r.removeFromTop (juce::roundToInt (30 * s));
    auto top = r.removeFromTop (juce::roundToInt (36 * s));
    closeButton.setBounds (top.removeFromRight (juce::roundToInt (90 * s)));
    top.removeFromRight (juce::roundToInt (10 * s));
    search.setBounds (top);
    r.removeFromTop (juce::roundToInt (12 * s));
    auto bottom = r.removeFromBottom (juce::roundToInt (34 * s));
    const int bw = juce::roundToInt (100 * s), gap = juce::roundToInt (8 * s);
    for (auto* b : { &saveButton, &renameButton, &deleteButton, &initButton })
    {
        b->setBounds (bottom.removeFromLeft (bw));
        bottom.removeFromLeft (gap);
    }
    for (auto* b : { &folderButton, &exportButton, &importButton })
    {
        b->setBounds (bottom.removeFromRight (bw + juce::roundToInt (10 * s)));
        bottom.removeFromRight (gap);
    }
    r.removeFromBottom (juce::roundToInt (12 * s));
    categoryList.setBounds (r.removeFromLeft (juce::roundToInt (190 * s)));
    r.removeFromLeft (juce::roundToInt (12 * s));
    auto info = r.removeFromBottom (juce::roundToInt (78 * s));
    juce::ignoreUnused (info);
    presetList.setBounds (r);
    categoryList.setRowHeight (juce::roundToInt (28 * s));
    presetList.setRowHeight (juce::roundToInt (28 * s));
    if (saveDialog != nullptr)
        saveDialog->setBounds (juce::Rectangle<int> (420, 330).withCentre (panelBounds.getCentre().toInt()));
}

void PresetBrowser::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / 1200.0f;
    g.fillAll (juce::Colours::black.withAlpha (0.55f));
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (panelBounds.translated (0, 6 * s).expanded (4 * s), 12 * s);
    juce::ColourGradient grad (juce::Colour (0xff15171c), 0, panelBounds.getY(), juce::Colour (0xff0d0e12), 0, panelBounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (panelBounds, 10 * s);
    g.setColour (palette::line);
    g.drawRoundedRectangle (panelBounds.reduced (0.5f), 10 * s, 1.0f);
    drawSectionTitle (g, { panelBounds.getX() + 20 * s, panelBounds.getY() + 14 * s, panelBounds.getWidth() - 40 * s, 20 * s }, "PRESETS", palette::accent);

    // info block for the selected preset
    const int idx = selectedIndex();
    auto info = presetList.getBounds().toFloat().withY ((float) presetList.getBottom() + 6 * s).withHeight (72 * s);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.fillRoundedRectangle (info, 6 * s);
    if (idx >= 0)
    {
        const auto& p = presets.getAll()[(size_t) idx];
        auto ir = info.reduced (12 * s, 8 * s);
        g.setFont (AirverbsLookAndFeel::font (17 * s, true));
        g.setColour (palette::text);
        g.drawText (p.name, ir.removeFromTop (22 * s), juce::Justification::centredLeft);
        juce::String engines;
        for (auto& [id, v] : p.values)
            if (id.endsWith ("_engine")) engines << (engines.isEmpty() ? "" : " / ") << EngineRegistry::get().info ((int) v).name;
        g.setFont (AirverbsLookAndFeel::font (12 * s));
        g.setColour (palette::textDim);
        g.drawText (p.category + (p.author.isNotEmpty() ? "  by " + p.author : juce::String()) + (engines.isNotEmpty() ? "   |   " + engines : juce::String()),
                    ir.removeFromTop (18 * s), juce::Justification::centredLeft);
        g.setColour (palette::text.withAlpha (0.8f));
        g.drawFittedText (p.description, ir.toNearestInt(), juce::Justification::topLeft, 2);
    }
    if (filtered.empty())
    {
        g.setFont (AirverbsLookAndFeel::font (14 * s));
        g.setColour (palette::textFaint);
        g.drawText ("No presets match.", presetList.getBounds().toFloat(), juce::Justification::centred);
    }
}

} // namespace airverbs
