#pragma once

#include "canvas.h"
#include "screen.h"
#include <memory>
#include <vector>

namespace chimera { class Engine; }

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
struct _SDL_Joystick;

namespace chimera::ui {

class Display {
public:
    Display();
    ~Display();

    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    bool init(const char* title, int scale = 2);
    void shutdown();

    void run();
    void run_one_frame();
    bool is_running() const { return running_; }
    bool touch_mode() const { return touch_mode_; }
    bool touch_active() const { return touch_active_; }
    void quit();

    void set_engine(chimera::Engine* e) { engine_ = e; }

    void push_screen(std::unique_ptr<Screen> screen);
    void pop_screen();
    void switch_screen(int index);

    Screen* current_screen();
    Screen* screen(int index);
    int screen_count() const { return static_cast<int>(screens_.size()); }

    bool menu_open() const { return menu_open_; }
    void toggle_menu() { menu_open_ = !menu_open_; }
    void close_menu() { menu_open_ = false; }

private:
    SDL_Window* window_{nullptr};
    SDL_Renderer* renderer_{nullptr};
    SDL_Texture* texture_{nullptr};
    Canvas* canvas_{nullptr};
    chimera::Engine* engine_ = nullptr;
    int scale_{2};
    bool running_{false};

    bool touch_mode_{false};

    // Joystick/arcade button support (opaque SDL_Joystick*)
    _SDL_Joystick* joystick_{nullptr};
    static constexpr int AXIS_DEADZONE = 8000;

    int active_screen_{0};
    std::vector<std::unique_ptr<Screen>> screens_;

    bool touch_active_{false};
    int touch_mx_{0};
    int touch_my_{0};

    bool menu_open_{false};
    int menu_selected_{-1};

    void handle_events();
    void handle_joystick_axis(int axis, int value);
    void handle_joystick_button(int button, bool down);
    void render();
    void draw_screen_menu(Canvas& canvas);
};

} // namespace chimera::ui
