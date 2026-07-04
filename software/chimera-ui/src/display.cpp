#include "chimera/ui/display.h"
#include "chimera/ui/theme.h"

#include <SDL.h>
#include <cstdio>

namespace chimera::ui {

static int sdl2_key_to_virtual(int sdl_key) {
    switch (sdl_key) {
        case SDLK_1: return 1;
        case SDLK_2: return 2;
        case SDLK_3: return 3;
        case SDLK_4: return 4;
        case SDLK_5: return 5;
        case SDLK_6: return 6;
        case SDLK_UP: return 200;
        case SDLK_DOWN: return 201;
        case SDLK_LEFT: return 202;
        case SDLK_RIGHT: return 203;
        case SDLK_RETURN: return 13;
        case SDLK_ESCAPE: return 27;
        case SDLK_SPACE: return 32;
        case SDLK_TAB: return 9;
        case SDLK_F5: return 269;
        default: return 0;
    }
}

Display::Display() = default;

Display::~Display() {
    shutdown();
}

bool Display::init(const char* title, int scale) {
    scale_ = scale;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    if (SDL_GetNumTouchDevices() > 0) {
        touch_mode_ = true;
        std::printf("UI: Touchscreen detected (%d devices)\n",
                     SDL_GetNumTouchDevices());
    }

    // Open first joystick if available
    int num_joysticks = SDL_NumJoysticks();
    if (num_joysticks > 0) {
        joystick_ = SDL_JoystickOpen(0);
        if (joystick_) {
            std::printf("UI: Joystick '%s' opened (%d axes, %d buttons)\n",
                       SDL_JoystickName(joystick_),
                       SDL_JoystickNumAxes(joystick_),
                       SDL_JoystickNumButtons(joystick_));
        }
    }

    auto& t = theme();
    int win_w = t.screen_w * scale_;
    int win_h = t.screen_h * scale_;

    window_ = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED,
                                SDL_WINDOWPOS_CENTERED, win_w, win_h,
                                SDL_WINDOW_SHOWN);
    if (!window_) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1,
                                    SDL_RENDERER_ACCELERATED |
                                    SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SDL_Quit();
        return false;
    }

    canvas_ = new Canvas(t.screen_w, t.screen_h);

    texture_ = SDL_CreateTexture(renderer_,
                                  SDL_PIXELFORMAT_RGB888,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  t.screen_w, t.screen_h);
    if (!texture_) {
        std::fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        shutdown();
        return false;
    }

    running_ = true;
    return true;
}

void Display::shutdown() {
    screens_.clear();
    if (joystick_) { SDL_JoystickClose(joystick_); joystick_ = nullptr; }
    if (texture_) { SDL_DestroyTexture(texture_); texture_ = nullptr; }
    if (canvas_) { delete canvas_; canvas_ = nullptr; }
    if (renderer_) { SDL_DestroyRenderer(renderer_); renderer_ = nullptr; }
    if (window_) { SDL_DestroyWindow(window_); window_ = nullptr; }
    SDL_Quit();
}

void Display::run() {
    while (running_) {
        handle_events();

        if (!screens_.empty() && active_screen_ >= 0 &&
            active_screen_ < static_cast<int>(screens_.size())) {
            screens_[active_screen_]->update();
        }

        render();
        SDL_Delay(16);
    }
}

void Display::run_one_frame() {
    handle_events();

    if (!screens_.empty() && active_screen_ >= 0 &&
        active_screen_ < static_cast<int>(screens_.size())) {
        screens_[active_screen_]->update();
    }

    render();
    SDL_Delay(16);
}

void Display::quit() {
    running_ = false;
}

void Display::push_screen(std::unique_ptr<Screen> screen) {
    screen->set_engine(engine_);
    screen->on_enter();
    screens_.push_back(std::move(screen));
    active_screen_ = static_cast<int>(screens_.size()) - 1;
}

void Display::pop_screen() {
    if (screens_.empty()) return;
    screens_.back()->on_leave();
    screens_.pop_back();
    if (!screens_.empty()) {
        active_screen_ = static_cast<int>(screens_.size()) - 1;
        screens_.back()->on_enter();
    } else {
        active_screen_ = -1;
    }
}

void Display::switch_screen(int index) {
    if (index < 0 || index >= static_cast<int>(screens_.size())) return;
    if (active_screen_ >= 0 && active_screen_ < static_cast<int>(screens_.size())) {
        screens_[active_screen_]->on_leave();
    }
    active_screen_ = index;
    screens_[active_screen_]->on_enter();
}

