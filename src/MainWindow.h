#pragma once

#include "AppController.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace stilleto
{

/** The dashboard window: custom dark chrome (rounded on Windows 11), sidebar navigation, top bar
    and animated page transitions. */
class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow (AppController& controller);
    ~MainWindow() override;

    void showPage (AppController::Page page);

    void closeButtonPressed() override;
    void minimiseButtonPressed() override;
    void visibilityChanged() override;

private:
    class Content;

    AppController& controller;
    Content* content = nullptr;   // owned by DocumentWindow (setContentOwned)
    bool introPlayed = false;
};

} // namespace stilleto
