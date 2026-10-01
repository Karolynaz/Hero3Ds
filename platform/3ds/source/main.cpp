// SPDX-License-Identifier: GPL-2.0-or-later
// Hardware probe only: not yet linked to the fheroes2 engine.
#include <3ds.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include "viewport.h"
#include "controls.h"
#include <cstdio>
int main() {
    gfxInitDefault();
    gfxSet3D(false);
    consoleInit(GFX_BOTTOM, nullptr);
    unsigned leftClicks = 0, rightClicks = 0;
    bool zoom = false;
    fh3ds::InputState input;
    static std::array<std::uint8_t, fh3ds::gameWidth * fh3ds::gameHeight> image{};
    static std::array<std::uint32_t, 256> palette{};
    for (unsigned i = 0; i < palette.size(); ++i)
        palette[i] = (i << 16) | ((255 - i) << 8) | 64;
    for (int y = 0; y < fh3ds::gameHeight; ++y)
        for (int x = 0; x < fh3ds::gameWidth; ++x)
            image[y * fh3ds::gameWidth + x] = static_cast<std::uint8_t>(((x / 32) + (y / 32) * 20) % 255);
    palette[255] = 0xffffff;
    while (aptMainLoop()) {
        hidScanInput();
        const auto held = hidKeysHeld();
        const auto pressed = hidKeysDown();
        if (pressed & KEY_START) break;
        if (pressed & KEY_Y) zoom = !zoom;
        fh3ds::Action action = fh3ds::Action::None;
        if (held & KEY_TOUCH) {
            touchPosition touch{};
            hidTouchRead(&touch);
            action = input.touch(touch.px, touch.py, true);
        } else input.touch(0,0,false);
        if ((pressed & KEY_A) || action == fh3ds::Action::LeftClick) ++leftClicks;
        if ((pressed & KEY_B) || action == fh3ds::Action::RightClick) ++rightClicks;
        circlePosition stick{};
        hidCircleRead(&stick);
        const int dx = (stick.dx > 20 || (held & KEY_DRIGHT)) ? 3 :
                       (stick.dx < -20 || (held & KEY_DLEFT)) ? -3 : 0;
        const int dy = (stick.dy > 20 || (held & KEY_DUP)) ? -3 :
                       (stick.dy < -20 || (held & KEY_DDOWN)) ? 3 : 0;
        input.move(dx,dy);
        const auto cursor = input.cursor;
        std::printf("\x1b[1;1Hfheroes2 3DS hardware probe\n"
                    "Synthetic image - NOT the game\n\n"
                    "Touchpad: drag to move cursor\n"
                    "Circle Pad / D-pad: cursor\n"
                    "A: left click  B: right click\n"
                    "Y: overview / native zoom\n"
                    "Start: exit\n\n"
                    "Cursor: %3d, %3d\n"
                    "Clicks L:%6u R:%6u\n"
                    "View: %-10s", cursor.x,cursor.y,leftClicks,rightClicks,zoom ? "zoom" : "overview");
        std::printf("\x1b[27;1H   LEFT CLICK       |    RIGHT CLICK");
        const auto index = (zoom ? cursor.y : cursor.y / 2 * 2) * fh3ds::gameWidth + (zoom ? cursor.x : cursor.x / 2 * 2);
        const auto previous = image[index];
        image[index] = 255;
        auto *top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, nullptr, nullptr);
        if (zoom) fh3ds::render(image.data(), palette.data(), top, 400, fh3ds::viewportOrigin(cursor), 1);
        else fh3ds::renderOverview(image.data(), palette.data(), top);
        image[index] = previous;
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }
    gfxExit();
    return 0;
}
