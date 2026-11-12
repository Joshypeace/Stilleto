#include "AppController.h"
#include "theme/Icons.h"
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
    bool moreThanOneInstanceAllowed() override
    {
        const auto args = getCommandLineParameters();   // developer render modes only
        return args.contains ("--snapshot") || args.contains ("--export-icon");
    }

    void initialise (const juce::String& commandLine) override
    {
        // The speech engine (ggml) is compiled for AVX2. Without it the first inference would crash
        // with an illegal-instruction fault, so explain instead of failing silently on old CPUs.
        if (! juce::SystemStats::hasAVX2())
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Stilleto cannot run on this PC",
                "Stilleto's offline speech engine needs a processor with AVX2 support "
                "(Intel 4th generation / AMD Ryzen or newer). This processor does not provide it.",
                nullptr, juce::ModalCallbackFunction::create ([] (int) { juce::JUCEApplication::quit(); }));
            return;
        }

        logger.reset (juce::FileLogger::createDefaultAppLogger ("Stilleto", "stilleto.log",
                                                                "Stilleto " + getApplicationVersion(), 512 * 1024));
        juce::Logger::setCurrentLogger (logger.get());

        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
        tooltips = std::make_unique<juce::TooltipWindow> (nullptr, 700);

        if (commandLine.contains ("--export-icon"))
        {
            // Developer aid: renders the vector brand mark to the PNG that CMake embeds as the exe icon.
            const juce::File out (commandLine.fromFirstOccurrenceOf ("--export-icon", false, false).trim().unquoted());
            out.getParentDirectory().createDirectory();
            out.deleteFile();
            if (auto stream = out.createOutputStream())
                juce::PNGImageFormat().writeImageToStream (ui::Icons::renderBrandImage (512), *stream);
            quit();
            return;
        }

        controller = std::make_unique<AppController>();

        if (commandLine.contains ("--snapshot"))
        {
            const auto dir = commandLine.fromFirstOccurrenceOf ("--snapshot", false, false).trim().unquoted();
            controller->renderSnapshots (juce::File::isAbsolutePath (dir) ? juce::File (dir)
                                                                         : juce::File::getCurrentWorkingDirectory().getChildFile ("snapshots"),
                                         [] { juce::JUCEApplication::quit(); });
            return;
        }

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
