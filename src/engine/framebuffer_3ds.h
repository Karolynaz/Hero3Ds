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

    // The palette is already remapped for color cycling, in packed 0xRRGGBB.
    // Output uses libctru's rotated, column-major BGR8 framebuffer format.
    inline void renderFramebuffers( const uint8_t * image, const int imageWidth, const int imageHeight, const std::array<uint32_t, 256> & palette, uint8_t * top,
                                    uint8_t * bottom, const bool adventure )
    {
        std::fill( top, top + topFramebufferBytes, 0 );
        std::fill( bottom, bottom + bottomFramebufferBytes, 0 );
        if ( !image || imageWidth <= 0 || imageHeight <= 0 )
            return;
        const auto copy = [&]( uint8_t * output, const int width, const int sourceY, const int scale, const int destinationX ) {
            for ( int x = 0; x < width; ++x ) {
                for ( int y = 0; y < 240; ++y ) {
                    const int sx = x * scale;
                    const int sy = sourceY + y * scale;
                    if ( sx >= imageWidth || sy >= imageHeight )
                        continue;
                    const uint32_t color = palette[image[static_cast<size_t>( sy ) * imageWidth + sx]];
                    const size_t offset = static_cast<size_t>( ( x + destinationX ) * 240 + 239 - y ) * 3;
                    output[offset] = static_cast<uint8_t>( color );
                    output[offset + 1] = static_cast<uint8_t>( color >> 8 );
                    output[offset + 2] = static_cast<uint8_t>( color >> 16 );
                }
            }
        };
        if ( adventure ) {
            copy( top, 400, 0, 1, 0 );
            copy( bottom, 320, 240, 1, 0 );
        }
        else {
            copy( top, 320, 0, 2, 40 );
            copy( bottom, 320, 0, 2, 0 );
        }
    }
}
