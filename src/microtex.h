// Renders TeX math formulas to bitmaps with MicroTeX, for showing them inline in cell text. The
// implementation lives in microtex.cpp (its own translation unit, so MicroTeX's namespace can be
// opened at global scope), like macos.mm and lobster_impl.cpp.

#pragma once

struct RenderedFormula {
    wxBitmap bitmap;
    int width {0};   // bitmap width in pixels
    int height {0};  // total height in pixels, above and below the baseline
    int depth {0};   // below-baseline amount in pixels, for aligning the formula with text
};

// Renders the LaTeX math in `latex` (without the surrounding $...$) to a transparent bitmap.
// `textsize` is the font size in points, `fg` the glyph color. Returns false if it fails.
bool RenderFormula(const wxString &latex, int textsize, const wxColour &fg, int maxwidth,
                   RenderedFormula &out);
