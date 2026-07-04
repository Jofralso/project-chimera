#include "chimera/ui/screens/drum_screen.h"

#include <SDL_events.h>
#include <cmath>
#include <cstdio>

namespace chimera::ui {

DrumScreen::DrumScreen()
    : Screen("DRUM MACHINE")
{
    knobs_[0] = {"VOLUME", "0.80", 0.8f, 0, 1, 0.01f, true};
    knobs_[1] = {"DECAY", "0.50", 0.5f, 0, 1, 0.01f, true};
    knobs_[2] = {"TUNE", "0.00", 0.0f, 0, 1, 0.01f, false};
    knobs_[3] = {"PAN", "C", 0.5f, 0, 1, 0.01f, false};
}

void DrumScreen::update() {
    pulse_.update();
    for (int i = 0; i < 4; ++i) {
        pad_flash_[i].update();
        pad_levels_[i].update();
    }
    meter_.update();
}

void DrumScreen::render(Canvas& canvas, bool active) {
    auto& t = theme();

    draw_header(canvas, name_, "M3 - SAMPLER");

    draw_pad_grid(canvas, t.padding, t.header_h + t.padding,
                  t.screen_w - 2 * t.padding, 180);

    int info_y = t.header_h + t.padding + 200;

    // Animated pad info display
    char buf[32];
    std::snprintf(buf, sizeof(buf), "PAD %d", selected_pad_ + 1);
    canvas.text(t.padding, info_y, buf, pad_flash_[selected_pad_].value() > 0.1f
                    ? t.fg_bright : t.fg, 2);

    // Waveform / sample display area
    int wave_y = info_y + 30;
    int wave_h = 80;
    canvas.rect(t.padding, wave_y, t.screen_w - 2 * t.padding, wave_h, t.fg_dim);

    if (engine_) {
        auto* n = engine_->graph().find_node_by_class("builtin.drum");
        if (n) {
            auto* drum = reinterpret_cast<chimera::DrumNode*>(n);
            if (drum->is_pad_loaded(static_cast<uint32_t>(selected_pad_))) {
                float pv = pad_flash_[selected_pad_].value();
                Color c = pv > 0.1f ? t.fg_bright : t.fg;
                canvas.text_centered(0, wave_y + wave_h / 2 - 8, t.screen_w,
                                     "[sample loaded]", c);

                // Draw volume bar that pulses on trigger
                int bw = t.screen_w - 2 * t.padding - 16;
                int by = wave_y + wave_h - 16;
                float lev = pad_levels_[selected_pad_].value();
                canvas.fill_rect(t.padding + 8, by,
                                 static_cast<int>(bw * lev), 8, c);
            } else {
                float p = pulse_.value();
                Color c = Color(static_cast<uint8_t>(80 * p),
                                static_cast<uint8_t>(100 * p),
                                static_cast<uint8_t>(40 * p), 255);
                canvas.text_centered(0, wave_y + wave_h / 2 - 8, t.screen_w,
                                     "[no sample]", c);
            }

            // Animated level meter on right side
            float meter_val = meter_.value();
            Color mc = meter_val > 0.7f ? t.red : t.fg;
            canvas.level_meter(t.screen_w - t.padding - 12, wave_y + 4,
                               8, wave_h - 8, meter_val, mc);
        }
    } else {
        canvas.text_centered(0, wave_y + wave_h / 2 - 8, t.screen_w,
                             "[no engine]", t.fg_dim);
    }

    int info_y2 = wave_y + wave_h + 8;
    canvas.text(t.padding, info_y2, "SAMPLE", t.fg_dim);
    canvas.text(t.padding, info_y2 + 14, "-- empty --",
                pad_flash_[selected_pad_].value() > 0.1f ? t.accent : t.fg);

    // BPM display
    float bpm = 130.0f;
    if (engine_) bpm = engine_->sequencer().bpm();
    std::snprintf(buf, sizeof(buf), "%.0f BPM", bpm);
    canvas.text_right(t.padding, info_y2, t.screen_w - 2 * t.padding, buf, t.fg);

    draw_knobs(canvas, knobs_, 4);

    apply_to_engine();
}

void DrumScreen::on_knob(int index, int delta) {
    if (index < 0 || index >= 4) return;
    knobs_[index].value += delta * knobs_[index].step;
    if (knobs_[index].value < knobs_[index].min)
        knobs_[index].value = knobs_[index].min;
    if (knobs_[index].value > knobs_[index].max)
        knobs_[index].value = knobs_[index].max;

    char buf[8];
    if (index == 3) {
        float pan = knobs_[3].value;
        if (pan < 0.35f) knobs_[3].value_str = "L";
        else if (pan > 0.65f) knobs_[3].value_str = "R";
        else knobs_[3].value_str = "C";
    } else {
        std::snprintf(buf, sizeof(buf), "%.2f", knobs_[index].value);
        knobs_[index].value_str = buf;
    }

    apply_to_engine();
}

void DrumScreen::on_key(int key, bool down) {
    if (!down) return;
    switch (key) {
        case 202: selected_pad_ = (selected_pad_ - 1 + 4) % 4; break;
        case 203: selected_pad_ = (selected_pad_ + 1) % 4; break;
        case 13:  // Enter — trigger pad
            pad_flash_[selected_pad_].trigger();
            pad_levels_[selected_pad_].set(1.0f);
            meter_.set(1.0f);
            if (engine_) {
                auto* n = engine_->graph().find_node_by_class("builtin.drum");
                if (n) {
                    reinterpret_cast<chimera::DrumNode*>(n)->trigger(
                        static_cast<uint32_t>(selected_pad_), 1.0f);
                }
            }
            break;
    }
}

void DrumScreen::on_mouse(int mx, int my, int buttons) {
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

        int pad_grid_x = t.padding;
        int pad_grid_y = t.header_h + t.padding;
        int pad_grid_w = t.screen_w - 2 * t.padding;
        int pad_grid_h = 180;
        int grid_cols = 4;
        int pad_w = (pad_grid_w - (grid_cols + 1) * t.padding) / grid_cols;

        if (mx >= pad_grid_x && mx < pad_grid_x + pad_grid_w &&
            my >= pad_grid_y && my < pad_grid_y + pad_grid_h) {
            int col = (mx - pad_grid_x - t.padding) / (pad_w + t.padding);
            if (col >= 0 && col < 4) {
                selected_pad_ = col;
                pad_flash_[col].trigger();
                pad_levels_[col].set(1.0f);
                meter_.set(1.0f);
                if (engine_) {
                    auto* n = engine_->graph().find_node_by_class("builtin.drum");
                    if (n) {
                        reinterpret_cast<chimera::DrumNode*>(n)->trigger(
                            static_cast<uint32_t>(col), 1.0f);
                    }
                }
            }
        }
    } else {
        end_knob_drag();
    }
}

