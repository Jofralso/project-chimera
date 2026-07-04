#include "chimera/ui/screen.h"

#include <cmath>

namespace chimera::ui {

void Screen::draw_header(Canvas& canvas, const std::string& title,
                         const std::string& subtitle)
{
    auto& t = theme();
    canvas.fill_rect(0, 0, t.screen_w, t.header_h, t.bg);
    canvas.hline(0, t.header_h - 1, t.screen_w, t.fg_dim);
    canvas.text(t.padding, t.padding, title, t.fg, 2);
    if (!subtitle.empty()) {
        canvas.text_right(t.padding, t.padding, t.screen_w - 2 * t.padding,
                         subtitle, t.fg_dim);
    }
}

void Screen::draw_footer(Canvas& canvas, const std::string& hint) {
    auto& t = theme();
    int y = t.screen_h - t.footer_h;
    canvas.fill_rect(0, y, t.screen_w, t.footer_h, t.bg);
    canvas.hline(0, y, t.screen_w, t.fg_dim);
    if (!hint.empty()) {
        canvas.text_centered(0, y + t.padding, t.screen_w, hint, t.fg_dim);
    }
}

void Screen::draw_knobs(Canvas& canvas, const KnobState* knobs, int count) {
    if (!knobs || count == 0) return;
    auto& t = theme();
    int knob_area_y = t.screen_h - t.footer_h;
    canvas.fill_rect(0, knob_area_y, t.screen_w, t.footer_h, t.bg);
    canvas.hline(0, knob_area_y, t.screen_w, t.fg_dim);

    int total_w = count * (t.knob_r * 2 + t.padding * 2);
    int start_x = (t.screen_w - total_w) / 2;
    if (start_x < t.padding) start_x = t.padding;

    for (int i = 0; i < count; ++i) {
        int kx = start_x + i * ((t.knob_r * 2) + t.padding * 2);
        int ky = knob_area_y + t.padding;
        draw_knob(canvas, kx + t.knob_r, ky + t.knob_r + 8, knobs[i]);
    }
}

void Screen::draw_knob(Canvas& canvas, int cx, int cy, const KnobState& k) {
    auto& t = theme();
    int r = t.knob_r;
    Color c = k.active ? t.fg : t.fg_dim;

    // Knob circle
    canvas.circle(cx, cy, r, c);
    canvas.circle(cx, cy, r - t.knob_stroke, t.bg, true);
    canvas.circle(cx, cy, r - t.knob_stroke, c);

    // Indicator line
    double angle = -135.0 + k.value * 270.0;
    double rad = angle * 3.14159 / 180.0;
    int line_len = r - t.knob_stroke - 2;
    for (int i = 0; i <= line_len; ++i) {
        int px = cx + static_cast<int>(i * cos(rad));
        int py = cy + static_cast<int>(i * sin(rad));
        canvas.set_pixel(px, py, k.active ? t.fg : t.fg_dim);
    }

    // Label
    canvas.text_centered(cx - r, cy + r + t.padding, r * 2, k.label, c, 1);
}

void Screen::set_footer(Canvas& canvas) {
    draw_footer(canvas);
}

} // namespace chimera::ui
