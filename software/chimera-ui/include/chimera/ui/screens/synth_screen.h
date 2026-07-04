#pragma once

#include "../screen.h"
#include <chimera/engine.h>
#include <chimera/nodes/synth_node.h>
#include <cstdio>

namespace chimera::ui {

class SynthScreen : public Screen {
public:
    SynthScreen()
        : Screen("SYNTHESIZER")
    {
        knobs_[0] = {"WAVEFORM", "saw", 0.25f, 0, 1, 0.01f, true};
        knobs_[1] = {"CUTOFF", "0.70", 0.7f, 0, 1, 0.01f, true};
        knobs_[2] = {"RESONANCE", "0.30", 0.3f, 0, 1, 0.01f, false};
        knobs_[3] = {"ENV AMT", "0.40", 0.4f, 0, 1, 0.01f, false};
    }

    void update() override {
        pulse_.update();
        anim_adsr_.update();
    }

    void render(Canvas& canvas, bool active) override;
    void on_knob(int index, int delta) override;
    const KnobState* knobs() const override { return knobs_; }
    int knob_count() const override { return 4; }

private:
    KnobState knobs_[4];

    AnimFloat anim_adsr_{0.0f, 0.0f, 0.05f};

    void apply_to_engine();
    void draw_waveform_preview(Canvas& canvas, int x, int y, int w, int h);
};

inline void SynthScreen::apply_to_engine() {
    if (!engine_) return;
    auto* n = engine_->graph().find_node_by_class("builtin.synth");
    if (!n) return;
    auto* synth = reinterpret_cast<chimera::SynthNode*>(n);

    int wf = static_cast<int>(knobs_[0].value * 4);
    if (wf > 3) wf = 3;
    synth->params().waveform = static_cast<chimera::Waveform>(wf);
    synth->params().filter_cutoff = knobs_[1].value;
    synth->params().filter_resonance = knobs_[2].value;
    synth->params().filter_env_amount = knobs_[3].value;

    anim_adsr_.set(knobs_[1].value);
}

} // namespace chimera::ui
