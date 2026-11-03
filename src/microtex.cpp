// Renders TeX math formulas with MicroTeX onto a wxGraphicsContext, so they can be shown inline in
// cell text. This is its own translation unit (like macos.mm), because MicroTeX's types live in the
// global `tex` namespace, which the unity build's `struct treesheets` scope would not match.

#include "microtex.h"

#include <wx/file.h>
#include <wx/filename.h>
#include <wx/graphics.h>
#include <wx/stdpaths.h>

#include "latex.h"

using namespace tex;

// The Computer Modern math fonts, compiled in by cmake/EmbedFiles.cmake, named by their path
// relative to the MicroTeX res/ directory (e.g. "fonts/base/cmmi10.ttf"). The metrics are already
// in the MicroTeX library; these files provide the glyphs.
struct EmbeddedFile {
    const char *name;
    const unsigned char *data;
    size_t size;
};
#include "embedded_microtexfonts.h"

namespace {

wxColour ToWx(tex::color c) {
    return wxColour(tex::color_r(c), tex::color_g(c), tex::color_b(c), tex::color_a(c));
}

// The family name a MicroTeX font file registers under is its basename without the extension
// (cmr10.ttf -> "cmr10").
wxString FamilyName(const std::string &path) {
    auto slash = path.find_last_of("/\\");
    auto base = slash == std::string::npos ? path : path.substr(slash + 1);
    auto dot = base.rfind('.');
    return wxString(dot == std::string::npos ? base : base.substr(0, dot));
}

// Writes an embedded font to a private file once, registers it with wx, and returns its family.
wxString RegisterFont(const std::string &path) {
    static std::unordered_map<std::string, wxString> families;
    auto it = families.find(path);
    if (it != families.end()) { return it->second; }

    wxString family = FamilyName(path);
    for (const auto &embedded : embedded_microtexfonts) {
        auto name = std::string(embedded.name);
        if (path.size() < name.size() ||
            path.compare(path.size() - name.size(), name.size(), name) != 0) {
            continue;
        }
        #ifdef __WXMAC__
            // Register from memory: on macOS AddPrivateFont() only accepts files already inside
            // the app bundle, and the glyph data is compiled in.
            if (!MacRegisterFont(embedded.data, embedded.size)) { family.clear(); }
        #else
            static wxString dir;
            if (dir.empty()) {
                dir = wxFileName::GetTempDir() + wxFILE_SEP_PATH +
                      wxString::Format("treesheets-texfonts-%ld",
                                       static_cast<long>(wxGetProcessId()));
                wxMkdir(dir);
            }
            auto file = dir + wxFILE_SEP_PATH + family + ".ttf";
            if (!wxFileExists(file)) {
                wxFile out(file, wxFile::write);
                out.Write(embedded.data, embedded.size);
            }
            if (!wxFont::AddPrivateFont(file)) { family.clear(); }
        #endif
        break;
    }
    families[path] = family;
    return family;
}

class FontWx : public tex::Font {
    wxFont font_;
    float size_;
    int style_;

public:
    FontWx(const wxString &family, float size, int style) : size_(size), style_(style) {
        auto weight = (style & tex::BOLD) ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL;
        auto fontstyle = (style & tex::ITALIC) ? wxFONTSTYLE_ITALIC : wxFONTSTYLE_NORMAL;
        font_ = family.empty()
                    ? wxFont(wxFontInfo(size).Weight(weight).Style(fontstyle))
                    : wxFont(wxFontInfo(size).FaceName(family).Weight(weight).Style(fontstyle));
    }

    float getSize() const override { return size_; }
    sptr<tex::Font> deriveFont(int style) const override {
        return sptrOf<FontWx>(font_.GetFaceName(), size_, style | style_);
    }
    bool operator==(const tex::Font &f) const override {
        const auto &o = static_cast<const FontWx &>(f);
        return size_ == o.size_ && style_ == o.style_ && font_.GetFaceName() == o.font_.GetFaceName();
    }
    bool operator!=(const tex::Font &f) const override { return !(*this == f); }

    const wxFont &GetWxFont() const { return font_; }
};

class TextLayoutWx : public tex::TextLayout {
    wxString text_;
    sptr<FontWx> font_;

public:
    TextLayoutWx(const std::wstring &src, const sptr<FontWx> &font)
        : text_(src.c_str(), src.size()), font_(font) {}

    void getBounds(tex::Rect &r) override {
        wxBitmap bmp(1, 1);
        wxMemoryDC dc(bmp);
        dc.SetFont(font_->GetWxFont());
        auto m = dc.GetFontMetrics();
        auto e = dc.GetTextExtent(text_);
        r.x = 0;
        r.y = -m.ascent;
        r.w = e.x;
        r.h = m.ascent + m.descent;
    }

    void draw(tex::Graphics2D &g2, float x, float y) override {
        g2.setFont(font_.get());
        g2.drawText(text_.ToStdWstring(), x, y);
    }
};

class Graphics2DWx : public tex::Graphics2D {
    wxGraphicsContext *gc_;
    tex::color color_ {tex::black};
    tex::Stroke stroke_;
    const FontWx *font_ {nullptr};
    float sx_ {1};
    float sy_ {1};

