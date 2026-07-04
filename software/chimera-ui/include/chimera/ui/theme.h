#pragma once

#include <cstdint>

namespace chimera::ui {

struct Color {
    uint8_t r, g, b, a;
    constexpr Color() : r(0), g(0), b(0), a(255) {}
    constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}
};

struct Theme {
    // OP-1 LCD classic green
    Color bg{0, 0, 0, 255};
    Color fg{170, 204, 85, 255};      // OP-1 green
    Color fg_dim{80, 100, 40, 255};   // dim green
    Color fg_bright{210, 240, 120, 255};
    Color white{255, 255, 255, 255};
    Color accent{255, 170, 0, 255};   // orange accent
    Color red{255, 80, 80, 255};
    Color blue{80, 160, 255, 255};
    Color muted{60, 60, 60, 255};

    // Layout constants
    int screen_w{320};
    int screen_h{640};
    int header_h{48};
    int footer_h{80};
    int knob_area_h{120};
    int padding{8};
    int widget_spacing{4};
    int title_size{12};
    int body_size{10};
    int small_size{8};

    // Knob
    int knob_r{20};
    int knob_stroke{2};

    // Slider
    int slider_w{8};
    int slider_h{120};

    // Step sequencer grid
    int step_size{16};
    int step_gap{2};
};

inline const Theme& theme() {
    static Theme t;
    return t;
}

} // namespace chimera::ui
