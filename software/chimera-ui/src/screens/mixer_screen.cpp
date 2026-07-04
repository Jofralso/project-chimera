#include "chimera/ui/screens/mixer_screen.h"
#include <SDL_events.h>
#include <cstdio>
#include <cmath>

namespace chimera::ui {

MixerScreen::MixerScreen()
    : Screen("MIXER")
{
    knobs_[0] = {"VOLUME", "0.80", 0.8f, 0, 1, 0.01f, true};
    knobs_[1] = {"PAN", "C", 0.5f, 0, 1, 0.01f, false};
    knobs_[2] = {"FX SEND", "0.00", 0.0f, 0, 1, 0.01f, false};
    knobs_[3] = {"MASTER", "0.90", 0.9f, 0, 1, 0.01f, true};
}

void MixerScreen::update() {
    pulse_.update();
    for (int i = 0; i < 4; ++i) channel_meters_[i].update();
    master_meter_.update();
}

void MixerScreen::render(Canvas& canvas, bool active) {
    auto& t = theme();

    draw_header(canvas, name_, "M6 - MIXER");

    int strip_w = (t.screen_w - 2 * t.padding - 3 * t.widget_spacing) / 4;
    int strip_y = t.header_h + t.padding;
    int strip_h = 340;

    const char* names[] = {"SYNTH", "DRUM", "SEQ", "FX"};
    for (int i = 0; i < 4; ++i) {
        int sx = t.padding + i * (strip_w + t.widget_spacing);
        draw_channel_strip(canvas, sx, strip_y, strip_w, strip_h,
                          i, names[i], channel_meters_[i].value());
    }

    // Master section
    int master_y = strip_y + strip_h + 8;
    canvas.hline(t.padding, master_y, t.screen_w - 2 * t.padding, t.fg_dim);

    char buf[32];
    std::snprintf(buf, sizeof(buf), "MASTER %.0f%%", knobs_[3].value * 100);
    canvas.text(t.padding, master_y + 8, buf,
                active ? t.fg : t.fg_dim, 2);

    // Animated master level meter (horizontal)
    int master_meter_y = master_y + 32;
    int mm_w = t.screen_w - 2 * t.padding;
    float mm_val = master_meter_.value();
    Color mm_c = mm_val > 0.8f ? t.red : (mm_val > 0.6f ? t.accent : t.fg);
    canvas.rect(t.padding, master_meter_y, mm_w, 12, t.fg_dim);
    canvas.fill_rect(t.padding, master_meter_y,
                     static_cast<int>(mm_w * mm_val), 12, mm_c);

    // Clip indicator
    if (mm_val > 0.95f) {
        float pv = pulse_.value();
        if (pv > 0.8f) {
            canvas.text_centered(0, master_meter_y - 14, t.screen_w,
                                 "CLIP!", t.red, 2);
        }
    }

    draw_knobs(canvas, knobs_, 4);

    apply_to_engine();
}

void MixerScreen::draw_channel_strip(Canvas& canvas, int x, int y, int w, int h,
                                      int channel, const char* name, float level) {
    auto& t = theme();
    bool sel = (channel == selected_channel_);

    // Background
    canvas.rect(x, y, w, h, sel ? t.fg : t.fg_dim);

    // Channel name
    canvas.text_centered(x, y + 4, w, name, sel ? t.fg : t.fg_dim, 1);

    // Vertical VU meter
    int meter_x = x + 2;
    int meter_y = y + 20;
    int meter_w = w - 4;
    int meter_h = h - 80;

    canvas.rect(meter_x, meter_y, meter_w, meter_h, t.muted);

    // Animated level
    Color meter_c = level > 0.8f ? t.red : (level > 0.5f ? t.accent : t.fg);
    int fill_h = static_cast<int>(level * meter_h);
    if (fill_h > meter_h) fill_h = meter_h;

    // Animated segments (8 segment VU)
    int segs = 8;
    int seg_h = meter_h / segs;
    for (int s = 0; s < segs; ++s) {
        int sy = meter_y + meter_h - (s + 1) * seg_h;
        float seg_level = static_cast<float>(s + 1) / segs;
        if (level >= seg_level) {
            Color seg_c = seg_level > 0.75f ? t.red
                        : (seg_level > 0.5f ? t.accent : t.fg);
            canvas.fill_rect(meter_x + 1, sy, meter_w - 2, seg_h - 1, seg_c);
        } else {
            canvas.fill_rect(meter_x + 1, sy, meter_w - 2, seg_h - 1, t.bg);
        }
    }

    // Volume number
    char buf[16];
    if (sel) {
        std::snprintf(buf, sizeof(buf), "%.0f%%", knobs_[0].value * 100);
        canvas.text_centered(x, y + h - 24, w, buf, t.fg, 2);
    }
}

void MixerScreen::on_knob(int index, int delta) {
    if (index < 0 || index >= 4) return;
    knobs_[index].value += delta * knobs_[index].step;
    if (knobs_[index].value < knobs_[index].min)
        knobs_[index].value = knobs_[index].min;
    if (knobs_[index].value > knobs_[index].max)
        knobs_[index].value = knobs_[index].max;

    char buf[8];
    if (index == 1) {
        float pan = knobs_[1].value;
        if (pan < 0.35f) knobs_[1].value_str = "L";
        else if (pan > 0.65f) knobs_[1].value_str = "R";
        else knobs_[1].value_str = "C";
    } else {
        std::snprintf(buf, sizeof(buf), "%.2f", knobs_[index].value);
        knobs_[index].value_str = buf;
    }

    if (index == 0 || index == 3) {
        float v = (index == 0) ? knobs_[0].value : knobs_[3].value;
        channel_meters_[selected_channel_].set(v);
        master_meter_.set(knobs_[3].value);
    }

    apply_to_engine();
}

void MixerScreen::on_key(int key, bool down) {
    if (!down) return;
    switch (key) {
        case 202: selected_channel_ = (selected_channel_ - 1 + 4) % 4; break;
        case 203: selected_channel_ = (selected_channel_ + 1) % 4; break;
    }
}

void MixerScreen::on_mouse(int mx, int my, int buttons) {
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

        int strip_y = t.header_h + t.padding;
        int strip_h = 340;
        int strip_w = (t.screen_w - 2 * t.padding - 3 * t.widget_spacing) / 4;

        if (my >= strip_y && my < strip_y + strip_h) {
            for (int i = 0; i < 4; ++i) {
                int sx = t.padding + i * (strip_w + t.widget_spacing);
                if (mx >= sx && mx < sx + strip_w) {
                    selected_channel_ = i;
                    break;
                }
            }
        }
    } else {
        end_knob_drag();
    }
}

} // namespace chimera::ui
