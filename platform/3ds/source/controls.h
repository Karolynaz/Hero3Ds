// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "viewport.h"
namespace fh3ds {
enum class Action { None, LeftClick, RightClick };
struct InputState {
    Point cursor{320,240};
    bool touching = false;
    Point previousTouch{0,0};
    Action touch(int x, int y, bool down) {
        if (!down) { touching = false; return Action::None; }
        const bool started = !touching;
        touching = true;
        if (y >= 200) {
            previousTouch = {x,y};
            return started ? (x < 160 ? Action::LeftClick : Action::RightClick) : Action::None;
        }
        if (!started && previousTouch.y < 200) {
            cursor.x = std::clamp(cursor.x + (x - previousTouch.x) * 2, 0, 639);
            cursor.y = std::clamp(cursor.y + (y - previousTouch.y) * 2, 0, 479);
        }
        previousTouch = {x,y};
        return Action::None;
    }
    void move(int dx, int dy) {
        cursor.x = std::clamp(cursor.x + dx, 0, 639);
        cursor.y = std::clamp(cursor.y + dy, 0, 479);
    }
};
}
