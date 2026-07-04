#include "chimera/ui/screens/sequencer_screen.h"
#include <SDL_events.h>
#include <cstdio>
#include <cmath>

namespace chimera::ui {

SequencerScreen::SequencerScreen()
    : Screen("SEQUENCER")
{
    knobs_[0] = {"BPM", "130", 0.54f, 0, 1, 0.01f, true};
    knobs_[1] = {"SWING", "0", 0.0f, 0, 1, 0.01f, false};
    knobs_[2] = {"PATTERN", "1/4", 0.0f, 0, 3, 1.0f, true};
    knobs_[3] = {"RESOLUTION", "1/16", 0.0f, 0, 3, 1.0f, false};
}

void SequencerScreen::update() {
    pulse_.update();
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 16; ++c)
            flash_[r][c].update();
}

void SequencerScreen::render(Canvas& canvas, bool active) {
    auto& t = theme();

    draw_header(canvas, name_, "M5 - PATTERN");

    int grid_y = t.header_h + t.padding;
    int track_h = t.step_size + t.step_gap;
    const char* labels[] = {"KICK", "SNARE", "HAT", "BASS"};
    for (int i = 0; i < 4; ++i) {
        int ly = grid_y + i * track_h;
        float pv = pulse_.value();
        bool is_active_track = (cursor_y_ == i);
        Color c = is_active_track
            ? Color(static_cast<uint8_t>(170 * pv),
                    static_cast<uint8_t>(204 * pv),
                    static_cast<uint8_t>(85 * pv), 255)
            : t.fg_dim;
        canvas.text(t.padding, ly + 2, labels[i], c);
    }

    draw_step_grid(canvas, t.padding + 40, grid_y,
                   t.screen_w - t.padding * 2 - 40, 4 * track_h);

    int playhead_y = grid_y + 4 * track_h + t.padding;

    // Animated transport position
    uint32_t step = 0;
    if (engine_) step = engine_->sequencer().current_step();
    float bpm = engine_ ? engine_->sequencer().bpm() : 130.0f;

    char buf[64];
    std::snprintf(buf, sizeof(buf), "STEP: %2d/16", step + 1);
    canvas.text(t.padding, playhead_y, buf, t.fg);

    int pat = static_cast<int>(knobs_[2].value) + 1;
    std::snprintf(buf, sizeof(buf), "PATTERN %d/4  %.0f BPM", pat, bpm);
    canvas.text_right(t.padding, playhead_y, t.screen_w - 2 * t.padding, buf, t.fg);

    int edit_y = playhead_y + 24;

    // Animated edit info box
    float bevel = pulse_.value();
    Color box_c = Color(static_cast<uint8_t>(60 * bevel),
                        static_cast<uint8_t>(60 * bevel),
                        static_cast<uint8_t>(60 * bevel), 255);
    canvas.fill_rect(t.padding, edit_y, t.screen_w - 2 * t.padding, 40, box_c);
    canvas.rect(t.padding, edit_y, t.screen_w - 2 * t.padding, 40, t.fg_dim);

    bool active_step = false;
    float step_vel = 0.0f;
    if (engine_) {
        auto& sd = engine_->sequencer().step(
            static_cast<uint32_t>(cursor_y_),
            static_cast<uint32_t>(cursor_x_));
        active_step = sd.active;
        step_vel = sd.velocity;
    }

    float fl = flash_[cursor_y_][cursor_x_].value();
    if (fl > 0.1f) {
        std::snprintf(buf, sizeof(buf), "TRACK %d  STEP %d  TOGGLED!",
                      cursor_y_ + 1, cursor_x_ + 1);
        canvas.text_centered(0, edit_y + 12, t.screen_w, buf,
                             fl > 0.5f ? t.fg_bright : t.fg);
    } else if (active_step) {
        std::snprintf(buf, sizeof(buf), "TRACK %d  STEP %d  VEL=%.0f%%",
                      cursor_y_ + 1, cursor_x_ + 1, step_vel * 100);
        canvas.text_centered(0, edit_y + 12, t.screen_w, buf, t.fg);
    } else {
        std::snprintf(buf, sizeof(buf), "TRACK %d  STEP %d  [EMPTY]",
                      cursor_y_ + 1, cursor_x_ + 1);
        canvas.text_centered(0, edit_y + 12, t.screen_w, buf, t.fg_dim);
    }

    draw_knobs(canvas, knobs_, 4);

    apply_to_engine();
}

