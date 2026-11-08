#include "MainWindow.h"

#include "components/Sidebar.h"
#include "components/TopBar.h"
#include "pages/AboutPage.h"
#include "pages/DictationPage.h"
#include "pages/HistoryPage.h"
#include "pages/ModelsPage.h"
#include "pages/OverviewPage.h"
#include "pages/SettingsPage.h"
#include "theme/Animation.h"
#include "theme/DesignTokens.h"
#include "theme/Icons.h"
#include "windows/NativeWindow.h"

namespace stilleto
{
using namespace ui;

class MainWindow::Content : public juce::Component,
                            private AppController::Listener
{
public:
    explicit Content (MainWindow& w, AppController& c)
        : window (w), controller (c),
          sidebar ({ { Icons::Id::Overview, "Overview" }, { Icons::Id::Dictation, "Dictation" },
                     { Icons::Id::History, "Transcription History" }, { Icons::Id::Models, "Models" },
                     { Icons::Id::Settings, "Settings" }, { Icons::Id::Help, "Help & About" } })
    {
        setOpaque (true);

        pages.add (new OverviewPage (c));
        pages.add (new DictationPage (c));
        pages.add (new HistoryPage (c));
        pages.add (new ModelsPage (c));
        pages.add (new SettingsPage (c));
        pages.add (new AboutPage (c));

        for (auto* p : pages)
        {
            addChildComponent (p);
            p->onNavigate = [this] (AppController::Page page) { showPage (static_cast<int> (page)); };
        }

        addAndMakeVisible (sidebar);
        addAndMakeVisible (topBar);

        sidebar.onSelect = [this] (int index) { showPage (index); };
        topBar.onSettings = [this] { showPage (static_cast<int> (AppController::Page::Settings)); };
        topBar.onMinimise = [this] { controller.mainWindowMinimiseRequested(); };
        topBar.onClose = [this] { controller.mainWindowCloseRequested(); };
        topBar.onToggleMaximise = [this] { window.setFullScreen (! window.isFullScreen()); };
        topBar.onDragStart = [this] (const juce::MouseEvent& e) { if (! window.isFullScreen()) dragger.startDraggingComponent (&window, e.getEventRelativeTo (&window)); };
        topBar.onDrag = [this] (const juce::MouseEvent& e) { if (! window.isFullScreen()) dragger.dragComponent (&window, e.getEventRelativeTo (&window), nullptr); };

        transition.onFrame = [this] (float dt) { return advanceTransition (dt); };

        pages[0]->setVisible (true);
        current = 0;
        controller.addListener (this);
        refreshStatus();
    }

    ~Content() override
    {
        controller.removeListener (this);
    }

    void showPage (int index)
    {
        if (! juce::isPositiveAndBelow (index, pages.size()))
            return;

        sidebar.setSelected (index);
        topBar.setTitle (pages[index]->pageTitle(), pages[index]->pageSubtitle());

        if (index == current)
        {
            pages[index]->pageShown();
            return;
        }

        if (previous >= 0)
            finishTransition();

        previous = current;
        current = index;
        pages[previous]->pageHidden();

        auto* incoming = pages[current];
        incoming->setBounds (pageArea());
        incoming->setVisible (true);
        incoming->pageShown();
        incoming->toFront (false);

        progress = Motion::reduced() ? 1.0f : 0.0f;
        transition.kick();
        advanceTransition (0.0f);
    }

    void playIntro()
    {
        sidebar.playIntro();
        intro = Motion::reduced() ? 1.0f : 0.0f;
        transition.kick();
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (Colours::background.brighter (0.015f), r.getRight(), 0,
                                                 Colours::background, r.getX() + Metrics::sidebarWidth, r.getBottom(), false));
        g.fillAll();

        // A very soft champagne glow in the top-right corner: depth without decoration.
        juce::ColourGradient glow (Colours::gold.withAlpha (0.035f), r.getRight(), 0,
                                   Colours::gold.withAlpha (0.0f), r.getRight() - 520.0f, 360.0f, true);
        g.setGradientFill (glow);
        g.fillRect (r);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        sidebar.setBounds (r.removeFromLeft (Metrics::sidebarWidth));
        topBar.setBounds (r.removeFromTop (Metrics::topBarHeight + 8));
        for (auto* p : pages)
            p->setBounds (pageArea());
    }

private:
    juce::Rectangle<int> pageArea() const
    {
        return getLocalBounds().withTrimmedLeft (Metrics::sidebarWidth).withTrimmedTop (Metrics::topBarHeight + 8);
    }

