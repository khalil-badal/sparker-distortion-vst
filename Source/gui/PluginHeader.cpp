#include "PluginHeader.h"
#include "PluginColors.h"

PluginHeader::PluginHeader (juce::AudioProcessorValueTreeState& apvts_, PedalLookAndFeel& laf_)
    : apvts (apvts_), laf (laf_), circuitSwitch (apvts_, laf_)
{
    addAndMakeVisible (circuitSwitch);
    refreshPresetList();
}

void PluginHeader::refreshPresetList()
{
    entries.clear();

    for (auto& p : getFactoryPresets())
    {
        Entry e;
        e.name = p.name;
        e.circuit = p.circuit;
        e.isUserFile = false;
        e.factory = &p;
        entries.push_back (e);
    }

    for (auto& file : PresetManager::listUserPresetFiles())
    {
        Entry e;
        e.name = PresetManager::readPresetName (file);
        e.circuit = -1; // unknown without a full parse; not needed for display
        e.isUserFile = true;
        e.file = file;
        entries.push_back (e);
    }

    currentPresetIndex = juce::jlimit (0, juce::jmax (0, (int) entries.size() - 1), currentPresetIndex);
    repaint();
}

void PluginHeader::resized()
{
    // More margin from the right edge — badge should sit further left.
    // The AMP/DRIVE voicing switch briefly lived here, immediately left of
    // the badge; it crowded the corner and now sits in the bottom right,
    // below the mode/aggression panel (see SuhrRiotEditor::resized).
    auto rightArea = getLocalBounds();
    circuitSwitch.setBounds (rightArea.removeFromRight (150).reduced (45, 8));

    // LCD is centered on the FULL header width (not "whatever's left after
    // removing the right-side badge"), so it reads as centered regardless
    // of the left brand text's width — that was the bug causing it to sit
    // too far left.
    constexpr int kLcdW = 340;
    const int lcdX = (getWidth() - kLcdW) / 2;
    lcdBounds = juce::Rectangle<int> (lcdX, 6, kLcdW, getHeight() - 12);
    prevArrowBounds = lcdBounds.removeFromLeft (24);
    nextArrowBounds = lcdBounds.removeFromRight (24);
    lcdCenterBounds = lcdBounds;
}

