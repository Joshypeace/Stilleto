#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Custom line icons drawn on a 24x24 grid and scaled to any rectangle, so every icon shares the
    same stroke weight and corner style. */
namespace stilleto::ui::Icons
{

enum class Id
{
    Overview, Dictation, History, Models, Settings, Help,
    Mic, Stop, Copy, Insert, Check, Close, Minimise, Search, Trash, Download,
    Folder, Keyboard, Shield, Alert, Refresh, Clock, Words, Chip, Spark, Cursor
};

/** Outline path in 24x24 coordinates. */
juce::Path path (Id id);

/** Strokes the icon fitted into bounds. */
void draw (juce::Graphics& g, Id id, juce::Rectangle<float> bounds, juce::Colour colour, float strokeWidth = 1.6f);

/** The Stilleto mark: a charcoal tile with silver voice bars around a tapered gold blade. */
void drawBrandMark (juce::Graphics& g, juce::Rectangle<float> bounds, float shimmer = 0.0f, bool withTile = true);

/** Renders the brand mark to an image (window icon, tray icon, exported PNG). */
juce::Image renderBrandImage (int size, bool recordingDot = false);

} // namespace stilleto::ui::Icons
