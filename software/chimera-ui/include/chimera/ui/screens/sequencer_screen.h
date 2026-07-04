#pragma once

#include "../screen.h"
#include <chimera/engine.h>

namespace chimera::ui {

class SequencerScreen : public Screen {
public:
    SequencerScreen();

    void update() override;
    void render(Canvas& canvas, bool active) override;
    void on_knob(int index, int delta) override;
    void on_key(int key, bool down) override;
    void on_mouse(int mx, int my, int buttons) override;
    const KnobState* knobs() const override { return knobs_; }
    int knob_count() const override { return 4; }

private:
    KnobState knobs_[4];
    int cursor_x_ = 0;
    int cursor_y_ = 0;

    FlashState flash_[4][16]{};

    void apply_to_engine();
    void draw_step_grid(Canvas& canvas, int x, int y, int w, int h);
};

inline void SequencerScreen::apply_to_engine() {
    if (!engine_) return;
    auto& seq = engine_->sequencer();
    float bpm = 60.0f + knobs_[0].value * 180.0f;
    seq.set_bpm(bpm);
}

} // namespace chimera::ui