    void ApplyPen() { gc_->SetPen(wxPen(ToWx(color_), stroke_.lineWidth, wxPENSTYLE_SOLID)); }

public:
    explicit Graphics2DWx(wxGraphicsContext *gc) : gc_(gc) { setStroke(tex::Stroke()); }

    void setColor(tex::color c) override {
        color_ = c;
        ApplyPen();
        gc_->SetBrush(wxBrush(ToWx(c)));
        if (font_) { gc_->SetFont(gc_->CreateFont(font_->GetWxFont(), ToWx(c))); }
    }
    tex::color getColor() const override { return color_; }
    void setStroke(const tex::Stroke &s) override {
        stroke_ = s;
        ApplyPen();
    }
    const tex::Stroke &getStroke() const override { return stroke_; }
    void setStrokeWidth(float w) override {
        stroke_.lineWidth = w;
        ApplyPen();
    }
    const tex::Font *getFont() const override { return font_; }
    void setFont(const tex::Font *font) override {
        font_ = static_cast<const FontWx *>(font);
        gc_->SetFont(gc_->CreateFont(font_->GetWxFont(), ToWx(color_)));
    }

    void translate(float dx, float dy) override { gc_->Translate(dx, dy); }
    void scale(float sx, float sy) override {
        sx_ *= sx;
        sy_ *= sy;
        gc_->Scale(sx, sy);
    }
    void rotate(float angle) override { gc_->Rotate(angle); }
    void rotate(float angle, float px, float py) override {
        gc_->Translate(px, py);
        gc_->Rotate(angle);
        gc_->Translate(-px, -py);
    }
    void reset() override {
        gc_->SetTransform(gc_->CreateMatrix());
        sx_ = sy_ = 1;
    }
    float sx() const override { return sx_; }
    float sy() const override { return sy_; }

    void drawChar(wchar_t c, float x, float y) override {
        drawText(std::wstring(1, c), x, y);
    }
    void drawText(const std::wstring &t, float x, float y) override {
        if (t.empty()) { return; }
        auto str = wxString(t.c_str(), t.size());
        // wx draws text from its top left corner, while MicroTeX gives the baseline.
        wxDouble w, h, descent, leading;
        gc_->GetTextExtent(str, &w, &h, &descent, &leading);
        gc_->DrawText(str, x, y - (h - descent));
    }

    void drawLine(float x1, float y1, float x2, float y2) override {
        gc_->StrokeLine(x1, y1, x2, y2);
    }
    void drawRect(float x, float y, float w, float h) override {
        gc_->SetBrush(*wxTRANSPARENT_BRUSH);
        gc_->DrawRectangle(x, y, w, h);
    }
    void fillRect(float x, float y, float w, float h) override {
        gc_->SetPen(*wxTRANSPARENT_PEN);
        gc_->DrawRectangle(x, y, w, h);
    }
    void drawRoundRect(float x, float y, float w, float h, float rx, float ry) override {
        gc_->SetBrush(*wxTRANSPARENT_BRUSH);
        gc_->DrawRoundedRectangle(x, y, w, h, (rx + ry) / 2);
    }
    void fillRoundRect(float x, float y, float w, float h, float rx, float ry) override {
        gc_->SetPen(*wxTRANSPARENT_PEN);
        gc_->DrawRoundedRectangle(x, y, w, h, (rx + ry) / 2);
    }
};

}  // namespace

tex::Font *tex::Font::create(const std::string &file, float size) {
    return new FontWx(RegisterFont(file), size, tex::PLAIN);
}

sptr<tex::Font> tex::Font::_create(const std::string &name, int style, float size) {
    return sptrOf<FontWx>(wxString(name), size, style);
}

sptr<tex::TextLayout> tex::TextLayout::create(const std::wstring &src, const sptr<tex::Font> &font) {
    return sptrOf<TextLayoutWx>(src, std::static_pointer_cast<FontWx>(font));
}

bool RenderFormula(const wxString &latex, int textsize, const wxColour &fg, int maxwidth,
                   RenderedFormula &out) {
    if (latex.empty()) { return false; }

    static bool initialized = false;
    if (!initialized) {
        tex::LaTeX::init("");
        initialized = true;
    }

    auto color = tex::argb(255, fg.Red(), fg.Green(), fg.Blue());
    std::unique_ptr<tex::TeXRender> render(
        tex::LaTeX::parse(latex.ToStdWstring(), maxwidth, textsize, textsize / 2.f, color));
    if (!render) { return false; }

    out.width = render->getWidth();
    out.height = render->getHeight();
    out.depth = render->getDepth();
    if (out.width <= 0 || out.height <= 0) { return false; }

    wxImage image(out.width, out.height);
    image.InitAlpha();
    memset(image.GetAlpha(), 0, out.width * out.height);
    memset(image.GetData(), 0, out.width * out.height * 3);
    wxBitmap bitmap(image, 32);
    {
        wxMemoryDC dc(bitmap);
        auto *gc = wxGraphicsContext::Create(dc);
        if (!gc) { return false; }
        gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
        Graphics2DWx g2(gc);
        render->draw(g2, 0, 0);
        delete gc;
    }
    out.bitmap = bitmap;
    return true;
}