Screen* Display::current_screen() {
    if (screens_.empty() || active_screen_ < 0) return nullptr;
    return screens_[active_screen_].get();
}

Screen* Display::screen(int index) {
    if (index < 0 || index >= static_cast<int>(screens_.size())) return nullptr;
    return screens_[index].get();
}

void Display::handle_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                running_ = false;
                break;

            case SDL_KEYDOWN: {
                int key = event.key.keysym.sym;

                // Screen navigation: 1-5 switches screens
                if (key >= SDLK_1 && key <= SDLK_5) {
                    int idx = key - SDLK_1;
                    if (idx < static_cast<int>(screens_.size())) {
                        switch_screen(idx);
                    }
                    break;
                }
                // Tab/Shift+Tab to cycle screens
                if (key == SDLK_TAB) {
                    int mod = SDL_GetModState();
                    if (mod & KMOD_SHIFT) {
                        switch_screen((active_screen_ - 1 +
                            static_cast<int>(screens_.size())) %
                            static_cast<int>(screens_.size()));
                    } else {
                        switch_screen((active_screen_ + 1) %
                            static_cast<int>(screens_.size()));
                    }
                    break;
                }

                if (!screens_.empty() && active_screen_ >= 0) {
                    int vkey = sdl2_key_to_virtual(key);
                    if (vkey) {
                        if (menu_open_ && vkey == 27) {
                            close_menu();
                        } else {
                            screens_[active_screen_]->on_key(vkey, true);
                        }
                    }
                }
                break;
            }

            case SDL_KEYUP: {
                int key = event.key.keysym.sym;
                if (!screens_.empty() && active_screen_ >= 0) {
                    int vkey = sdl2_key_to_virtual(key);
                    if (vkey) screens_[active_screen_]->on_key(vkey, false);
                }
                break;
            }

            case SDL_MOUSEMOTION:
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                int mx = event.button.x / scale_;
                int my = event.button.y / scale_;
                int buttons = SDL_GetMouseState(nullptr, nullptr);

                if (menu_open_) {
                    if (event.type == SDL_MOUSEBUTTONUP) {
                        int cols = 2;
                        int item_w = (theme().screen_w - 2 * theme().padding - (cols - 1) * theme().padding) / cols;
                        int item_h = 60;
                        int rows = (static_cast<int>(screens_.size()) + cols - 1) / cols;
                        int total_h = rows * item_h + (rows - 1) * theme().padding;
                        int start_y = (theme().screen_h - total_h) / 2;

                        for (size_t i = 0; i < screens_.size(); ++i) {
                            int col = static_cast<int>(i) % cols;
                            int row = static_cast<int>(i) / cols;
                            int x = theme().padding + col * (item_w + theme().padding);
                            int y = start_y + row * (item_h + theme().padding);
                            if (mx >= x && mx < x + item_w && my >= y && my < y + item_h) {
                                switch_screen(static_cast<int>(i));
                                close_menu();
                                break;
                            }
                        }
                        if (menu_open_) close_menu();
                    }
                } else if (!screens_.empty() && active_screen_ >= 0) {
                    screens_[active_screen_]->on_mouse(mx, my, buttons);

                    auto& t = theme();
                    int menu_btn_x = t.screen_w - t.padding - 30;
                    int menu_btn_y = t.padding + 6;
                    int menu_btn_r = 10;
                    int dx = mx - menu_btn_x;
                    int dy = my - menu_btn_y;
                    if (event.type == SDL_MOUSEBUTTONUP &&
                        dx * dx + dy * dy <= (menu_btn_r + 4) * (menu_btn_r + 4)) {
                        toggle_menu();
                    }
                }
                break;
            }

            case SDL_MOUSEWHEEL: {
                if (!screens_.empty() && active_screen_ >= 0) {
                    screens_[active_screen_]->on_knob(0, event.wheel.y);
                }
                break;
            }

            case SDL_FINGERDOWN: {
                int mx = static_cast<int>(event.tfinger.x * theme().screen_w);
                int my = static_cast<int>(event.tfinger.y * theme().screen_h);
                touch_active_ = true;
                touch_mx_ = mx;
                touch_my_ = my;

                if (menu_open_) {
                    int cols = 2;
                    int item_w = (theme().screen_w - 2 * theme().padding - (cols - 1) * theme().padding) / cols;
                    int item_h = 60;
                    int rows = (static_cast<int>(screens_.size()) + cols - 1) / cols;
                    int total_h = rows * item_h + (rows - 1) * theme().padding;
                    int start_y = (theme().screen_h - total_h) / 2;
                    menu_selected_ = -1;
                    for (size_t i = 0; i < screens_.size(); ++i) {
                        int col = static_cast<int>(i) % cols;
                        int row = static_cast<int>(i) / cols;
                        int x = theme().padding + col * (item_w + theme().padding);
                        int y = start_y + row * (item_h + theme().padding);
                        if (mx >= x && mx < x + item_w && my >= y && my < y + item_h) {
                            menu_selected_ = static_cast<int>(i);
                            break;
                        }
                    }
                } else {
                    auto& t = theme();
                    int menu_btn_x = t.screen_w - t.padding - 30;
                    int menu_btn_y = t.padding + 6;
                    int menu_btn_r = 10;
                    int dx = mx - menu_btn_x;
                    int dy = my - menu_btn_y;
                    if (dx * dx + dy * dy <= (menu_btn_r + 4) * (menu_btn_r + 4)) {
                        toggle_menu();
                    }
                }
                break;
            }

            case SDL_FINGERUP: {
                touch_active_ = false;

                if (menu_open_) {
                    if (menu_selected_ >= 0 && menu_selected_ < static_cast<int>(screens_.size())) {
                        switch_screen(menu_selected_);
                    }
                    close_menu();
                }
                break;
            }

            case SDL_FINGERMOTION: {
                if (!touch_active_) break;
                touch_mx_ = static_cast<int>(event.tfinger.x * theme().screen_w);
                touch_my_ = static_cast<int>(event.tfinger.y * theme().screen_h);

                if (menu_open_) {
                    int cols = 2;
                    int item_w = (theme().screen_w - 2 * theme().padding - (cols - 1) * theme().padding) / cols;
                    int item_h = 60;
                    int rows = (static_cast<int>(screens_.size()) + cols - 1) / cols;
                    int total_h = rows * item_h + (rows - 1) * theme().padding;
                    int start_y = (theme().screen_h - total_h) / 2;
                    menu_selected_ = -1;
                    for (size_t i = 0; i < screens_.size(); ++i) {
                        int col = static_cast<int>(i) % cols;
                        int row = static_cast<int>(i) / cols;
                        int x = theme().padding + col * (item_w + theme().padding);
                        int y = start_y + row * (item_h + theme().padding);
                        if (touch_mx_ >= x && touch_mx_ < x + item_w &&
                            touch_my_ >= y && touch_my_ < y + item_h) {
                            menu_selected_ = static_cast<int>(i);
                            break;
                        }
                    }
                }
                break;
            }

            case SDL_JOYAXISMOTION: {
                handle_joystick_axis(event.jaxis.axis, event.jaxis.value);
                break;
            }

            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP: {
                handle_joystick_button(event.jbutton.button,
                                       event.jbutton.state == SDL_PRESSED);
                break;
            }
        }
    }
}

