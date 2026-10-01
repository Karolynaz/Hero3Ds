// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
namespace fh3ds {
constexpr int gameWidth = 640, gameHeight = 480;
struct Point { int x, y; };
// Bottom display is a full 640x480 overview. Top display is a 400x240
// native-pixel viewport centered on the cursor, avoiding unreadable scaling.
inline Point touchToGame(int x, int y) {
    return {std::clamp(x, 0, 319) * 2, std::clamp(y, 0, 239) * 2};
}
inline Point viewportOrigin(Point cursor) {
    return {std::clamp(cursor.x - 200, 0, 240), std::clamp(cursor.y - 120, 0, 240)};
}
inline std::size_t framebufferOffset(int x, int y) {
    return static_cast<std::size_t>(x * 240 + 239 - y) * 3;
}
// Input is indexed, row-major; output is the rotated libctru BGR888 buffer.
// Palette contains packed 0xRRGGBB values. Caller provides valid buffer sizes.
inline void render(const std::uint8_t *image, const std::uint32_t *palette,
                   std::uint8_t *output, int width, Point origin, int scale) {
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < 240; ++y) {
            const auto color = palette[image[(origin.y + y * scale) * gameWidth + origin.x + x * scale]];
            const auto offset = framebufferOffset(x, y);
            output[offset] = static_cast<std::uint8_t>(color);
            output[offset + 1] = static_cast<std::uint8_t>(color >> 8);
            output[offset + 2] = static_cast<std::uint8_t>(color >> 16);
        }
    }
}
}

namespace fh3ds {
// Full game on top: preserve 4:3 aspect ratio, with 40-pixel side bars.
inline void renderOverview(const std::uint8_t *image, const std::uint32_t *palette, std::uint8_t *output) {
    std::fill(output, output + 400 * 240 * 3, 0);
    for (int x = 0; x < 320; ++x) {
        for (int y = 0; y < 240; ++y) {
            const auto color = palette[image[(y * 2) * gameWidth + x * 2]];
            const auto offset = framebufferOffset(x + 40, y);
            output[offset] = static_cast<std::uint8_t>(color);
            output[offset + 1] = static_cast<std::uint8_t>(color >> 8);
            output[offset + 2] = static_cast<std::uint8_t>(color >> 16);
        }
    }
}
}
