#include "pages/AboutPage.h"

namespace stilleto
{
using namespace ui;

AboutPage::AboutPage (AppController& c) : Page (c)
{
    for (auto* card : { &brandCard, &stepsCard, &limitsCard, &diagnosticsCard })
    {
        addAndMakeVisible (card);
        card->setInterceptsMouseClicks (false, true);
    }
    diagnosticsCard.addAndMakeVisible (dataFolder);
    diagnosticsCard.addAndMakeVisible (logFile);

    dataFolder.onClick = [] { SettingsManager::appDataDirectory().startAsProcess(); };
    logFile.onClick = []
    {
        if (auto* logger = dynamic_cast<juce::FileLogger*> (juce::Logger::getCurrentLogger()))
            logger->getLogFile().startAsProcess();
    };

    systemInfo = juce::String (WhisperEngine::systemInfo()).trim();
}

void AboutPage::resized()
{
    auto r = getLocalBounds().reduced (Metrics::pagePadding, Metrics::pagePadding - 8);
    auto left = r.removeFromLeft (r.getWidth() * 55 / 100);
    r.removeFromLeft (Metrics::gap);

    brandCard.setBounds (left.removeFromTop (170));
    left.removeFromTop (Metrics::gap);
    stepsCard.setBounds (left);

    limitsCard.setBounds (r.removeFromTop (r.getHeight() * 58 / 100));
    r.removeFromTop (Metrics::gap);
    diagnosticsCard.setBounds (r);

    auto buttons = diagnosticsCard.getLocalBounds().reduced (Metrics::cardPadding).removeFromBottom (36);
    dataFolder.setBounds (buttons.removeFromLeft (dataFolder.getIdealWidth()));
    buttons.removeFromLeft (8);
    logFile.setBounds (buttons.removeFromLeft (logFile.getIdealWidth()));
}

void AboutPage::paintOverChildren (juce::Graphics& g)
{
    // Brand
    {
        auto r = brandCard.getBounds().reduced (28);
        Icons::drawBrandMark (g, r.removeFromLeft (96).toFloat().withSizeKeepingCentre (96, 96));
        r.removeFromLeft (24);
        g.setColour (Colours::text);
        g.setFont (Fonts::make (34.0f, "Light"));
        g.drawText ("Stilleto", r.removeFromTop (r.getHeight() / 2).withTrimmedTop (8), juce::Justification::bottomLeft);
        g.setColour (Colours::gold);
        g.setFont (Fonts::body());
        g.drawText ("Your voice. Anywhere you type.", r.removeFromTop (24), juce::Justification::centredLeft);
        g.setColour (Colours::textMuted);
        g.setFont (Fonts::small());
        g.drawText ("Offline speech recognition by whisper.cpp  ·  Built with JUCE", r.removeFromTop (22), juce::Justification::centredLeft);
    }

    auto paragraphList = [&g] (juce::Rectangle<int> r, const juce::StringArray& items, bool numbered)
    {
        for (int i = 0; i < items.size(); ++i)
        {
            const int lines = items[i].length() > 70 ? 2 : 1;
            auto row = r.removeFromTop (lines == 2 ? 44 : 28);
            auto marker = row.removeFromLeft (28);

            if (numbered)
            {
                const auto dot = marker.toFloat().withSizeKeepingCentre (20, 20).withY ((float) row.getY() + 2.0f);
                g.setColour (Colours::gold.withAlpha (0.14f));
                g.fillEllipse (dot);
                g.setColour (Colours::gold);
                g.setFont (Fonts::caption());
                g.drawText (juce::String (i + 1), dot, juce::Justification::centred);
            }
            else
            {
                g.setColour (Colours::silver.withAlpha (0.6f));
                g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ (float) marker.getX() + 8.0f, (float) row.getY() + 11.0f }));
            }

            g.setColour (Colours::textSecondary);
            g.setFont (Fonts::small());
            g.drawFittedText (items[i], row, juce::Justification::topLeft, lines);
        }
    };

    paragraphList ((stepsCard.getBounds().reduced (Metrics::cardPadding)).withTrimmedTop (36),
                   { "Click into the text field of any application: Notepad, Word, a browser, chat or an editor.",
                     "Press " + controller.hotkeyText() + ". The floating widget shows that Stilleto is listening.",
                     "Speak naturally, then press the shortcut again to stop.",
                     "Whisper transcribes the audio locally on this PC, typically in a second or two.",
                     "Stilleto pastes the text at your cursor and restores your clipboard afterwards." }, true);

    paragraphList ((limitsCard.getBounds().reduced (Metrics::cardPadding)).withTrimmedTop (36),
                   { "Windows does not let normal applications type into programs running as administrator. Run Stilleto as administrator too if you need that.",
                     "Password fields, secure desktops (UAC prompts) and some games or remote sessions ignore simulated input.",
                     "Transcription starts after you stop recording; live word-by-word transcription is not available yet.",
                     "Speech is processed on this PC. Only model downloads, when you ask for them, use the internet." }, false);

    {
        auto r = diagnosticsCard.getBounds().reduced (Metrics::cardPadding).withTrimmedTop (34).withTrimmedBottom (44);
        g.setColour (Colours::textMuted);
        g.setFont (Fonts::mono (11.0f));
        juce::String info;
        info << "Model: " << controller.modelName() << "\n"
             << "Microphone: " << controller.microphoneName() << "\n"
             << "Data: " << SettingsManager::appDataDirectory().getFullPathName() << "\n"
             << "whisper.cpp: " << systemInfo;
        g.drawFittedText (info, r, juce::Justification::topLeft, juce::jmax (1, r.getHeight() / 15), 1.0f);
    }
}

} // namespace stilleto