void Display::handle_joystick_axis(int axis, int value) {
    if (!joystick_) return;
    if (std::abs(value) < AXIS_DEADZONE) return;

    if (!screens_.empty() && active_screen_ >= 0) {
        // Axis 0: horizontal → knob 1 (or left/right)
        // Axis 1: vertical → knob 0 (or up/down)
        int dir = (value > 0) ? 1 : -1;
        if (axis == 0) {
            screens_[active_screen_]->on_key(dir > 0 ? 203 : 202, true);
            screens_[active_screen_]->on_key(dir > 0 ? 203 : 202, false);
        } else if (axis == 1) {
            screens_[active_screen_]->on_key(dir > 0 ? 201 : 200, true);
            screens_[active_screen_]->on_key(dir > 0 ? 201 : 200, false);
        } else if (axis == 2) {
            screens_[active_screen_]->on_knob(0, dir);
        } else if (axis == 3) {
            screens_[active_screen_]->on_knob(1, dir);
        } else if (axis == 4) {
            screens_[active_screen_]->on_knob(2, dir);
        } else if (axis == 5) {
            screens_[active_screen_]->on_knob(3, dir);
        }
    }
}

void Display::handle_joystick_button(int button, bool down) {
    if (!down) return;
    if (!joystick_) return;

    // Button 0-4 → screen navigation (like keys 1-5)
    if (button >= 0 && button < static_cast<int>(screens_.size())) {
        switch_screen(button);
        return;
    }

    // Button 9 (usually Start) → toggle menu
    if (button == 9) {
        toggle_menu();
        return;
    }

    // Other mappings
    int vkey = 0;
    switch (button) {
        case 4: vkey = 202; break;  // Left
        case 5: vkey = 203; break;  // Right
        case 6: vkey = 200; break;  // Up
        case 7: vkey = 201; break;  // Down
        case 8: vkey = 13;  break;  // Enter/action
        case 9: vkey = 27;  break;  // Escape/back
    }

    if (vkey && !screens_.empty() && active_screen_ >= 0) {
        screens_[active_screen_]->on_key(vkey, true);
        screens_[active_screen_]->on_key(vkey, false);
    }
}

