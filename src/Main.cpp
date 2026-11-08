#include "AppController.h"
#include "theme/StilletoLookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace stilleto
{

class StilletoApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }

    // One instance owns the global shortcut; a second launch just shows the first one's window.
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& commandLine) override
    {
        logger.reset (juce::FileLogger::createDefaultAppLogger ("Stilleto", "stilleto.log",
                                                                "Stilleto " + getApplicationVersion(), 512 * 1024));
        juce::Logger::setCurrentLogger (logger.get());

        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        tooltips = std::make_unique<juce::TooltipWindow> (nullptr, 700);

        controller = std::make_unique<AppController>();
        controller->initialise (commandLine.contains ("--minimized"));
    }

    void shutdown() override
    {
        if (controller != nullptr)
            controller->shutdown();
        controller.reset();
        tooltips.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        juce::Logger::setCurrentLogger (nullptr);
        logger.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted (const juce::String&) override
    {
        if (controller != nullptr)
            controller->showMainWindow();
    }

private:
    // Declared first so it outlives every component created by the controller.
    ui::StilletoLookAndFeel lookAndFeel;
    std::unique_ptr<juce::FileLogger> logger;
    std::unique_ptr<juce::TooltipWindow> tooltips;
    std::unique_ptr<AppController> controller;
};

} // namespace stilleto

START_JUCE_APPLICATION (stilleto::StilletoApplication)
