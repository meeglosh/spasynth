#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "Theme.h"
#include "Controls.h"   // disableMouseClickFocusGrab
#include "../library/Library.h"   // getLicenseLine

namespace spa::ui
{

// "About SPASynth..." call-out content, opened from the logo settings menu
// (see ContentComponent::showSettingsMenu / showAboutPanel). Read-only, so
// there is nothing here to MIDI Learn and no parameter attachments to worry
// about detaching -- unlike VoicePanel, this panel never touches the APVTS,
// so the editor-close-with-panel-open crash class that fix addressed cannot
// occur here (nothing left dangling once the call-out is deleted). Still
// parented via callOutParent() (never getTopLevelComponent()) for the same
// reason every other pop-over here is: see callOutParent()'s comment.
class AboutPanel : public juce::Component
{
public:
    explicit AboutPanel (juce::AudioProcessor& processorForHostInfo)
        : processor (processorForHostInfo)
    {
        wordmark.setText ("SPASYNTH", juce::dontSendNotification);
        wordmark.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
        wordmark.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (wordmark);

        byline.setText ("by Silverplatter Audio", juce::dontSendNotification);
        byline.setFont (metrics::smallFont());
        byline.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (byline);

        infoLabel.setText (buildInfoBlock(), juce::dontSendNotification);
        infoLabel.setFont (metrics::labelFont());
        infoLabel.setJustificationType (juce::Justification::centredTop);
        infoLabel.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (infoLabel);

        link.setButtonText ("silverplatteraudio.com");
        link.setURL (juce::URL (websiteUrl));
        link.setFont (metrics::smallFont(), false, juce::Justification::centred);
        addAndMakeVisible (link);

        licenseLabel.setText (library::getLicenseLine(), juce::dontSendNotification);
        licenseLabel.setFont (metrics::labelFont());
        licenseLabel.setJustificationType (juce::Justification::centred);
        licenseLabel.setColour (juce::Label::textColourId, currentTheme().textSecondary);
        addAndMakeVisible (licenseLabel);

        copyrightLabel.setText (juce::String (juce::CharPointer_UTF8 ("\xc2\xa9")) + " 2026 Silverplatter Audio. All rights reserved.",
                                 juce::dontSendNotification);
        copyrightLabel.setFont (metrics::labelFont());
        copyrightLabel.setJustificationType (juce::Justification::centred);
        copyrightLabel.setColour (juce::Label::textColourId, currentTheme().textSecondary);
        addAndMakeVisible (copyrightLabel);

        juceLabel.setText ("Built with JUCE " + juce::SystemStats::getJUCEVersion().fromFirstOccurrenceOf ("v", false, false),
                            juce::dontSendNotification);
        juceLabel.setFont (metrics::labelFont());
        juceLabel.setJustificationType (juce::Justification::centred);
        juceLabel.setColour (juce::Label::textColourId, currentTheme().textSecondary);
        addAndMakeVisible (juceLabel);

        copyButton.setButtonText ("Copy Info");
        copyButton.onClick = [this] { copyInfoToClipboard(); };
        addAndMakeVisible (copyButton);

        closeButton.setButtonText ("Close");
        closeButton.onClick = [this] { if (onCloseRequested) onCloseRequested(); };
        addAndMakeVisible (closeButton);

        // This whole panel is read-only display + two buttons; keep it out
        // of the QWERTY focus-grab path like every other pop-over control.
        disableMouseClickFocusGrab (*this);
        // ...except the one control CallOutBox needs a real focus target to
        // grab on open (same reasoning as VoicePanel's ctor comment) --
        // Esc/click-outside dismissal works through CallOutBox's own modal
        // handling regardless of which child holds focus.
        setWantsKeyboardFocus (true);

        setSize (280, 240);
    }

