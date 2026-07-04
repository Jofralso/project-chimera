#pragma once

#include "../screen.h"
#include <chimera/engine.h>
#include <chimera/nodes/master_output.h>

namespace chimera::ui {

class MixerScreen : public Screen {
public:
    MixerScreen();

    void update() override;
    void render(Canvas& canvas, bool active) override;
    void on_knob(int index, int delta) override;
    void on_key(int key, bool down) override;
    void on_mouse(int mx, int my, int buttons) override;
    const KnobState* knobs() const override { return knobs_; }
    KnobState* knobs() override { return knobs_; }
    int knob_count() const override { return 4; }

private:
    KnobState knobs_[4];
    int selected_channel_ = 0;

    AnimFloat channel_meters_[4]{};
    AnimFloat master_meter_{0.0f, 0.0f, 0.2f};

    void apply_to_engine();
    void draw_channel_strip(Canvas& canvas, int x, int y, int w, int h,
                            int channel, const char* name, float level);
};

inline void MixerScreen::apply_to_engine() {
    if (!engine_) return;
    auto* n = engine_->graph().find_node_by_class("builtin.master_output");
    if (!n) return;
    auto* master = reinterpret_cast<chimera::MasterOutputNode*>(n);
    master->set_volume(knobs_[3].value);
}

} // namespace chimera::ui