void DrumScreen::draw_pad_grid(Canvas& canvas, int x, int y, int w, int h) {
    auto& t = theme();
    int grid_cols = 4;
    int pad_w = (w - (grid_cols + 1) * t.padding) / grid_cols;

    for (int col = 0; col < grid_cols; ++col) {
        int px = x + col * (pad_w + t.padding) + t.padding;
        bool sel = (col == selected_pad_);
        float flash = pad_flash_[col].value();
        float pulse = pulse_.value();

        Color c;
        if (flash > 0.1f) {
            uint8_t intensity = static_cast<uint8_t>(170 + 85 * flash);
            c = Color(intensity, static_cast<uint8_t>(204 * flash),
                      85, 255);
        } else if (sel) {
            c = Color(static_cast<uint8_t>(170 * pulse),
                      static_cast<uint8_t>(204 * pulse),
                      static_cast<uint8_t>(85 * pulse), 255);
        } else {
            c = t.fg_dim;
        }

        canvas.rect(px, y, pad_w, h - t.padding, c);

        if (sel) {
            canvas.fill_rect(px + 2, y + 2, pad_w - 4, h - t.padding - 4, t.bg);
            canvas.rect(px + 2, y + 2, pad_w - 4, h - t.padding - 4, c);
        }

        char buf[8];
        std::snprintf(buf, sizeof(buf), "%d", col + 1);
        canvas.text_centered(px, y + h / 2 - 14 - t.padding / 2,
                            pad_w, buf, c, 2);

        if (engine_) {
            auto* n = engine_->graph().find_node_by_class("builtin.drum");
            if (n) {
                auto* drum = reinterpret_cast<chimera::DrumNode*>(n);
                if (drum->is_pad_loaded(static_cast<uint32_t>(col))) {
                    canvas.text_centered(px, y + h / 2 + 4 - t.padding / 2,
                                        pad_w, "WAV", t.fg_dim);
                } else {
                    canvas.text_centered(px, y + h / 2 + 4 - t.padding / 2,
                                        pad_w, "---", t.fg_dim);
                }
            }
        }
    }
}

} // namespace chimera::ui