    void paint (juce::Graphics& g) override
    {
        const auto& t = currentTheme();
        g.fillAll (t.panel);
        g.setColour (t.seam);
        g.drawRect (getLocalBounds(), 1);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 10);
        wordmark.setBounds (r.removeFromTop (28));
        byline.setBounds (r.removeFromTop (16));
        r.removeFromTop (8);
        infoLabel.setBounds (r.removeFromTop (56));
        r.removeFromTop (4);
        link.setBounds (r.removeFromTop (20));
        r.removeFromTop (10);
        licenseLabel.setBounds (r.removeFromTop (16));
        copyrightLabel.setBounds (r.removeFromTop (16));
        juceLabel.setBounds (r.removeFromTop (16));
        r.removeFromTop (10);
        auto buttons = r.removeFromTop (26);
        copyButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (4, 0));
        closeButton.setBounds (buttons.reduced (4, 0));
    }

    // Esc is handled by CallOutBox itself (juce_CallOutBox.cpp's own
    // keyPressed override calls inputAttemptWhenModal(), which dismisses),
    // same as clicking outside -- no override needed here for either. Only
    // the Close button needs to actively ask for a dismiss.
    std::function<void()> onCloseRequested;

    // Plain-text block a "Copy Info" click puts on the clipboard, exposed so
    // tests can check its contents without driving the real OS clipboard.
    juce::String buildClipboardText() const
    {
        juce::StringArray lines;
        lines.add ("SPASynth " + versionString());
        lines.add ("Commit: " + commitString());
        lines.add ("Build date: " + juce::String (SPASYNTH_BUILD_DATE));
        lines.add ("Format: " + formatString());
        lines.add ("OS: " + juce::SystemStats::getOperatingSystemName());
        lines.add ("Sample rate: " + juce::String (processor.getSampleRate(), 0) + " Hz");
        lines.add ("Block size: " + juce::String (processor.getBlockSize()));
        const juce::String host (juce::PluginHostType().getHostDescription());
        if (host.isNotEmpty() && host != "Unknown")
            lines.add ("Host: " + host);
        return lines.joinIntoString ("\n");
    }

    juce::String getLinkUrlForTest() const { return link.getURL().toString (false); }
    juce::String getVersionTextForTest() const { return infoLabel.getText(); }

private:
    static juce::String versionString() { return SPASYNTH_VERSION; }
    static juce::String commitString()  { return SPASYNTH_GIT_COMMIT; }

    juce::String formatString() const
    {
        switch (processor.wrapperType)
        {
            case juce::AudioProcessor::wrapperType_AudioUnit:   return "AU";
            case juce::AudioProcessor::wrapperType_VST3:        return "VST3";
            case juce::AudioProcessor::wrapperType_Standalone:  return "Standalone";
            case juce::AudioProcessor::wrapperType_Undefined:
            case juce::AudioProcessor::wrapperType_VST:
            case juce::AudioProcessor::wrapperType_AudioUnitv3:
            case juce::AudioProcessor::wrapperType_AAX:
            case juce::AudioProcessor::wrapperType_Unity:
            case juce::AudioProcessor::wrapperType_LV2:
            default:
                return "Unknown";
        }
    }

    juce::String buildInfoBlock() const
    {
        juce::StringArray lines;
        lines.add ("Version " + versionString());
        lines.add ("Build " + juce::String (SPASYNTH_BUILD_DATE) + " (" + commitString() + ")");
        lines.add ("Format: " + formatString());
        return lines.joinIntoString ("\n");
    }

    void copyInfoToClipboard()
    {
        juce::SystemClipboard::copyTextToClipboard (buildClipboardText());
        copyButton.setButtonText ("Copied");
        juce::Timer::callAfterDelay (900, [safe = juce::Component::SafePointer<juce::TextButton> (&copyButton)]
        {
            if (safe != nullptr)
                safe->setButtonText ("Copy Info");
        });
    }

    static constexpr const char* websiteUrl = "https://silverplatteraudio.com";

    juce::AudioProcessor& processor;
    juce::Label wordmark, byline, infoLabel, licenseLabel, copyrightLabel, juceLabel;
    juce::HyperlinkButton link;
    juce::TextButton copyButton, closeButton;
};

} // namespace spa::ui
