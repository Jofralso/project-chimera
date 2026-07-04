#include "chimera/ui/screens/synth_screen.h"

#include <SDL_events.h>
#include <cmath>

namespace chimera::ui {

void SynthScreen::render(Canvas& canvas, bool active) {
    auto& t = theme();

    draw_header(canvas, name_, "M4 - SUBTRACTIVE");

    int y = t.header_h + t.padding;

    // Animated waveform preview — draw multiple cycles with pulsing amplitude
    int preview_h = 100;
    canvas.rect(t.padding, y, t.screen_w - 2 * t.padding, preview_h, t.fg_dim);

    int cy = y + preview_h / 2;
    int wf_type = static_cast<int>(knobs_[0].value * 4);
    if (wf_type > 3) wf_type = 3;
    float amp_pulse = active ? pulse_.value() : 1.0f;

    for (int px = 0; px < t.screen_w - 2 * t.padding; ++px) {
        float phase = static_cast<float>(px) / (t.screen_w - 2 * t.padding);
        float val = 0;
        switch (wf_type) {
            case 0:
                val = std::sin(phase * 6.28318f * 4 + pulse_.phase);
                break;
            case 1:
                val = 2.0f * (phase * 4 - std::floor(phase * 4 + 0.5f));
                break;
            case 2:
                val = (std::fmod(phase * 4, 1.0f) < 0.5f) ? 1.0f : -1.0f;
                break;
            case 3: {
                float t2 = std::fmod(phase * 4, 1.0f);
                val = t2 < 0.5f ? 4.0f * t2 - 1.0f : 3.0f - 4.0f * t2;
                break;
            }
        }
        val *= amp_pulse;
        int sy = static_cast<int>(val * (preview_h / 2 - 4));
        int py = cy + sy;
        if (py >= y && py < y + preview_h)
            canvas.set_pixel(t.padding + px, py, t.fg);
        // Thicker line
        if (py > y && py - 1 < y + preview_h)
            canvas.set_pixel(t.padding + px, py - 1, t.fg_dim);
        if (py + 1 < y + preview_h)
            canvas.set_pixel(t.padding + px, py + 1, t.fg_dim);
    }

    y += preview_h + 8;

    // Waveform label
    const char* waveforms[] = {"SINE", "SAW", "SQUARE", "TRIANGLE"};
    canvas.text(t.padding, y, "WAVEFORM", t.fg_dim);
    int idx = static_cast<int>(knobs_[0].value * 4); if (idx > 3) idx = 3;
    canvas.text(t.padding + 70, y, waveforms[idx],
                active ? t.fg : t.fg_dim, 2);

    y += 28;

    // Animated ADSR visualization
    int adsr_h = 60;
    int adsr_w = t.screen_w - 2 * t.padding;
    canvas.rect(t.padding, y, adsr_w, adsr_h, t.fg_dim);

    float env_pos = anim_adsr_.value();
    int seg_a = static_cast<int>(20 * (1.0f + 0.3f * pulse_.value()));
    int seg_d = static_cast<int>(40 * (1.0f - 0.2f * knobs_[2].value));
    int seg_r = static_cast<int>(40 * (1.0f + 0.2f * pulse_.value()));
    int seg_s = static_cast<int>(15 * knobs_[1].value);

    int base_y = y + adsr_h - 4;
    int pts[5][2] = {
        {t.padding + 4, base_y},
        {t.padding + 4 + seg_a, y + 4},
        {t.padding + 4 + seg_a + seg_d, base_y - seg_s},
        {t.padding + adsr_w - 4 - seg_r, base_y - seg_s},
        {t.padding + adsr_w - 4, base_y}
    };
    for (int i = 0; i < 4; ++i) {
        int x0 = pts[i][0], y0 = pts[i][1];
        int x1 = pts[i + 1][0], y1 = pts[i + 1][1];
        int steps = x1 - x0;
        if (steps <= 0) steps = 1;
        for (int s = 0; s < steps; ++s) {
            int px = x0 + (s * (x1 - x0)) / steps;
            int py = y0 + (s * (y1 - y0)) / steps;
            float t_frac = static_cast<float>(s) / steps;
            Color c = (t_frac > env_pos) ? t.fg_dim : t.fg;
            canvas.set_pixel(px, py, c);
        }
    }

    // Animated env playhead dot
    int env_x = t.padding + 4 + static_cast<int>(env_pos * (adsr_w - 8));
    int env_y_height = base_y - (y + 4);
    int env_y_pos = base_y - static_cast<int>(env_pos * env_y_height);
    canvas.circle(env_x, env_y_pos, 3, t.fg_bright, true);

    canvas.text(t.padding + 4, y + adsr_h + 4, "A", t.fg_dim);
    canvas.text(t.padding + 4 + seg_a + seg_d / 2, y + adsr_h + 4, "D", t.fg_dim);
    canvas.text(t.padding + adsr_w - 4 - seg_r / 2, y + adsr_h + 4, "R", t.fg_dim);
    canvas.text_centered(0, y + adsr_h + 4, t.screen_w, "S", t.fg_dim);

    y += adsr_h + 24;

    // Filter info
    canvas.text(t.padding, y, "FILTER", t.fg_dim);
    canvas.text(t.padding, y + 14, "LP 24dB/oct", t.fg, 1);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%.0f%%", knobs_[1].value * 100);
    canvas.text_right(t.padding, y, t.screen_w - 2 * t.padding, buf,
                      active ? t.fg : t.fg_dim, 2);

    draw_knobs(canvas, knobs_, 4);

    apply_to_engine();
}

void SynthScreen::on_knob(int index, int delta) {
    if (index < 0 || index >= 4) return;
    knobs_[index].value += delta * knobs_[index].step;
    if (knobs_[index].value < knobs_[index].min)
        knobs_[index].value = knobs_[index].min;
    if (knobs_[index].value > knobs_[index].max)
        knobs_[index].value = knobs_[index].max;

    if (index == 0) {
        const char* names[] = {"sine", "saw", "square", "tri"};
        int idx = static_cast<int>(knobs_[0].value * 4);
        if (idx > 3) idx = 3;
        knobs_[0].value_str = names[idx];
    } else {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%.2f", knobs_[index].value);
        knobs_[index].value_str = buf;
    }

    apply_to_engine();
}

void SynthScreen::on_mouse(int mx, int my, int buttons) {
    auto& t = theme();

    if (buttons & SDL_BUTTON_LMASK) {
        if (drag_knob_ < 0) {
            drag_knob_ = hit_test_knob(mx, my);
            if (drag_knob_ >= 0) {
                drag_start_y_ = my;
                drag_start_value_ = knobs()[drag_knob_].value;
            }
        }
        if (drag_knob_ >= 0) {
            handle_knob_drag(my);
            apply_to_engine();
            return;
        }

        int preview_y = t.header_h + t.padding;
        int preview_h = 100;
        if (my >= preview_y && my < preview_y + preview_h) {
            knobs_[0].value += 0.25f;
            if (knobs_[0].value > 1.0f) knobs_[0].value = 0.0f;
            const char* names[] = {"sine", "saw", "square", "tri"};
            int idx = static_cast<int>(knobs_[0].value * 4);
            if (idx > 3) idx = 3;
            knobs_[0].value_str = names[idx];
            apply_to_engine();
        }
    } else {
        end_knob_drag();
    }
}

} // namespace chimera::ui