    bool advanceTransition (float dt)
    {
        bool running = false;

        if (intro < 1.0f)
        {
            intro = juce::jmin (1.0f, intro + dt / 0.6f);
            const float e = Ease::outCubic (intro);
            const int offset = juce::roundToInt ((1.0f - e) * 14.0f);
            for (auto* c : std::initializer_list<juce::Component*> { &topBar, pages[current] })
            {
                c->setAlpha (e);
                c->setTransform (juce::AffineTransform::translation (0.0f, (float) offset));
            }
            running = intro < 1.0f;
        }

        if (previous >= 0)
        {
            progress = juce::jmin (1.0f, progress + dt / 0.26f);
            const float e = Ease::outCubic (progress);

            pages[previous]->setAlpha (1.0f - e);
            pages[previous]->setTransform (juce::AffineTransform::translation (0.0f, -6.0f * e));
            pages[current]->setAlpha (e);
            pages[current]->setTransform (juce::AffineTransform::translation (0.0f, 10.0f * (1.0f - e)));

            if (progress >= 1.0f)
                finishTransition();
            else
                running = true;
        }

        return running;
    }

    void finishTransition()
    {
        if (previous >= 0)
        {
            pages[previous]->setVisible (false);
            pages[previous]->setAlpha (1.0f);
            pages[previous]->setTransform ({});
        }
        pages[current]->setAlpha (1.0f);
        pages[current]->setTransform ({});
        previous = -1;
    }

    void refreshStatus()
    {
        // Microphone
        if (! controller.microphoneAvailable())
            topBar.microphone.setStatus ("No microphone", StatusIndicator::Tone::Bad);
        else if (controller.state() == DictationState::Recording)
            topBar.microphone.setStatus ("Recording", StatusIndicator::Tone::Gold, true);
        else
            topBar.microphone.setStatus ("Mic ready", StatusIndicator::Tone::Good);

        // Engine
        switch (controller.modelStatus())
        {
            case AppController::ModelStatus::Ready:
                if (controller.state() == DictationState::Processing)
                    topBar.engine.setStatus ("Transcribing", StatusIndicator::Tone::Busy, true);
                else
                    topBar.engine.setStatus (controller.modelName(), StatusIndicator::Tone::Good);
                break;
            case AppController::ModelStatus::Loading: topBar.engine.setStatus ("Loading model", StatusIndicator::Tone::Busy, true); break;
            case AppController::ModelStatus::Failed:  topBar.engine.setStatus ("Model error", StatusIndicator::Tone::Bad); break;
            case AppController::ModelStatus::NoModel: topBar.engine.setStatus ("No model", StatusIndicator::Tone::Warning); break;
        }

        sidebar.setShortcutText (controller.hotkeyText(), controller.isHotkeyPaused());
        topBar.setTitle (pages[current]->pageTitle(), pages[current]->pageSubtitle());
        topBar.resized();
    }

    void sessionChanged() override          { refreshStatus(); }
    void modelStatusChanged() override      { refreshStatus(); }
    void hotkeyStatusChanged() override     { refreshStatus(); }
    void microphoneStatusChanged() override { refreshStatus(); }

    MainWindow& window;
    AppController& controller;
    Sidebar sidebar;
    TopBar topBar;
    juce::OwnedArray<Page> pages;
    juce::ComponentDragger dragger;
    FrameDriver transition;
    int current = 0, previous = -1;
    float progress = 1.0f, intro = 1.0f;
};

//==============================================================================================
MainWindow::MainWindow (AppController& c)
    : juce::DocumentWindow ("Stilleto", Colours::background, 0, true), controller (c)
{
    setUsingNativeTitleBar (false);
    setTitleBarHeight (0);
    setResizable (true, false);
    setResizeLimits (980, 620, 8192, 8192);

    content = new Content (*this, controller);
    setContentOwned (content, false);

    // Fit the default size to the work area so the window is never larger than the screen.
    const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    const auto work = display != nullptr ? display->userBounds.toNearestInt() : juce::Rectangle<int> (0, 0, 1366, 728);
    const int width = juce::jlimit (980, 1280, work.getWidth() - 48);
    const int height = juce::jlimit (620, 820, work.getHeight() - 48);

    const auto saved = controller.getSettings().windowState();
    if (saved.isEmpty() || ! restoreWindowStateFromString (saved) || ! work.intersects (getBounds()))
        centreWithSize (width, height);

    native::applyModernFrame (getWindowHandle(), true);
    setIcon (Icons::renderBrandImage (64));
    if (auto* peer = getPeer())
        peer->setIcon (Icons::renderBrandImage (64));
}

MainWindow::~MainWindow() = default;   // AppController::shutdown() saves the window state

void MainWindow::showPage (AppController::Page page)
{
    content->showPage (static_cast<int> (page));
}

void MainWindow::closeButtonPressed()
{
    controller.mainWindowCloseRequested();
}

void MainWindow::minimiseButtonPressed()
{
    controller.mainWindowMinimiseRequested();
}

void MainWindow::visibilityChanged()
{
    DocumentWindow::visibilityChanged();

    if (isVisible())
    {
        native::applyModernFrame (getWindowHandle(), true);
        if (! introPlayed)
        {
            introPlayed = true;
            content->playIntro();
        }
    }
}

} // namespace stilleto
