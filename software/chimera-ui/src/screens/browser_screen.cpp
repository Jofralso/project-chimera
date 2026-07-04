#include "chimera/ui/screens/browser_screen.h"

#include <chimera/nodes/drum_node.h>
#include <chimera/nodes/master_output.h>
#include <chimera/nodes/synth_node.h>

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <cstring>
#include <SDL_events.h>

namespace chimera::ui {

BrowserScreen::BrowserScreen()
    : Screen("BROWSER")
{
    knobs_[0] = {"MODE", "Samples", 0.0f, 0, 1, 1.0f, true};
    knobs_[1] = {"SORT", "Name", 0.0f, 0, 1, 1.0f, false};
    knobs_[2] = {"PAD", "1", 0.0f, 0, 3, 1.0f, true};
    knobs_[3] = {"ACTION", "--", 0.0f, 0, 1, 1.0f, false};

    current_dir_ = std::filesystem::current_path().string();
    refresh_dir();
}

void BrowserScreen::update() {
    pulse_.update();
    action_flash_.update();
    scroll_anim_.update();
}

void BrowserScreen::render(Canvas& canvas, bool active) {
    auto& t = theme();
    draw_header(canvas, name_, mode_ == 0 ? "SAMPLES" : "SESSIONS");

    // Directory path (truncated)
    std::string dir_display = current_dir_;
    if (dir_display.size() > 36) {
        dir_display = "..." + dir_display.substr(dir_display.size() - 33);
    }
    canvas.text(t.padding, t.header_h + 2, dir_display, t.fg_dim, 1);

    int list_y = t.header_h + 18;
    int list_h = t.screen_h - list_y - t.footer_h - t.padding;
    draw_file_list(canvas, t.padding, list_y, t.screen_w - 2 * t.padding, list_h);

    draw_status_bar(canvas, t.screen_h - t.footer_h - 4);

    draw_knobs(canvas, knobs_, 4);

    // Knob 3 action text
    if (mode_ == 0 && !entries_.empty() && cursor_ < static_cast<int>(entries_.size())) {
        auto path = current_dir_ + "/" + entries_[cursor_];
        if (!is_dir_[cursor_] && path.size() > 4 &&
            path.substr(path.size() - 4) == ".wav") {
            knobs_[3].value_str = "LOAD";
            knobs_[3].active = true;
            knobs_[3].label = "LOAD";
        } else {
            knobs_[3].value_str = "--";
            knobs_[3].active = false;
            knobs_[3].label = "ACTION";
        }
    } else if (mode_ == 1 && !entries_.empty() && cursor_ < static_cast<int>(entries_.size())) {
        auto path = current_dir_ + "/" + entries_[cursor_];
        if (!is_dir_[cursor_] && path.size() > 8 &&
            path.substr(path.size() - 8) == ".chimera") {
            knobs_[3].value_str = "LOAD";
            knobs_[3].active = true;
            knobs_[3].label = "LOAD";
        } else {
            knobs_[3].value_str = "--";
            knobs_[3].active = false;
            knobs_[3].label = "ACTION";
        }
    }
}

void BrowserScreen::draw_file_list(Canvas& canvas, int x, int y, int w, int h) {
    auto& t = theme();
    canvas.rect(x, y, w, h, t.fg_dim);

    int line_h = 14;
    int visible = h / line_h;

    if (cursor_ < scroll_) scroll_ = cursor_;
    if (cursor_ >= scroll_ + visible) scroll_ = cursor_ - visible + 1;

    // ".." entry
    int draw_y = y + 2;
    bool cursor_on_parent = (cursor_ == -1 && scroll_ <= -1);
    if (scroll_ <= -1) {
        Color c = cursor_on_parent ? t.fg_bright : t.fg_dim;
        canvas.text(x + 4, draw_y, "[..]", c, 1);
        draw_y += line_h;
    }

    for (int i = scroll_; i < static_cast<int>(entries_.size()) && draw_y < y + h - 4; ++i) {
        bool sel = (i == cursor_);
        if (sel && draw_y >= y && draw_y < y + h) {
            canvas.fill_rect(x + 1, draw_y, w - 2, line_h - 2, t.muted);
        }

        std::string name = entries_[i];
        if (is_dir_[i]) name = "[" + name + "]";

        Color c = sel ? t.fg_bright : (is_dir_[i] ? t.fg : t.fg_dim);
        canvas.text(x + 4, draw_y, name, c, 1);

        if (sel && mode_ == 0 && !is_dir_[i]) {
            auto full = current_dir_ + "/" + name;
            if (full.size() > 4 && full.substr(full.size() - 4) == ".wav") {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "PAD %d", pad_target_ + 1);
                canvas.text_right(x + 4, draw_y, w - 8, buf, t.accent, 1);
            }
        } else if (sel && mode_ == 1 && !is_dir_[i]) {
            auto full = current_dir_ + "/" + name;
            if (full.size() > 8 && full.substr(full.size() - 8) == ".chimera") {
                canvas.text_right(x + 4, draw_y, w - 8, "LOAD", t.accent, 1);
            }
        }
        draw_y += line_h;
    }
}