void PluginHeader::paint (juce::Graphics& g)
{
    // Header bar background (mockup: #08060d @90%, border-bottom #1f1730)
    g.setColour (juce::Colour (PluginColors::kHeaderBg));
    g.fillRect (getLocalBounds());
    g.setColour (juce::Colour (PluginColors::kHeaderBorder));
    g.fillRect (getLocalBounds().removeFromBottom (1));

    // Left: SPARKER DISTORTION branding
    {
        auto leftArea = getLocalBounds().withWidth (getWidth() / 3).reduced (40, 0);
        g.setFont (laf.uiFont (12.0f, true));
        g.setColour (juce::Colours::white);
        g.drawText ("SPARKER DISTORTION", leftArea, juce::Justification::centredLeft, false);
    }

    const auto lcdFullBounds = lcdBounds.toFloat().withLeft ((float) prevArrowBounds.getX())
                                                   .withRight ((float) nextArrowBounds.getRight());

    // Center: LCD preset stepper
    {
        g.setColour (juce::Colour (PluginColors::kLcdBg));
        g.fillRoundedRectangle (lcdFullBounds, 10.0f);
        g.setColour (juce::Colour (PluginColors::kLcdBorder));
        g.drawRoundedRectangle (lcdFullBounds, 10.0f, 1.0f);

        g.setColour (juce::Colour (0xff94a3b8));
        auto drawChevron = [&g] (juce::Rectangle<int> area, bool pointLeft)
        {
            const float cx = (float) area.getCentreX();
            const float cy = (float) area.getCentreY();
            const float s = 4.0f;
            juce::Path p;
            if (pointLeft) { p.startNewSubPath (cx + s, cy - s); p.lineTo (cx - s, cy); p.lineTo (cx + s, cy + s); }
            else            { p.startNewSubPath (cx - s, cy - s); p.lineTo (cx + s, cy); p.lineTo (cx - s, cy + s); }
            g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        drawChevron (prevArrowBounds, true);
        drawChevron (nextArrowBounds, false);

        if (! entries.empty())
        {
            const auto& entry = entries[(size_t) currentPresetIndex];
            g.setFont (laf.uiFont (11.0f, true));
            g.setColour (juce::Colours::white);
            g.drawText (entry.name, lcdCenterBounds, juce::Justification::centred, true);
        }
        else
        {
            g.setFont (laf.uiFont (10.0f, false));
            g.setColour (juce::Colour (0xff64748b));
            g.drawText ("No presets", lcdCenterBounds, juce::Justification::centred, true);
        }
    }

    // Dropdown browser overlay
    if (isDropdownOpen)
    {
        auto dropBounds = juce::Rectangle<int> (prevArrowBounds.getX(), lcdBounds.getBottom() + 6,
                                                  nextArrowBounds.getRight() - prevArrowBounds.getX(),
                                                  juce::jmin (240, (int) entries.size() * 22 + 8));
        g.setColour (juce::Colour (0xff0c0817));
        g.fillRoundedRectangle (dropBounds.toFloat(), 10.0f);
        g.setColour (juce::Colour (0xff2d2047));
        g.drawRoundedRectangle (dropBounds.toFloat(), 10.0f, 1.0f);

        for (size_t i = 0; i < entries.size(); ++i)
        {
            const auto rowBounds = juce::Rectangle<int> (dropBounds.getX() + 4, dropBounds.getY() + 4 + (int) i * 22,
                                                           dropBounds.getWidth() - 8, 20);
            if (rowBounds.getBottom() > dropBounds.getBottom() - 4)
                break;

            const bool selected = ((int) i == currentPresetIndex);
            if (selected)
            {
                g.setColour (juce::Colour (0xff1e1436));
                g.fillRoundedRectangle (rowBounds.toFloat(), 6.0f);
                g.setColour (juce::Colour (0xffa855f7).withAlpha (0.4f));
                g.drawRoundedRectangle (rowBounds.toFloat(), 6.0f, 1.0f);
            }
            g.setFont (laf.uiFont (10.5f, true));
            g.setColour (selected ? juce::Colour (0xffe9d5ff) : juce::Colour (0xffcbd5e1));
            g.drawText ((entries[i].isUserFile ? juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85 ")) : juce::String())
                        + entries[i].name,
                        rowBounds.reduced (8, 0), juce::Justification::centredLeft, true);
        }
    }
}

void PluginHeader::mouseDown (const juce::MouseEvent& e)
{
    const auto pos = e.getPosition();

    // Note: the "+" save-preset button was removed from the header per
    // explicit request. promptAndSaveNewPreset() (and PresetManager's real
    // save/load) still exist and work — there's just no UI trigger for
    // saving right now. Tell me where you'd like it (e.g. a right-click
    // menu on the LCD, or back in the header in a different spot) and I'll
    // wire a new trigger to the same function.

    if (isDropdownOpen)
    {
        auto dropBounds = juce::Rectangle<int> (prevArrowBounds.getX(), lcdBounds.getBottom() + 6,
                                                  nextArrowBounds.getRight() - prevArrowBounds.getX(),
                                                  juce::jmin (240, (int) entries.size() * 22 + 8));
        if (dropBounds.contains (pos))
        {
            const int idx = (pos.y - dropBounds.getY() - 4) / 22;
            if (idx >= 0 && idx < (int) entries.size())
                selectPreset (idx);
            isDropdownOpen = false;
            repaint();
            return;
        }

        isDropdownOpen = false;
        repaint();
        return;
    }

    if (entries.empty())
        return;

    if (prevArrowBounds.contains (pos))
    {
        selectPreset ((currentPresetIndex - 1 + (int) entries.size()) % (int) entries.size());
        return;
    }
    if (nextArrowBounds.contains (pos))
    {
        selectPreset ((currentPresetIndex + 1) % (int) entries.size());
        return;
    }
    if (lcdCenterBounds.contains (pos))
    {
        isDropdownOpen = true;
        repaint();
        return;
    }
}

void PluginHeader::selectPreset (int index)
{
    currentPresetIndex = index;
    if (onSelectPreset && ! entries.empty())
        onSelectPreset (entries[(size_t) index]);
    repaint();
}

void PluginHeader::promptAndSaveNewPreset()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save Preset",
                                                       "Enter a name for the current settings:",
                                                       juce::MessageBoxIconType::NoIcon);
    saveDialog->addTextEditor ("name", "My Preset");
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create (
        [this] (int result)
        {
            if (result == 1 && saveDialog != nullptr)
            {
                const auto name = saveDialog->getTextEditorContents ("name");
                if (name.isNotEmpty())
                {
                    PresetManager::savePreset (apvts, name);
                    refreshPresetList();
                }
            }
            saveDialog.reset();
        }));
}
