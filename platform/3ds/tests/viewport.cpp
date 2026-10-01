// SPDX-License-Identifier: GPL-2.0-or-later
#include "../source/viewport.h"
#include <cassert>
#include <vector>
#include "../source/controls.h"
int main() {
    using namespace fh3ds;
    assert(touchToGame(-1, -1).x == 0);
    assert(touchToGame(319,239).x == 638);
    assert(touchToGame(999,999).y == 478);
    assert(viewportOrigin({0,0}).x == 0);
    assert(viewportOrigin({639,479}).x == 240);
    assert(viewportOrigin({639,479}).y == 240);
    assert(framebufferOffset(0,239) == 0);
    std::vector<std::uint8_t> input(gameWidth * gameHeight, 1);
    std::uint32_t palette[256]{};
    palette[1] = 0x123456;
    InputState state;
    state.touch(10,10,true);
    state.touch(20,30,true);
    assert(state.cursor.x == 340 && state.cursor.y == 280);
    state.touch(0,0,false);
    assert(state.touch(10,220,true) == Action::LeftClick);
    assert(state.touch(10,220,true) == Action::None);
    state.touch(0,0,false);
    assert(state.touch(200,220,true) == Action::RightClick);
    state.move(10000,-10000);
    assert(state.cursor.x == 639 && state.cursor.y == 0);
    std::vector<std::uint8_t> overview(400 * 240 * 3 + 2, 0xaa);
    renderOverview(input.data(),palette,overview.data()+1);
    assert(overview.front() == 0xaa && overview.back() == 0xaa);
    assert(overview[1 + framebufferOffset(0,0)] == 0);
    assert(overview[1 + framebufferOffset(40,0)] == 0x56);
    assert(overview[1 + framebufferOffset(359,239)] == 0x56);
    assert(overview[1 + framebufferOffset(360,0)] == 0);
    for (int width : {320,400}) {
        std::vector<std::uint8_t> output(width * 240 * 3 + 2, 0xaa);
        render(input.data(), palette, output.data()+1, width,
               width == 400 ? Point{240,240} : Point{0,0}, width == 400 ? 1 : 2);
        assert(output.front() == 0xaa && output.back() == 0xaa);
        assert(output[1] == 0x56 && output[2] == 0x34 && output[3] == 0x12);
    }
}