void BrowserScreen::draw_status_bar(Canvas& canvas, int y) {
    auto& t = theme();
    canvas.fill_rect(0, y, t.screen_w, 4, t.fg_dim);

    char buf[48];
    int dirs = 0;
    for (auto d : is_dir_) if (d) dirs++;

    if (mode_ == 0) {
        int wavs = 0;
        for (size_t i = 0; i < entries_.size(); ++i) {
            if (!is_dir_[i]) {
                auto p = current_dir_ + "/" + entries_[i];
                if (p.size() > 4 && p.substr(p.size() - 4) == ".wav") wavs++;
            }
        }
        std::snprintf(buf, sizeof(buf), "%d .wav  %d dirs", wavs, dirs);
    } else {
        int sess = 0;
        for (size_t i = 0; i < entries_.size(); ++i) {
            if (!is_dir_[i]) {
                auto p = current_dir_ + "/" + entries_[i];
                if (p.size() > 8 && p.substr(p.size() - 8) == ".chimera") sess++;
            }
        }
        std::snprintf(buf, sizeof(buf), "%d sessions  %d dirs", sess, dirs);
    }
    canvas.text(t.padding, t.screen_h - t.footer_h + 4, buf, t.fg_dim, 1);

    // Show action feedback
    if (action_flash_.value() > 0.1f) {
        float af = action_flash_.value();
        Color ac = Color(static_cast<uint8_t>(170 + 85 * af),
                         static_cast<uint8_t>(204 * af),
                         static_cast<uint8_t>(85 * af), 255);
        canvas.text_right(t.padding, t.screen_h - t.footer_h + 4,
                          t.screen_w - 2 * t.padding, knobs_[3].value_str.c_str(),
                          ac, 1);
    } else {
        char help[32];
        std::snprintf(help, sizeof(help), "Esc=back  F5=save");
        canvas.text_right(t.padding, t.screen_h - t.footer_h + 4,
                          t.screen_w - 2 * t.padding, help, t.fg_dim, 1);
    }
}

void BrowserScreen::on_knob(int index, int delta) {
    if (index < 0 || index >= 4) return;
    knobs_[index].value += delta * knobs_[index].step;
    if (knobs_[index].value < knobs_[index].min)
        knobs_[index].value = knobs_[index].min;
    if (knobs_[index].value > knobs_[index].max)
        knobs_[index].value = knobs_[index].max;

    if (index == 0) {
        int new_mode = static_cast<int>(knobs_[0].value);
        if (new_mode != mode_) {
            mode_ = new_mode;
            knobs_[0].value_str = mode_ == 0 ? "Samples" : "Sessions";
            refresh_dir();
        }
    } else if (index == 2) {
        pad_target_ = static_cast<int>(knobs_[2].value);
        char buf[12];
        std::snprintf(buf, sizeof(buf), "%d", pad_target_ + 1);
        knobs_[2].value_str = buf;
    } else if (index == 3 && knobs_[3].active) {
        activate_entry();
    }
}

