#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Stilleto design tokens: one source for colour, type, spacing and radii. */
namespace stilleto::ui
{

namespace Colours
{
    inline const juce::Colour background   { 0xff0b0c0f };
    inline const juce::Colour background2  { 0xff111318 };
    inline const juce::Colour surface      { 0xff171a20 };
    inline const juce::Colour elevated     { 0xff1e222a };
    inline const juce::Colour border       { 0xff2a2e37 };
    inline const juce::Colour text         { 0xfff3f3f1 };
    inline const juce::Colour textSecondary{ 0xffa4a8b0 };
    inline const juce::Colour textMuted    { 0xff727782 };
    inline const juce::Colour silver       { 0xffc7cbd2 };
    inline const juce::Colour gold         { 0xffd6b36a };
    inline const juce::Colour goldDeep     { 0xff96743a };
    inline const juce::Colour goldLight    { 0xffecd59c };
    inline const juce::Colour success      { 0xff75c99a };
    inline const juce::Colour error        { 0xffe47f83 };
    inline const juce::Colour warning      { 0xffd9a35b };
}

namespace Metrics
{
    constexpr float radiusSmall  = 8.0f;
    constexpr float radiusMedium = 12.0f;
    constexpr float radiusLarge  = 16.0f;

    constexpr int sidebarWidth = 236;
    constexpr int topBarHeight = 68;
    constexpr int pagePadding  = 28;
    constexpr int gap          = 16;
    constexpr int cardPadding  = 20;
}

namespace Fonts
{
    inline juce::Font make (float height, const char* style = "Regular")
    {
        return juce::Font (juce::FontOptions ("Segoe UI", height, juce::Font::plain).withStyle (style));
    }

    inline juce::Font display()  { return make (30.0f, "Light"); }
    inline juce::Font title()    { return make (21.0f, "Semibold"); }
    inline juce::Font heading()  { return make (15.5f, "Semibold"); }
    inline juce::Font body()     { return make (14.0f); }
    inline juce::Font bodyStrong() { return make (14.0f, "Semibold"); }
    inline juce::Font small()    { return make (12.5f); }
    inline juce::Font caption()  { return make (11.0f, "Semibold"); }
    inline juce::Font metric()   { return make (32.0f, "Light"); }

    inline juce::Font mono (float height = 12.5f)
    {
        return juce::Font (juce::FontOptions ("Cascadia Mono", height, juce::Font::plain)
                               .withFallbacks ({ "Consolas", "Courier New" }));
    }
}

/** Champagne-gold gradient used for primary actions and the recording state. */
inline juce::ColourGradient goldGradient (juce::Rectangle<float> r, float brightness = 1.0f)
{
    juce::ColourGradient g (Colours::goldLight.withMultipliedBrightness (brightness), r.getX(), r.getY(),
                            Colours::goldDeep.withMultipliedBrightness (brightness), r.getRight(), r.getBottom(), false);
    g.addColour (0.45, Colours::gold.withMultipliedBrightness (brightness));
    return g;
}

/** Fine metallic edge: brighter at the top, fading towards the bottom. */
inline juce::ColourGradient silverEdge (juce::Rectangle<float> r, float alpha = 1.0f)
{
    return juce::ColourGradient (Colours::silver.withAlpha (0.22f * alpha), r.getX(), r.getY(),
                                 Colours::silver.withAlpha (0.04f * alpha), r.getX(), r.getBottom(), false);
}

/** Layered card surface with a soft shadow and hairline metallic border. */
inline void paintCard (juce::Graphics& g, juce::Rectangle<float> r, float radius = Metrics::radiusLarge,
                       juce::Colour fill = Colours::surface, float emphasis = 0.0f)
{
    juce::DropShadow (juce::Colours::black.withAlpha (0.28f), 18, { 0, 6 }).drawForPath (g, [&] {
        juce::Path p; p.addRoundedRectangle (r, radius); return p; }());

    g.setGradientFill (juce::ColourGradient (fill.brighter (0.035f), r.getX(), r.getY(),
                                             fill, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);

    g.setGradientFill (silverEdge (r, 1.0f + emphasis));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);

    if (emphasis > 0.001f)
    {
        g.setColour (Colours::gold.withAlpha (0.35f * emphasis));
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    }
}

} // namespace stilleto::ui
