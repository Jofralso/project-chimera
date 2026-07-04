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

    int menu_btn_x = t.screen_w - t.padding - 30;
    int menu_btn_y = t.padding + 6;
    int menu_btn_r = 10;
    canvas.circle(menu_btn_x, menu_btn_y, menu_btn_r, t.fg, true);
    canvas.circle(menu_btn_x, menu_btn_y, menu_btn_r - 2, t.bg, true);
    canvas.circle(menu_btn_x, menu_btn_y, menu_btn_r - 2, t.fg);
    for (int i = -4; i <= 4; i += 4) {
        canvas.fill_rect(menu_btn_x + i - 1, menu_btn_y - 3, 2, 6, t.fg);
    }
    for (int i = -4; i <= 4; i += 4) {
        canvas.fill_rect(menu_btn_x - 3, menu_btn_y + i - 1, 6, 2, t.fg);
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

    canvas.circle(cx, cy, r, c);
    canvas.circle(cx, cy, r - t.knob_stroke, t.bg, true);
    canvas.circle(cx, cy, r - t.knob_stroke, c);

    double angle = -135.0 + k.value * 270.0;
    double rad = angle * 3.14159 / 180.0;
    int line_len = r - t.knob_stroke - 2;
    for (int i = 0; i <= line_len; ++i) {
        int px = cx + static_cast<int>(i * cos(rad));
        int py = cy + static_cast<int>(i * sin(rad));
        canvas.set_pixel(px, py, k.active ? t.fg : t.fg_dim);
    }

    canvas.text_centered(cx - r, cy + r + t.padding, r * 2, k.label, c, 1);
}

int Screen::hit_test_knob(int mx, int my) const {
    auto& t = theme();
    const KnobState* knobs = this->knobs();
    int count = this->knob_count();
    if (!knobs || count == 0) return -1;

    int knob_area_y = t.screen_h - t.footer_h;
    int total_w = count * (t.knob_r * 2 + t.padding * 2);
    int start_x = (t.screen_w - total_w) / 2;
    if (start_x < t.padding) start_x = t.padding;

    for (int i = 0; i < count; ++i) {
        int kx = start_x + i * ((t.knob_r * 2) + t.padding * 2);
        int ky = knob_area_y + t.padding;
        int cx = kx + t.knob_r;
        int cy = ky + t.knob_r + 8;
        int dx = mx - cx;
        int dy = my - cy;
        if (dx * dx + dy * dy <= (t.knob_r + 4) * (t.knob_r + 4)) {
            return i;
        }
    }
    return -1;
}

void Screen::handle_knob_drag(int my) {
    if (drag_knob_ < 0 || drag_knob_ >= knob_count()) return;
    KnobState* k = knobs();
    if (!k) return;
    int delta = drag_start_y_ - my;
    float range = k[drag_knob_].max - k[drag_knob_].min;
    float step = k[drag_knob_].step;
    if (step <= 0.0f) step = range / 64.0f;
    k[drag_knob_].value = drag_start_value_ + static_cast<float>(delta) * step;
    if (k[drag_knob_].value < k[drag_knob_].min) k[drag_knob_].value = k[drag_knob_].min;
    if (k[drag_knob_].value > k[drag_knob_].max) k[drag_knob_].value = k[drag_knob_].max;
}

void Screen::end_knob_drag() {
    drag_knob_ = -1;
    drag_start_y_ = 0;
    drag_start_value_ = 0.0f;
}

void Screen::set_footer(Canvas& canvas) {
    draw_footer(canvas);
}

} // namespace chimera::ui