void BrowserScreen::on_key(int key, bool down) {
    if (!down) return;
    switch (key) {
        case 202: // Left - go up
            go_up();
            break;
        case 203: // Right - enter
        case 13:  // Enter
            activate_entry();
            break;
        case 200: // Up
            if (cursor_ > -1) {
                cursor_--;
            } else {
                cursor_ = static_cast<int>(entries_.size()) - 1;
            }
            break;
        case 201: // Down
            if (cursor_ < static_cast<int>(entries_.size()) - 1) {
                cursor_++;
            } else {
                cursor_ = -1;
            }
            break;
        case 27:  // Escape - go up
            go_up();
            break;
        case 269: // F5 - save session
            if (mode_ == 1) save_session();
            break;
    }
}

void BrowserScreen::on_mouse(int mx, int my, int buttons) {
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
            if (drag_knob_ == 3 && knobs()[3].active) {
                activate_entry();
            }
            return;
        }

        int list_y = t.header_h + 18;
        int list_h = t.screen_h - list_y - t.footer_h - t.padding;
        if (my >= list_y && my < list_y + list_h) {
            int line_h = 14;
            int idx = (my - list_y - 2) / line_h + scroll_;
            int logical_idx = idx - (scroll_ <= -1 ? 1 : 0);
            if (logical_idx >= -1 && logical_idx < static_cast<int>(entries_.size())) {
                cursor_ = logical_idx;
                if (!(buttons & SDL_BUTTON_LMASK)) {
                    activate_entry();
                }
            }
        }
    } else {
        end_knob_drag();
    }
}

void BrowserScreen::refresh_dir() {
    entries_.clear();
    is_dir_.clear();
    cursor_ = 0;
    scroll_ = 0;

    std::error_code ec;
    if (!std::filesystem::exists(current_dir_, ec)) return;
    for (const auto& entry : std::filesystem::directory_iterator(current_dir_, ec)) {
        if (ec) break;
        std::string name = entry.path().filename().string();

        if (name.empty() || name[0] == '.') continue;

        if (entry.is_directory()) {
            entries_.push_back(name);
            is_dir_.push_back(true);
        } else {
            std::string ext;
            if (name.size() > 4) ext = name.substr(name.size() - 4);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

            if (mode_ == 0 && ext == ".wav") {
                entries_.push_back(name);
                is_dir_.push_back(false);
            } else if (mode_ == 1) {
                std::string ext_long;
                if (name.size() > 8) ext_long = name.substr(name.size() - 8);
                std::transform(ext_long.begin(), ext_long.end(),
                               ext_long.begin(), ::tolower);
                if (ext_long == ".chimera") {
                    entries_.push_back(name);
                    is_dir_.push_back(false);
                } else {
                    entries_.push_back(name);
                    is_dir_.push_back(false);
                }
            }
        }
    }

    // Sort: dirs first, then by name
    std::vector<std::pair<std::string, bool>> pairs;
    for (size_t i = 0; i < entries_.size(); ++i)
        pairs.emplace_back(entries_[i], is_dir_[i]);

    std::sort(pairs.begin(), pairs.end(),
        [](const auto& a, const auto& b) {
            if (a.second != b.second) return a.second > b.second;
            std::string al, bl;
            al.resize(a.first.size());
            bl.resize(b.first.size());
            std::transform(a.first.begin(), a.first.end(), al.begin(), ::tolower);
            std::transform(b.first.begin(), b.first.end(), bl.begin(), ::tolower);
            return al < bl;
        });

    entries_.clear();
    is_dir_.clear();
    for (auto& p : pairs) {
        entries_.push_back(p.first);
        is_dir_.push_back(p.second);
    }
}

