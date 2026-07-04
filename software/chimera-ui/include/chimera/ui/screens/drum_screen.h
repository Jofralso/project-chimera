#pragma once

#include "../screen.h"
#include <chimera/engine.h>
#include <chimera/nodes/drum_node.h>

namespace chimera::ui {

class DrumScreen : public Screen {
public:
    DrumScreen();

    void update() override;
    void render(Canvas& canvas, bool active) override;
    void on_knob(int index, int delta) override;
    void on_key(int key, bool down) override;
    const KnobState* knobs() const override { return knobs_; }
    int knob_count() const override { return 4; }

private:
    KnobState knobs_[4];
    int selected_pad_ = 0;

    FlashState pad_flash_[4];
    AnimFloat pad_levels_[4]{};
    AnimFloat meter_{0.0f, 0.0f, 0.3f};

    void apply_to_engine();
    void draw_pad_grid(Canvas& canvas, int x, int y, int w, int h);
};

inline void DrumScreen::apply_to_engine() {
    if (!engine_) return;
    auto* n = engine_->graph().find_node_by_class("builtin.drum");
    if (!n) return;
    auto* drum = reinterpret_cast<chimera::DrumNode*>(n);

    float pan = (knobs_[3].value - 0.5f) * 2.0f;
    drum->set_pad_level(static_cast<uint32_t>(selected_pad_), knobs_[0].value);
    drum->set_pad_decay(static_cast<uint32_t>(selected_pad_), knobs_[1].value);
    drum->set_pad_pan(static_cast<uint32_t>(selected_pad_), pan);
    drum->set_pad_tune(static_cast<uint32_t>(selected_pad_),
                       (knobs_[2].value - 0.5f) * 12.0f);
}

} // namespace chimera::ui
