/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2026                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace fheroes2::threeDS
{
    constexpr size_t topFramebufferBytes = 400 * 240 * 3;
    constexpr size_t bottomFramebufferBytes = 320 * 240 * 3;

    // Native 400x240 battle viewport, shared with frozen modal backgrounds.
    inline void copyBattleTopIndexed( const uint8_t * source, const int width, const int height, const int footerY, uint8_t * output, const int cameraX = 0,
                                      const int cameraY = 0 )
    {
        std::fill( output, output + 400 * 240, 0 );
        for ( int y = 0; y < 240; ++y )
            for ( int x = 0; x < 400; ++x ) {
                const int sx = cameraX + x, sy = cameraY + y;
                if ( sx >= 0 && sy >= 0 && sx < width && sy < height && sy < footerY )
                    output[y * 400 + x] = source[sy * width + sx];
            }
    }

    struct BattleFrame
    {
        bool nativeView{ false };
        int cameraX{ 0 };
        int cameraY{ 0 };
        int footerY{ 430 };
        int footerHeight{ 50 };
        const uint8_t * modalSnapshot{ nullptr };
    };

    struct DialogFrame
    {
        const uint8_t * frozenTop{ nullptr }; // Packed 400x240 indexed pixels.
        bool topOverlay{ false };
        int cropX{ 0 }, cropY{ 240 };
        int focusX{ 0 }, focusY{ 0 }, focusWidth{ 0 }, focusHeight{ 0 };
        bool dimBackground{ true };
    };

    // Prepare a coherent full-size modal background from the visible upper
    // adventure screen. Never sample hidden canvas pixels left by the menu.
    inline void prepareModalCanvas( const uint8_t * snapshot, uint8_t * output, const int width, const int height )
    {
        if ( !snapshot || !output || width <= 0 || height <= 0 )
            return;
        const int visibleWidth = std::min( width, 400 );
        const int visibleHeight = std::min( height, 240 );
        for ( int y = 0; y < height; ++y ) {
            for ( int x = 0; x < width; ++x ) {
                output[static_cast<size_t>( y ) * width + x] = snapshot[static_cast<size_t>( y * visibleHeight / height ) * width + x * visibleWidth / width];
            }
        }
    }

    // The palette is already remapped for color cycling, in packed 0xRRGGBB.
    // Output uses libctru's rotated, column-major BGR8 framebuffer format.
    inline void renderFramebuffers( const uint8_t * image, const int imageWidth, const int imageHeight, const std::array<uint32_t, 256> & palette, uint8_t * top,
                                    uint8_t * bottom, const bool adventure, const uint8_t * adventureSnapshot = nullptr, const BattleFrame * battle = nullptr,
                                    const DialogFrame * dialog = nullptr )
    {
        std::fill( top, top + topFramebufferBytes, 0 );
        std::fill( bottom, bottom + bottomFramebufferBytes, 0 );
        if ( !image || imageWidth <= 0 || imageHeight <= 0 )
            return;
        const auto copy = [&]( uint8_t * output, const int width, const int sourceY, const int scale, const int destinationX, const uint8_t * source ) {
            for ( int x = 0; x < width; ++x ) {
                for ( int y = 0; y < 240; ++y ) {
                    const int sx = x * scale;
                    const int sy = sourceY + y * scale;
                    if ( sx >= imageWidth || sy >= imageHeight )
                        continue;
                    const uint32_t color = palette[source[static_cast<size_t>( sy ) * imageWidth + sx]];
                    const size_t offset = static_cast<size_t>( ( x + destinationX ) * 240 + 239 - y ) * 3;
                    output[offset] = static_cast<uint8_t>( color );
                    output[offset + 1] = static_cast<uint8_t>( color >> 8 );
                    output[offset + 2] = static_cast<uint8_t>( color >> 16 );
                }
            }
        };
        const auto copyNative
            = [&]( uint8_t * output, const int sourceX, const int sourceY, const int width, const int height, const int destinationY, const uint8_t * source ) {
                  for ( int x = 0; x < width; ++x ) {
                      for ( int y = 0; y < height && y + destinationY < 240; ++y ) {
                          const int sx = sourceX + x;
                          const int sy = sourceY + y;
                          if ( sx < 0 || sy < 0 || sx >= imageWidth || sy >= imageHeight )
                              continue;
                          const auto color = palette[source[static_cast<size_t>( sy ) * imageWidth + sx]];
                          const size_t offset = static_cast<size_t>( x * 240 + 239 - y - destinationY ) * 3;
                          output[offset] = static_cast<uint8_t>( color );
                          output[offset + 1] = static_cast<uint8_t>( color >> 8 );
                          output[offset + 2] = static_cast<uint8_t>( color >> 16 );
                      }
                  }
              };
        if ( dialog && dialog->frozenTop ) {
            if ( dialog->topOverlay )
                copyNative( top, 0, 0, 400, 240, 0, image );
            else
                for ( int x = 0; x < 400; ++x )
                    for ( int y = 0; y < 240; ++y ) {
                        const auto color = palette[dialog->frozenTop[y * 400 + x]];
                        const size_t offset = static_cast<size_t>( x * 240 + 239 - y ) * 3;
                        top[offset] = static_cast<uint8_t>( color );
                        top[offset + 1] = static_cast<uint8_t>( color >> 8 );
                        top[offset + 2] = static_cast<uint8_t>( color >> 16 );
                    }
            if ( dialog->dimBackground ) {
                for ( int x = 0; x < 400; ++x )
                    for ( int y = 0; y < 240; ++y ) {
                        // An authored top panel is foreground; only its backdrop
                        // is darkened. Indexed canvas and shared palette stay intact.
                        if ( dialog->topOverlay && x >= 10 && x < 390 && y >= 10 && y < 230 )
                            continue;
                        const size_t offset = static_cast<size_t>( x * 240 + 239 - y ) * 3;
                        for ( size_t channel = 0; channel < 3; ++channel )
                            top[offset + channel] = static_cast<uint8_t>( static_cast<unsigned>( top[offset + channel] ) * 55 / 100 );
                    }
            }
            copyNative( bottom, dialog->cropX, dialog->cropY, 320, 240, 0, image );
            if ( dialog->focusWidth > 0 && dialog->focusHeight > 0 ) {
                const int left = dialog->focusX - dialog->cropX, upper = dialog->focusY - dialog->cropY;
                const auto highlight = [&]( const int x, const int y ) {
                    if ( x < 0 || x >= 320 || y < 0 || y >= 240 )
                        return;
                    const size_t offset = static_cast<size_t>( x * 240 + 239 - y ) * 3;
                    bottom[offset] = 32;
                    bottom[offset + 1] = 240;
                    bottom[offset + 2] = 255;
                };
                for ( int x = left; x < left + dialog->focusWidth; ++x ) {
                    highlight( x, upper );
                    highlight( x, upper + dialog->focusHeight - 1 );
                }
                for ( int y = upper; y < upper + dialog->focusHeight; ++y ) {
                    highlight( left, y );
                    highlight( left + dialog->focusWidth - 1, y );
                }
            }
        }
        else if ( battle && ( battle->nativeView || battle->modalSnapshot ) ) {
            if ( battle->nativeView ) {
                for ( int y = 0; y < 240; ++y )
                    for ( int x = 0; x < 400; ++x ) {
                        const int sx = battle->cameraX + x, sy = battle->cameraY + y;
                        if ( sx < 0 || sy < 0 || sx >= imageWidth || sy >= imageHeight || sy >= battle->footerY )
                            continue;
                        const auto color = palette[image[static_cast<size_t>( sy ) * imageWidth + sx]];
                        const size_t offset = static_cast<size_t>( x * 240 + 239 - y ) * 3;
                        top[offset] = static_cast<uint8_t>( color );
                        top[offset + 1] = static_cast<uint8_t>( color >> 8 );
                        top[offset + 2] = static_cast<uint8_t>( color >> 16 );
                    }
            }
            else
                copy( top, 320, 0, 2, 40, image );
            // Keep all original footer controls and hitboxes on the logical
            // canvas. Present its left and right halves in two native rows.
            const auto * footer = battle->nativeView ? image : battle->modalSnapshot;
            const int height = std::clamp( battle->footerHeight, 1, 120 );
            copyNative( bottom, 0, battle->footerY, 320, height, 0, footer );
            copyNative( bottom, 320, battle->footerY, 320, height, height, footer );
        }
        else if ( adventure ) {
            copy( top, 400, 0, 1, 0, image );
            copy( bottom, 320, 240, 1, 0, image );
        }
        else {
            copy( top, 320, 0, 2, 40, image );
            // Modal content belongs to the top screen. Keep the dashboard below
            // when the modal came from adventure, rather than duplicating it.
            if ( adventureSnapshot )
                copy( bottom, 320, 240, 1, 0, adventureSnapshot );
        }
    }
}