void BrowserScreen::activate_entry() {
    if (cursor_ < 0 || cursor_ >= static_cast<int>(entries_.size())) return;
    std::string name = entries_[cursor_];
    std::string full = current_dir_ + "/" + name;

    if (is_dir_[cursor_]) {
        current_dir_ = full;
        refresh_dir();
        return;
    }

    if (mode_ == 0) {
        std::string ext;
        if (name.size() > 4) ext = name.substr(name.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".wav") {
            load_sample(full);
        }
    } else if (mode_ == 1) {
        std::string ext;
        if (name.size() > 8) ext = name.substr(name.size() - 8);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".chimera") {
            load_session(full);
        }
    }
}

void BrowserScreen::go_up() {
    auto parent = std::filesystem::path(current_dir_).parent_path();
    current_dir_ = parent.string();
    refresh_dir();
}

void BrowserScreen::save_session() {
    if (!engine_) return;
    std::string save_dir = session_path_.empty() ? current_dir_ : session_path_;
    auto ts = std::time(nullptr);
    char name[64];
    std::strftime(name, sizeof(name), "session_%Y%m%d_%H%M%S.chimera",
                  std::localtime(&ts));
    std::string save_path = save_dir + "/" + name;

    chimera::Session session;
    session.set_name(name);
    session.export_graph(engine_->graph());
    if (session.save(save_path)) {
        knobs_[3].value_str = "SAVED";
        knobs_[3].active = true;
        knobs_[3].label = "SAVED";
        current_dir_ = save_dir;
        refresh_dir();
    } else {
        knobs_[3].value_str = "FAILED";
        knobs_[3].active = true;
        knobs_[3].label = "FAILED";
    }
    action_flash_.trigger();
}

void BrowserScreen::load_session(const std::string& path) {
    if (!engine_) return;
    chimera::Session session;
    if (!session.load(path)) {
        knobs_[3].value_str = "ERR";
        knobs_[3].active = true;
        knobs_[3].label = "ERR";
        return;
    }

    // Need to stop engine first
    auto* eng = engine_;
    eng->stop();
    eng->graph().clear();

    if (!session.import_graph(eng->graph())) {
        knobs_[3].value_str = "ERR";
        knobs_[3].active = true;
        knobs_[3].label = "ERR";
        // Rebuild default graph on failure
        auto master = std::make_unique<chimera::MasterOutputNode>(2u);
        eng->add_node(std::move(master));
        auto synth = std::make_unique<chimera::SynthNode>(6);
        eng->add_node(std::move(synth));
        auto drum = std::make_unique<chimera::DrumNode>(4);
        eng->add_node(std::move(drum));
        eng->start();
        return;
    }

    eng->graph().prepare(eng->sample_rate(), eng->block_size());
    eng->start();

    knobs_[3].value_str = "LOADED";
    knobs_[3].active = true;
    knobs_[3].label = "LOADED";
    refresh_dir();
    action_flash_.trigger();
}

void BrowserScreen::load_sample(const std::string& path) {
    if (!engine_) return;
    auto* n = engine_->graph().find_node_by_class("builtin.drum");
    if (!n) {
        knobs_[3].value_str = "NODRUM";
        knobs_[3].active = true;
        knobs_[3].label = "NODRUM";
        return;
    }
    auto* drum = reinterpret_cast<chimera::DrumNode*>(n);
    if (drum->load_sample(static_cast<uint32_t>(pad_target_), path)) {
        knobs_[3].value_str = "LOADED";
        knobs_[3].active = true;
        knobs_[3].label = "LOADED";
    } else {
        knobs_[3].value_str = "FAILED";
        knobs_[3].active = true;
        knobs_[3].label = "FAILED";
    }
    action_flash_.trigger();
}

} // namespace chimera::ui
