#pragma once

#include "../screen.h"
#include <chimera/engine.h>
#include <chimera/session.h>
#include <string>
#include <vector>

namespace chimera::ui {

class BrowserScreen : public Screen {
public:
    BrowserScreen();

    void update() override;
    void render(Canvas& canvas, bool active) override;
    void on_knob(int index, int delta) override;
    void on_key(int key, bool down) override;
    void on_mouse(int mx, int my, int buttons) override;
    const KnobState* knobs() const override { return knobs_; }
    KnobState* knobs() override { return knobs_; }
    int knob_count() const override { return 4; }

    void set_session_path(const std::string& path) { session_path_ = path; }

private:
    KnobState knobs_[4];
    FlashState action_flash_{};
    AnimFloat scroll_anim_{0.0f, 0.0f, 0.2f};
    int mode_ = 0; // 0 = Samples, 1 = Sessions

    std::string current_dir_;
    std::vector<std::string> entries_;
    std::vector<bool> is_dir_;
    int cursor_ = 0;
    int scroll_ = 0;

    std::string session_path_;
    int pad_target_ = 0;

    void refresh_dir();
    void activate_entry();
    void go_up();
    void save_session();
    void load_session(const std::string& path);
    void load_sample(const std::string& path);

    void draw_file_list(Canvas& canvas, int x, int y, int w, int h);
    void draw_status_bar(Canvas& canvas, int y);
};

} // namespace chimera::ui
