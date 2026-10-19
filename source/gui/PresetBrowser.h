#pragma once

#include "Widgets.h"

namespace airverbs
{

class PresetManager;

/** Full preset browser overlay: search, categories, favourites, audition-on-click, save / rename / delete / import / export. */
class PresetBrowser : public juce::Component,
                      private juce::TextEditor::Listener
{
public:
    explicit PresetBrowser (AirverbsProcessor&);
    ~PresetBrowser() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void showSaveDialog();
    bool keyPressed (const juce::KeyPress&) override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void()> onClose;

private:
    class CategoryModel;
    class PresetModel;
    class SaveDialog;

    void textEditorTextChanged (juce::TextEditor&) override { rebuildFilter(); }
    void rebuildFilter();
    void selectedPresetChanged (int filteredRow, bool load);
    int selectedIndex() const;

    AirverbsProcessor& processor;
    PresetManager& presets;
    juce::TextEditor search;
    juce::ListBox categoryList, presetList;
    std::unique_ptr<CategoryModel> categoryModel;
    std::unique_ptr<PresetModel> presetModel;
    juce::TextButton saveButton { "Save As..." }, renameButton { "Rename" }, deleteButton { "Delete" },
                     importButton { "Import..." }, exportButton { "Export..." }, folderButton { "Open Folder" },
                     initButton { "Init" }, closeButton { "Close" };
    std::unique_ptr<SaveDialog> saveDialog;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::StringArray categories;
    juce::String currentCategory = "All";
    std::vector<int> filtered;
    juce::Rectangle<float> panelBounds;
};

} // namespace airverbs
