// Renders TeX math formulas with MicroTeX, for showing them inline in cell text. The
// implementation lives in microtex.cpp (its own translation unit, so MicroTeX's namespace can be
// opened at global scope), like macos.mm and lobster_impl.cpp.

#pragma once

// A cached, laid-out TeX formula. `handle` is opaque to callers; the dimensions are in points.
struct FormulaLayout {
    const void *handle {nullptr};
    int width {0};   // total width
    int height {0};  // total height, above and below the baseline
    int depth {0};   // below-baseline amount, for aligning the formula with text
};

// Returns a cached layout for the LaTeX math in `latex` (without the surrounding delimiters) at
// `textsize` points, parsing it once. Returns an empty layout (handle null) if it fails. Layouts
// are cached for the process lifetime.
FormulaLayout GetFormulaLayout(const wxString &latex, int textsize);

// Draws a formula from GetFormulaLayout() with its top-left at (x, y) and in `color`. Uses the
// DC's graphics context for crisp vector output where it has one, and falls back to a bitmap
// otherwise (printing and SVG export).
void DrawFormula(const FormulaLayout &formula, wxDC &dc, int x, int y, const wxColour &color);
