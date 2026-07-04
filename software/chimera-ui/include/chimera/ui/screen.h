#pragma once

#include "canvas.h"
#include <string>
#include <vector>

namespace chimera { class Engine; }

namespace chimera::ui {

struct KnobState {
    std::string label;
    std::string value_str;
    float value{0.0f};
    float min{0.0f};
    float max{1.0f};
    float step{0.01f};
    bool active{false};
};

// Animation helper — exponential moving average for smooth transitions
struct AnimFloat {
    float current{0.0f};
    float target{0.0f};
    float speed{0.1f};

    void set(float t) { target = t; }
    void snap(float v) { current = target = v; }
    void update() {
        current += (target - current) * speed;
        if (std::abs(current - target) < 0.001f) current = target;
    }
    float value() const { return current; }
};

// Pulse/blink helper
struct PulseState {
    float phase{0.0f};
    float speed{0.05f};
    float min_v{0.3f};
    float max_v{1.0f};

    void update() { phase += speed; if (phase > 6.28318f) phase -= 6.28318f; }
    float value() const { return min_v + (max_v - min_v) * (0.5f + 0.5f * std::sin(phase)); }
};

// Flash state — fades after trigger
struct FlashState {
    float intensity{0.0f};

    void trigger() { intensity = 1.0f; }
    void update() { intensity *= 0.92f; if (intensity < 0.01f) intensity = 0.0f; }
    float value() const { return intensity; }
};

class Screen {
public:
    Screen(const std::string& name) : name_(name) {}
    virtual ~Screen() = default;

    virtual void update() {}
    virtual void render(Canvas& canvas, bool active) = 0;
    virtual void on_enter() {}
    virtual void on_leave() {}

    virtual void on_key(int key, bool down) {}
    virtual void on_mouse(int mx, int my, int buttons) {}
    virtual void on_knob(int index, int delta) {}

    const std::string& name() const { return name_; }

    void set_engine(chimera::Engine* e) { engine_ = e; }
    chimera::Engine* engine() { return engine_; }

    virtual const KnobState* knobs() const { return nullptr; }
    virtual KnobState* knobs() { return nullptr; }
    virtual int knob_count() const { return 0; }

    void set_header(const Canvas& canvas);
    void set_footer(Canvas& canvas);

protected:
    std::string name_;
    chimera::Engine* engine_ = nullptr;

    PulseState pulse_;

    void draw_header(Canvas& canvas, const std::string& title,
                     const std::string& subtitle = "");
    void draw_knobs(Canvas& canvas, const KnobState* knobs, int count);
    void draw_footer(Canvas& canvas, const std::string& hint = "");

    int hit_test_knob(int mx, int my) const;
    void handle_knob_drag(int my);
    void end_knob_drag();

    int drag_knob_{-1};
    int drag_start_y_{0};
    float drag_start_value_{0.0f};

private:
    void draw_knob(Canvas& canvas, int x, int y, const KnobState& k);
};

} // namespace chimera::ui
