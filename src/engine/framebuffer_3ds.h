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

    struct CastleFrame
    {
        bool mainView{ false };
        bool fullTop{ false };
        bool nativeDual{ false };
        const uint8_t * footer{ nullptr };
        int footerWidth{ 0 };
        int footerHeight{ 0 };
    };

    struct DialogFrame
    {
        const uint8_t * frozenTop{ nullptr }; // Packed 400x240 indexed pixels.
        bool topOverlay{ false };
        int cropX{ 0 }, cropY{ 240 };
        int focusX{ 0 }, focusY{ 0 }, focusWidth{ 0 }, focusHeight{ 0 };
        bool dimBackground{ true };
    };

    inline void copyScaledIndexed( const uint8_t * source, const int width, const int height, const int srcX, const int srcY, const int srcWidth,
                                   const int srcHeight, uint8_t * output )
    {
        if ( !source || !output || width <= 0 || height <= 0 || srcWidth <= 0 || srcHeight <= 0 ) {
            std::fill( output, output + 400 * 240, 0 );
            return;
        }
        for ( int y = 0; y < 240; ++y ) {
            const int sy = std::clamp( srcY + ( y * 2 + 1 ) * srcHeight / 480, 0, height - 1 );
            for ( int x = 0; x < 400; ++x ) {
                const int sx = std::clamp( srcX + ( x * 2 + 1 ) * srcWidth / 800, 0, width - 1 );
                output[y * 400 + x] = source[static_cast<size_t>( sy ) * width + sx];
            }
        }
    }

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
                                    const DialogFrame * dialog = nullptr, const CastleFrame * castle = nullptr )
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
        const auto copyNative = [&]( uint8_t * output, const int sourceX, const int sourceY, const int width, const int height, const int destinationY,
                                     const uint8_t * source, const int sourceStride, const int sourceHeight ) {
            for ( int x = 0; x < width; ++x ) {
                const int sx = sourceX + x;
                if ( sx < 0 || sx >= sourceStride )
                    continue;
                for ( int y = 0; y < height && y + destinationY < 240; ++y ) {
                    const int sy = sourceY + y;
                    if ( sy < 0 || sy >= sourceHeight )
                        continue;
                    const auto color = palette[source[static_cast<size_t>( sy ) * sourceStride + sx]];
                    const size_t offset = static_cast<size_t>( x * 240 + 239 - y - destinationY ) * 3;
                    output[offset] = static_cast<uint8_t>( color );
                    output[offset + 1] = static_cast<uint8_t>( color >> 8 );
                    output[offset + 2] = static_cast<uint8_t>( color >> 16 );
                }
            }
        };
        const auto copyScaledFast = [&]( uint8_t * output, const int dstWidth, const int dstHeight, const int dstY, const int srcX, const int srcY,
                                         const int srcWidth, const int srcHeight ) {
            if ( srcWidth <= 0 || srcHeight <= 0 || dstWidth <= 0 || dstHeight <= 0 )
                return;
            std::array<int, 400> sx{};
            std::array<const uint8_t *, 240> rows{};
            for ( int x = 0; x < dstWidth; ++x )
                sx[x] = std::clamp( srcX + ( 2 * x + 1 ) * srcWidth / ( 2 * dstWidth ), 0, imageWidth - 1 );
            for ( int y = 0; y < dstHeight; ++y ) {
                const int sy = std::clamp( srcY + ( 2 * y + 1 ) * srcHeight / ( 2 * dstHeight ), 0, imageHeight - 1 );
                rows[y] = image + static_cast<size_t>( sy ) * imageWidth;
            }
            for ( int x = 0; x < dstWidth; ++x ) {
                const int col = sx[x];
                uint8_t * dst = output + static_cast<size_t>( x * 240 + 239 - dstY ) * 3;
                for ( int y = 0; y < dstHeight && y + dstY < 240; ++y, dst -= 3 ) {
                    const uint32_t color = palette[rows[y][col]];
                    dst[0] = static_cast<uint8_t>( color );
                    dst[1] = static_cast<uint8_t>( color >> 8 );
                    dst[2] = static_cast<uint8_t>( color >> 16 );
                }
            }
        };
        const auto copyScaledAvgX = [&]( uint8_t * output, const int dstWidth, const int dstHeight, const int dstY, const int srcX, const int srcY,
                                         const int srcWidth, const int srcHeight ) {
            if ( srcWidth <= 0 || srcHeight <= 0 || dstWidth <= 0 || dstHeight <= 0 )
                return;
            std::array<int, 400> x0{};
            std::array<int, 400> x1{};
            std::array<const uint8_t *, 240> rows{};
            for ( int x = 0; x < dstWidth; ++x ) {
                const int p0 = std::clamp( ( 2 * x * srcWidth ) / ( 2 * dstWidth ), 0, srcWidth - 1 );
                const int p1 = std::clamp( ( ( 2 * x + 1 ) * srcWidth ) / ( 2 * dstWidth ), 0, srcWidth - 1 );
                x0[x] = std::clamp( srcX + p0, 0, imageWidth - 1 );
                x1[x] = std::clamp( srcX + p1, 0, imageWidth - 1 );
            }
            for ( int y = 0; y < dstHeight; ++y ) {
                const int sy = std::clamp( srcY + ( 2 * y + 1 ) * srcHeight / ( 2 * dstHeight ), 0, imageHeight - 1 );
                rows[y] = image + static_cast<size_t>( sy ) * imageWidth;
            }
            for ( int x = 0; x < dstWidth; ++x ) {
                const int col0 = x0[x];
                const int col1 = x1[x];
                uint8_t * dst = output + static_cast<size_t>( x * 240 + 239 - dstY ) * 3;
                for ( int y = 0; y < dstHeight && y + dstY < 240; ++y, dst -= 3 ) {
                    const uint32_t c0 = palette[rows[y][col0]];
                    const uint32_t c1 = palette[rows[y][col1]];
                    const uint32_t avg = ( c0 & c1 ) + ( ( ( c0 ^ c1 ) & 0xFEFEFEu ) >> 1 );
                    dst[0] = static_cast<uint8_t>( avg );
                    dst[1] = static_cast<uint8_t>( avg >> 8 );
                    dst[2] = static_cast<uint8_t>( avg >> 16 );
                }
            }
        };
        const auto copyScaledFull = [&]( uint8_t * output, const int dstWidth, const int dstHeight, const int srcWidth, const int srcHeight ) {
            if ( srcWidth <= 0 || srcHeight <= 0 || dstWidth <= 0 || dstHeight <= 0 )
                return;
            std::array<int, 400> x0{};
            std::array<int, 400> x1{};
            std::array<const uint8_t *, 240> rows0{};
            std::array<const uint8_t *, 240> rows1{};
            for ( int x = 0; x < dstWidth; ++x ) {
                x0[x] = std::clamp( ( 2 * x * srcWidth ) / ( 2 * dstWidth ), 0, imageWidth - 1 );
                x1[x] = std::clamp( ( ( 2 * x + 1 ) * srcWidth ) / ( 2 * dstWidth ), 0, imageWidth - 1 );
            }
            for ( int y = 0; y < dstHeight; ++y ) {
                const int sy0 = std::clamp( ( 2 * y * srcHeight ) / ( 2 * dstHeight ), 0, imageHeight - 1 );
                const int sy1 = std::clamp( ( ( 2 * y + 1 ) * srcHeight ) / ( 2 * dstHeight ), 0, imageHeight - 1 );
                rows0[y] = image + static_cast<size_t>( sy0 ) * imageWidth;
                rows1[y] = image + static_cast<size_t>( sy1 ) * imageWidth;
            }
            for ( int x = 0; x < dstWidth; ++x ) {
                const int col0 = x0[x];
                const int col1 = x1[x];
                uint8_t * dst = output + static_cast<size_t>( x * 240 + 239 ) * 3;
                for ( int y = 0; y < dstHeight; ++y, dst -= 3 ) {
                    const uint32_t topAvg = ( palette[rows0[y][col0]] & palette[rows0[y][col1]] )
                                            + ( ( ( palette[rows0[y][col0]] ^ palette[rows0[y][col1]] ) & 0xFEFEFEu ) >> 1 );
                    const uint32_t botAvg = ( palette[rows1[y][col0]] & palette[rows1[y][col1]] )
                                            + ( ( ( palette[rows1[y][col0]] ^ palette[rows1[y][col1]] ) & 0xFEFEFEu ) >> 1 );
                    const uint32_t avg = ( topAvg & botAvg ) + ( ( ( topAvg ^ botAvg ) & 0xFEFEFEu ) >> 1 );
                    dst[0] = static_cast<uint8_t>( avg );
                    dst[1] = static_cast<uint8_t>( avg >> 8 );
                    dst[2] = static_cast<uint8_t>( avg >> 16 );
                }
            }
        };
        if ( dialog && dialog->frozenTop ) {
            if ( dialog->topOverlay )
                copyNative( top, 0, 0, 400, 240, 0, image, imageWidth, imageHeight );
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
            copyNative( bottom, dialog->cropX, dialog->cropY, 320, 240, 0, image, imageWidth, imageHeight );
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
            copyNative( bottom, 0, battle->footerY, 320, height, 0, footer, imageWidth, imageHeight );
            copyNative( bottom, 320, battle->footerY, 320, height, height, footer, imageWidth, imageHeight );
        }
        else if ( castle && castle->mainView ) {
            copyScaledFast( top, 400, 240, 0, 0, 0, std::min( imageWidth, 640 ), std::min( imageHeight, 256 ) );
            if ( imageHeight > 256 ) {
                const int armyWidth = std::min( imageWidth, 552 );
                const int armyHeight = std::min( imageHeight - 256, 204 );
                if ( castle->footer && castle->footerWidth == 320 && castle->footerHeight > 0 && castle->footerHeight < 240 ) {
                    const int splitY = 240 - castle->footerHeight;
                    copyScaledAvgX( bottom, 320, splitY, 0, 0, 256, armyWidth, armyHeight );
                    copyNative( bottom, 0, 0, 320, castle->footerHeight, splitY, castle->footer, castle->footerWidth, castle->footerHeight );
                }
                else {
                    copyScaledAvgX( bottom, 320, 240, 0, 0, 256, armyWidth, armyHeight );
                }
            }
        }
        else if ( castle && castle->fullTop ) {
            copyScaledFull( top, 400, 240, std::min( imageWidth, 640 ), std::min( imageHeight, 480 ) );
        }
        else if ( castle && castle->nativeDual ) {
            copy( top, 400, 0, 1, 0, image );
            copy( bottom, 320, 240, 1, 0, image );
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