void SequencerScreen::draw_step_grid(Canvas& canvas, int x, int y,
                                      int w, int h) {
    auto& t = theme();
    int cols = 16;
    int step_w = (w - (cols - 1) * t.step_gap) / cols;
    int step_h = t.step_size;

    // Beat lines (every 4 steps)
    for (int i = 0; i < 4; ++i) {
        int bx = x + i * 4 * (step_w + t.step_gap);
        canvas.vline(bx - 1, y, h, t.fg_dim);
    }

    uint32_t play_step = 0;
    if (engine_) play_step = engine_->sequencer().current_step();

    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 16; ++col) {
            int sx = x + col * (step_w + t.step_gap);
            int sy = y + row * (step_h + t.step_gap);

            bool active = false;
            if (engine_) {
                active = engine_->sequencer().step(
                    static_cast<uint32_t>(row),
                    static_cast<uint32_t>(col)).active;
            }
            bool cursor = (row == cursor_y_ && col == cursor_x_);
            bool is_playing = (static_cast<uint32_t>(col) == play_step);

            Color fill_c = t.bg;
            float fl = flash_[row][col].value();

            if (is_playing && active) {
                fill_c = t.fg_bright;
            } else if (is_playing) {
                float pv = pulse_.value();
                fill_c = Color(static_cast<uint8_t>(60 * pv),
                               static_cast<uint8_t>(60 * pv),
                               static_cast<uint8_t>(60 * pv), 255);
            } else if (fl > 0.1f) {
                uint8_t v = static_cast<uint8_t>(120 + 135 * fl);
                fill_c = Color(static_cast<uint8_t>(v * 170 / 255),
                               static_cast<uint8_t>(v * 204 / 255),
                               static_cast<uint8_t>(v * 85 / 255), 255);
            } else if (active) {
                fill_c = t.fg;
            }

            if (active || is_playing || fl > 0.1f)
                canvas.fill_rect(sx, sy, step_w, step_h, fill_c);

            Color border = cursor ? t.fg_bright
                         : (active ? t.fg : t.muted);
            if (is_playing) border = t.white;

            int bw = cursor ? 2 : 1;
            canvas.rect(sx, sy, step_w, step_h, border, bw);

            // Step number on first row
            if (row == 0) {
                char buf[4];
                std::snprintf(buf, sizeof(buf), "%d", col + 1);
                canvas.text_centered(sx, sy + step_h + 2, step_w, buf, t.fg_dim);
            }
        }
    }
}

void SequencerScreen::on_knob(int index, int delta) {
    if (index < 0 || index >= 4) return;
    knobs_[index].value += delta * knobs_[index].step;
    if (knobs_[index].value < knobs_[index].min)
        knobs_[index].value = knobs_[index].min;
    if (knobs_[index].value > knobs_[index].max)
        knobs_[index].value = knobs_[index].max;

    if (index == 2) {
        int p = static_cast<int>(knobs_[2].value) + 1;
        char buf[12];
        std::snprintf(buf, sizeof(buf), "%d/4", p);
        knobs_[2].value_str = buf;
    }

    apply_to_engine();
}

void SequencerScreen::on_key(int key, bool down) {
    if (!down) return;
    switch (key) {
        case 202: cursor_x_ = (cursor_x_ - 1 + 16) % 16; break;
        case 203: cursor_x_ = (cursor_x_ + 1) % 16; break;
        case 200: cursor_y_ = (cursor_y_ - 1 + 4) % 4; break;
        case 201: cursor_y_ = (cursor_y_ + 1) % 4; break;
        case 13:
            if (engine_) {
                auto& seq = engine_->sequencer();
                bool cur = seq.step(
                    static_cast<uint32_t>(cursor_y_),
                    static_cast<uint32_t>(cursor_x_)).active;
                seq.set_step(static_cast<uint32_t>(cursor_y_),
                            static_cast<uint32_t>(cursor_x_),
                            !cur, 0.8f);
                flash_[cursor_y_][cursor_x_].trigger();
            }
            break;
    }
}

void SequencerScreen::on_mouse(int mx, int my, int buttons) {
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
    } else {
        end_knob_drag();
    }

    int grid_x = t.padding + 40;
    int grid_y = t.header_h + t.padding;
    int cols = 16;
    int step_w = (t.screen_w - t.padding * 2 - 40 - (cols - 1) * t.step_gap) / cols;
    int step_h = t.step_size;

    int col = (mx - grid_x) / (step_w + t.step_gap);
    int row = (my - grid_y) / (step_h + t.step_gap);

    if (col >= 0 && col < 16 && row >= 0 && row < 4) {
        cursor_x_ = col;
        cursor_y_ = row;
        if (buttons & SDL_BUTTON_LMASK) {
            if (engine_) {
                auto& seq = engine_->sequencer();
                bool cur = seq.step(
                    static_cast<uint32_t>(row),
                    static_cast<uint32_t>(col)).active;
                seq.set_step(static_cast<uint32_t>(row),
                            static_cast<uint32_t>(col),
                            !cur, 0.8f);
                flash_[row][col].trigger();
            }
        }
    }
}

} // namespace chimera::ui