void Display::render() {
    auto& t = theme();
    canvas_->clear(t.bg);

    if (!screens_.empty() && active_screen_ >= 0 &&
        active_screen_ < static_cast<int>(screens_.size())) {
        screens_[active_screen_]->render(*canvas_, true);
    } else {
        canvas_->text_centered(0, t.screen_h / 2, t.screen_w,
                               "Chimera", t.fg, 3);
        canvas_->text_centered(0, t.screen_h / 2 + 30, t.screen_w,
                               "no screen loaded", t.fg_dim);
    }

    if (menu_open_) {
        draw_screen_menu(*canvas_);
    }

    // Screen indicator bar
    int bar_y = t.screen_h - 4;
    if (!screens_.empty()) {
        int dot_w = t.screen_w / static_cast<int>(screens_.size());
        for (size_t i = 0; i < screens_.size(); ++i) {
            int dx = static_cast<int>(i) * dot_w;
            canvas_->fill_rect(dx, bar_y, dot_w - 2, 4,
                static_cast<int>(i) == active_screen_ ? t.fg : t.muted);
        }
    }

    // Touch feedback indicator
    if (touch_active_) {
        canvas_->circle(touch_mx_, touch_my_, 6, t.fg_bright, true);
        canvas_->circle(touch_mx_, touch_my_, 8, t.fg_bright, false);
    }

    SDL_UpdateTexture(texture_, nullptr, canvas_->pixels(),
                      t.screen_w * sizeof(uint32_t));
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
}

void Display::draw_screen_menu(Canvas& canvas) {
    auto& t = theme();

    // Dimmed background overlay
    canvas.fill_rect(0, 0, t.screen_w, t.screen_h,
                     Color(0, 0, 0, 200));

    int cols = 2;
    int rows = (screens_.size() + cols - 1) / cols;
    int item_w = (t.screen_w - 2 * t.padding - (cols - 1) * t.padding) / cols;
    int item_h = 60;
    int total_h = rows * item_h + (rows - 1) * t.padding;
    int start_y = (t.screen_h - total_h) / 2;

    for (size_t i = 0; i < screens_.size(); ++i) {
        int col = static_cast<int>(i) % cols;
        int row = static_cast<int>(i) / cols;
        int x = t.padding + col * (item_w + t.padding);
        int y = start_y + row * (item_h + t.padding);

        bool active = (static_cast<int>(i) == active_screen_);
        bool selected = (static_cast<int>(i) == menu_selected_);

        Color bg_c = active ? Color(40, 50, 20, 255) : t.bg;
        Color border_c = selected ? t.fg_bright : (active ? t.fg : t.fg_dim);

        canvas.fill_rect(x, y, item_w, item_h, bg_c);
        canvas.rect(x, y, item_w, item_h, border_c, 2);

        if (active) {
            canvas.fill_rect(x + 2, y + 2, item_w - 4, 3, t.fg);
        }

        canvas.text_centered(x, y + item_h / 2 - 10, item_w,
                             screens_[i]->name().c_str(),
                             active ? t.fg_bright : t.fg, 2);

        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", static_cast<int>(i) + 1);
        canvas.text_right(x + 4, y + item_h / 2 - 10, item_w - 8,
                          buf, t.fg_dim, 1);
    }

    canvas.text_centered(0, t.screen_h - t.footer_h + 4, t.screen_w,
                         "TAP SCREEN TO SELECT  |  ESC / MENU TO CLOSE",
                         t.fg_dim, 1);
}

} // namespace chimera::ui
