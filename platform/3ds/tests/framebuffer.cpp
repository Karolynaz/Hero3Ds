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
#include <cassert>
#include <vector>

#include "../../../src/engine/framebuffer_3ds.h"

namespace
{
    uint32_t pixel( const std::vector<uint8_t> & buffer, int x, int y )
    {
        const size_t i = 1 + static_cast<size_t>( x * 240 + 239 - y ) * 3;
        return buffer[i] | ( uint32_t( buffer[i + 1] ) << 8 ) | ( uint32_t( buffer[i + 2] ) << 16 );
    }
}
int main()
{
    using namespace fheroes2::threeDS;
    std::vector<uint8_t> image( 640 * 480, 1 );
    for ( int y = 240; y < 480; ++y )
        std::fill( image.begin() + y * 640, image.begin() + ( y + 1 ) * 640, 2 );
    image[239 * 640 + 399] = 3;
    image[479 * 640 + 319] = 4;
    std::array<uint32_t, 256> palette{};
    palette[1] = 0x123456;
    palette[2] = 0xABCDEF;
    palette[3] = 0x010203;
    palette[4] = 0x987654;
    std::vector<uint8_t> top( topFramebufferBytes + 2, 0xA5 ), bottom( bottomFramebufferBytes + 2, 0xA5 );
    const auto render = [&]( const uint8_t * input, int width, int height, bool adventure ) {
        renderFramebuffers( input, width, height, palette, top.data() + 1, bottom.data() + 1, adventure );
        assert( top.front() == 0xA5 && top.back() == 0xA5 && bottom.front() == 0xA5 && bottom.back() == 0xA5 );
    };
    render( image.data(), 640, 480, true );
    assert( pixel( top, 0, 0 ) == 0x123456 && pixel( top, 399, 239 ) == 0x010203 );
    assert( pixel( bottom, 0, 0 ) == 0xABCDEF && pixel( bottom, 319, 239 ) == 0x987654 );
    // A cycling remap is applied by the engine before presentation.
    palette[1] = 0x112233;
    render( image.data(), 640, 480, false );
    assert( pixel( top, 39, 0 ) == 0 && pixel( top, 360, 239 ) == 0 );
    assert( pixel( top, 40, 0 ) == 0x112233 && pixel( top, 359, 239 ) == 0xABCDEF );
    // Menus and modal content must not be duplicated onto the lower display.
    assert( pixel( bottom, 0, 0 ) == 0 && pixel( bottom, 319, 239 ) == 0 );

    // Reproduce an adventure canvas whose hidden area retains a red main menu.
    // Opening a dialog must use only the visible map for its background and
    // preserve the native lower dashboard independently of dialog content.
    std::vector<uint8_t> adventureImage( 640 * 480, 5 );
    palette[5] = 0xFF0000;
    for ( int y = 0; y < 240; ++y )
        std::fill( adventureImage.begin() + y * 640, adventureImage.begin() + y * 640 + 400, 1 );
    for ( int y = 240; y < 480; ++y )
        std::fill( adventureImage.begin() + y * 640, adventureImage.begin() + y * 640 + 320, 2 );
    std::vector<uint8_t> modal( adventureImage.size() );
    prepareModalCanvas( adventureImage.data(), modal.data(), 640, 480 );
    assert( std::all_of( modal.begin(), modal.end(), []( uint8_t value ) { return value == 1; } ) );
    // Dialog pixels may overwrite the native dashboard coordinates on the
    // shared canvas; the snapshot must still be what the lower screen shows.
    modal[300 * 640 + 100] = 3;
    renderFramebuffers( modal.data(), 640, 480, palette, top.data() + 1, bottom.data() + 1, false, adventureImage.data() );
    assert( pixel( top, 90, 150 ) == 0x010203 );
    assert( pixel( bottom, 100, 60 ) == 0xABCDEF );
    assert( pixel( top, 359, 239 ) == 0x112233 );
    // Closing the dialog restores the original native split rather than
    // leaving the enlarged background or dialog pixels on either screen.
    render( adventureImage.data(), 640, 480, true );
    assert( pixel( top, 100, 60 ) == 0x112233 && pixel( bottom, 100, 60 ) == 0xABCDEF );
    const uint8_t tiny[] = { 3, 4, 2, 1 };
    render( tiny, 2, 2, true );
    assert( pixel( top, 1, 0 ) == 0x987654 && pixel( top, 2, 0 ) == 0 && pixel( bottom, 0, 0 ) == 0 );
    render( nullptr, 0, 0, false );
    assert( pixel( top, 40, 0 ) == 0 && pixel( bottom, 0, 0 ) == 0 );
}
