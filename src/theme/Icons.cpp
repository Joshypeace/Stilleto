#include "theme/Icons.h"

#include "theme/DesignTokens.h"

namespace stilleto::ui::Icons
{
namespace
{
void addLine (juce::Path& p, float x1, float y1, float x2, float y2)
{
    p.startNewSubPath (x1, y1);
    p.lineTo (x2, y2);
}

void addArc (juce::Path& p, float cx, float cy, float r, float fromDeg, float toDeg)
{
    p.addCentredArc (cx, cy, r, r, 0.0f, juce::degreesToRadians (fromDeg), juce::degreesToRadians (toDeg), true);
}
} // namespace

juce::Path path (Id id)
{
    juce::Path p;

    switch (id)
    {
        case Id::Overview:
            p.addRoundedRectangle (3.5f, 3.5f, 7.0f, 7.0f, 1.8f);
            p.addRoundedRectangle (13.5f, 3.5f, 7.0f, 7.0f, 1.8f);
            p.addRoundedRectangle (3.5f, 13.5f, 7.0f, 7.0f, 1.8f);
            p.addRoundedRectangle (13.5f, 13.5f, 7.0f, 7.0f, 1.8f);
            break;

        case Id::Dictation:
            addLine (p, 4, 10, 4, 14);
            addLine (p, 8, 7, 8, 17);
            addLine (p, 12, 3.5f, 12, 20.5f);
            addLine (p, 16, 7, 16, 17);
            addLine (p, 20, 10, 20, 14);
            break;

        case Id::History:
            addArc (p, 12, 12, 8.5f, -120.0f, 210.0f);
            addLine (p, 12, 7.5f, 12, 12);
            addLine (p, 12, 12, 15.2f, 14.2f);
            addLine (p, 3.6f, 7.2f, 4.3f, 10.6f);
            addLine (p, 4.3f, 10.6f, 7.6f, 9.6f);
            break;

        case Id::Models:
            p.startNewSubPath (12, 3);
            p.lineTo (20, 7.5f); p.lineTo (20, 16.5f); p.lineTo (12, 21); p.lineTo (4, 16.5f); p.lineTo (4, 7.5f);
            p.closeSubPath();
            addLine (p, 4, 7.5f, 12, 12);
            addLine (p, 20, 7.5f, 12, 12);
            addLine (p, 12, 12, 12, 21);
            break;

        case Id::Settings:
            addLine (p, 4, 7, 20, 7);
            addLine (p, 4, 12, 20, 12);
            addLine (p, 4, 17, 20, 17);
            p.addEllipse (13, 5, 4, 4);
            p.addEllipse (6, 10, 4, 4);
            p.addEllipse (11, 15, 4, 4);
            break;

        case Id::Help:
            p.addEllipse (3.5f, 3.5f, 17, 17);
            p.startNewSubPath (9.4f, 9.4f);
            p.cubicTo (9.4f, 6.2f, 14.8f, 6.2f, 14.8f, 9.4f);
            p.cubicTo (14.8f, 11.6f, 12, 11.8f, 12, 14.0f);
            addLine (p, 12, 16.8f, 12, 17.0f);
            break;

        case Id::Mic:
            p.addRoundedRectangle (9, 3, 6, 11, 3);
            addArc (p, 12, 11, 6, 90.0f, 270.0f);
            addLine (p, 12, 17, 12, 21);
            addLine (p, 9, 21, 15, 21);
            break;

        case Id::Stop:
            p.addRoundedRectangle (6.5f, 6.5f, 11, 11, 2.5f);
            break;

        case Id::Copy:
            p.addRoundedRectangle (8.5f, 8.5f, 11.5f, 11.5f, 2.2f);
            p.startNewSubPath (15.5f, 8.5f);
            p.lineTo (15.5f, 6); p.quadraticTo (15.5f, 4, 13.5f, 4);
            p.lineTo (6, 4); p.quadraticTo (4, 4, 4, 6);
            p.lineTo (4, 13.5f); p.quadraticTo (4, 15.5f, 6, 15.5f);
            p.lineTo (8.5f, 15.5f);
            break;

        case Id::Insert:
            addLine (p, 12, 3.5f, 12, 14);
            addLine (p, 8, 10, 12, 14);
            addLine (p, 16, 10, 12, 14);
            p.startNewSubPath (4, 14);
            p.lineTo (4, 18); p.quadraticTo (4, 20, 6, 20);
            p.lineTo (18, 20); p.quadraticTo (20, 20, 20, 18);
            p.lineTo (20, 14);
            break;

        case Id::Check:
            p.startNewSubPath (5, 12.5f);
            p.lineTo (10, 17.5f);
            p.lineTo (19.5f, 7);
            break;

        case Id::Close:
            addLine (p, 6, 6, 18, 18);
            addLine (p, 18, 6, 6, 18);
            break;

        case Id::Minimise:
            addLine (p, 6, 12.5f, 18, 12.5f);
            break;

        case Id::Search:
            p.addEllipse (4, 4, 12.5f, 12.5f);
            addLine (p, 14.6f, 14.6f, 20, 20);
            break;

        case Id::Trash:
            addLine (p, 4, 6.5f, 20, 6.5f);
            p.startNewSubPath (9, 6.5f); p.lineTo (9.5f, 4); p.lineTo (14.5f, 4); p.lineTo (15, 6.5f);
            p.startNewSubPath (6, 6.5f); p.lineTo (7, 19); p.quadraticTo (7.2f, 20.5f, 8.7f, 20.5f);
            p.lineTo (15.3f, 20.5f); p.quadraticTo (16.8f, 20.5f, 17, 19); p.lineTo (18, 6.5f);
            addLine (p, 10.2f, 10, 10.5f, 17);
            addLine (p, 13.8f, 10, 13.5f, 17);
            break;

        case Id::Download:
            addLine (p, 12, 3.5f, 12, 15);
            addLine (p, 7.5f, 10.5f, 12, 15);
            addLine (p, 16.5f, 10.5f, 12, 15);
            addLine (p, 5, 20, 19, 20);
            break;

        case Id::Folder:
            p.startNewSubPath (3.5f, 7);
            p.quadraticTo (3.5f, 5, 5.5f, 5);
            p.lineTo (9.5f, 5); p.lineTo (11.5f, 7.5f); p.lineTo (18.5f, 7.5f);
            p.quadraticTo (20.5f, 7.5f, 20.5f, 9.5f);
            p.lineTo (20.5f, 17); p.quadraticTo (20.5f, 19, 18.5f, 19);
            p.lineTo (5.5f, 19); p.quadraticTo (3.5f, 19, 3.5f, 17);
            p.closeSubPath();
            break;

        case Id::Keyboard:
            p.addRoundedRectangle (2.5f, 6, 19, 12, 2.5f);
            for (float x : { 6.0f, 9.5f, 13.0f, 16.5f })
                addLine (p, x, 9.6f, x + 0.6f, 9.6f);
            addLine (p, 7.5f, 14.5f, 16.5f, 14.5f);
            break;

        case Id::Shield:
            p.startNewSubPath (12, 3);
            p.lineTo (19.5f, 6); p.lineTo (19.5f, 11.5f);
            p.cubicTo (19.5f, 16, 16, 19.5f, 12, 21);
            p.cubicTo (8, 19.5f, 4.5f, 16, 4.5f, 11.5f);
            p.lineTo (4.5f, 6);
            p.closeSubPath();
            p.startNewSubPath (8.8f, 12); p.lineTo (11.2f, 14.4f); p.lineTo (15.4f, 9.6f);
            break;

        case Id::Alert:
            p.addEllipse (3.5f, 3.5f, 17, 17);
            addLine (p, 12, 7.5f, 12, 13);
            addLine (p, 12, 16.3f, 12, 16.5f);
            break;

        case Id::Refresh:
            addArc (p, 12, 12, 7.5f, 30.0f, 300.0f);
            p.startNewSubPath (15.5f, 3.5f); p.lineTo (16.3f, 5.6f); p.lineTo (14, 6.6f);
            break;

        case Id::Clock:
            p.addEllipse (3.5f, 3.5f, 17, 17);
            addLine (p, 12, 7.5f, 12, 12);
            addLine (p, 12, 12, 15, 14);
            break;

        case Id::Words:
            addLine (p, 4, 6, 20, 6);
            addLine (p, 4, 10, 20, 10);
            addLine (p, 4, 14, 20, 14);
            addLine (p, 4, 18, 13, 18);
            break;

        case Id::Chip:
            p.addRoundedRectangle (6, 6, 12, 12, 2.2f);
            for (float c : { 9.5f, 14.5f })
            {
                addLine (p, c, 3, c, 6); addLine (p, c, 18, c, 21);
                addLine (p, 3, c, 6, c); addLine (p, 18, c, 21, c);
            }
            break;

        case Id::Spark:
            p.startNewSubPath (12, 3);
            p.quadraticTo (13, 11, 21, 12);
            p.quadraticTo (13, 13, 12, 21);
            p.quadraticTo (11, 13, 3, 12);
            p.quadraticTo (11, 11, 12, 3);
            p.closeSubPath();
            break;

        case Id::Cursor:
            addLine (p, 12, 4, 12, 20);
            addLine (p, 9, 4, 15, 4);
            addLine (p, 9, 20, 15, 20);
            break;
    }

    return p;
}

void draw (juce::Graphics& g, Id id, juce::Rectangle<float> bounds, juce::Colour colour, float strokeWidth)
{
    auto p = path (id);
    const float scale = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 24.0f;
    p.applyTransform (juce::AffineTransform::scale (scale)
                          .translated (bounds.getCentreX() - 12.0f * scale, bounds.getCentreY() - 12.0f * scale));

    g.setColour (colour);
    g.strokePath (p, juce::PathStrokeType (strokeWidth * juce::jmax (0.75f, scale), juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
}

void drawBrandMark (juce::Graphics& g, juce::Rectangle<float> bounds, float shimmer, bool withTile)
{
    const auto r = bounds.withSizeKeepingCentre (juce::jmin (bounds.getWidth(), bounds.getHeight()),
                                                 juce::jmin (bounds.getWidth(), bounds.getHeight()));
    const float s = r.getWidth();

    if (withTile)
    {
        const float radius = s * 0.26f;
        g.setGradientFill (juce::ColourGradient (Colours::elevated.brighter (0.12f), r.getX(), r.getY(),
                                                 Colours::background, r.getRight(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radius);

        g.setGradientFill (juce::ColourGradient (Colours::silver.withAlpha (0.55f), r.getX(), r.getY(),
                                                 Colours::silver.withAlpha (0.06f), r.getX(), r.getBottom(), false));
        g.drawRoundedRectangle (r.reduced (s * 0.012f), radius, juce::jmax (1.0f, s * 0.02f));
    }

    const float cx = r.getCentreX();
    const float cy = r.getCentreY();
    const float barW = s * 0.075f;

    // Silver voice bars, symmetric around the blade.
    struct Bar { float dx, h; };
    for (const auto& b : { Bar { -0.27f, 0.20f }, Bar { -0.15f, 0.38f }, Bar { 0.15f, 0.38f }, Bar { 0.27f, 0.20f } })
    {
        const auto bar = juce::Rectangle<float> (barW, s * b.h).withCentre ({ cx + b.dx * s, cy });
        g.setGradientFill (juce::ColourGradient (Colours::silver, bar.getX(), bar.getY(),
                                                 Colours::silver.darker (0.55f), bar.getX(), bar.getBottom(), false));
        g.fillRoundedRectangle (bar, barW * 0.5f);
    }

    // The stiletto: a tall gold blade with a rounded head and a needle point.
    juce::Path blade;
    const float top = cy - s * 0.34f, bottom = cy + s * 0.36f, half = s * 0.062f;
    blade.startNewSubPath (cx - half, top + half);
    blade.quadraticTo (cx - half, top, cx, top);
    blade.quadraticTo (cx + half, top, cx + half, top + half);
    blade.lineTo (cx + half * 0.9f, cy + s * 0.08f);
    blade.lineTo (cx, bottom);
    blade.lineTo (cx - half * 0.9f, cy + s * 0.08f);
    blade.closeSubPath();

    g.setGradientFill (goldGradient ({ cx - half, top, half * 2, bottom - top }));
    g.fillPath (blade);

    // Metallic highlight sweeping across the blade during the startup animation.
    if (shimmer > 0.0f && shimmer < 1.0f)
    {
        const float y = top + (bottom - top) * shimmer;
        juce::ColourGradient glint (juce::Colours::white.withAlpha (0.0f), cx, y - s * 0.12f,
                                    juce::Colours::white.withAlpha (0.0f), cx, y + s * 0.12f, false);
        glint.addColour (0.5, juce::Colours::white.withAlpha (0.75f));
        g.setGradientFill (glint);
        g.fillPath (blade);
    }
}

juce::Image renderBrandImage (int size, bool recordingDot)
{
    juce::Image image (juce::Image::ARGB, size, size, true);
    juce::Graphics g (image);
    drawBrandMark (g, juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (size), static_cast<float> (size)).reduced (size * 0.03f));

    if (recordingDot)
    {
        const float d = size * 0.34f;
        const auto dot = juce::Rectangle<float> (size - d - size * 0.02f, size - d - size * 0.02f, d, d);
        g.setColour (Colours::background);
        g.fillEllipse (dot.expanded (size * 0.04f));
        g.setGradientFill (goldGradient (dot, 1.15f));
        g.fillEllipse (dot);
    }

    return image;
}

} // namespace stilleto::ui::Icons
