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
#include <cmath>

#include "input_3ds.h"

int main()
{
    using fheroes2::input3DS::TouchGate;
    using Transition = TouchGate::Transition;
    TouchGate touch;
    assert( touch.update( true, true, true, false ) == Transition::Press );
    assert( touch.active() );
    assert( touch.update( false, true, false, false ) == Transition::Cancel );
    assert( !touch.active() );
    // Returning from a modal while still touching cannot select the dashboard.
    assert( touch.update( true, true, false, false ) == Transition::None );
    assert( !touch.active() );
    assert( touch.update( true, false, false, true ) == Transition::None );
    assert( touch.update( true, true, true, false ) == Transition::Press );
    assert( touch.update( true, false, false, true ) == Transition::Release );
    // Main menu/battle contacts stay ignored until released in adventure.
    assert( touch.update( false, true, true, false ) == Transition::None );
    assert( touch.update( true, true, false, false ) == Transition::None );
    assert( touch.update( true, false, false, true ) == Transition::None );
    using fheroes2::input3DS::cursorAxis;
    assert( cursorAxis( 400, 0, 0.016, 240 ) == 239 );
    assert( cursorAxis( 42, 0, 0.016, 0 ) == 0 );
    assert( cursorAxis( 42, 156, 0.016, 0 ) == 0 );
    assert( cursorAxis( -10, 0, 0.016, 240 ) == 0 );
    assert( cursorAxis( 42, 20, 0.016, 400 ) == 42 );
    assert( cursorAxis( 42, -20, 0.016, 400 ) == 42 );
    assert( cursorAxis( 398, 156, 0.016, 400 ) == 399 );
    assert( cursorAxis( 1, -156, 0.016, 400 ) == 0 );
    // Pausing a frame cannot send the cursor across the screen on resume.
    assert( cursorAxis( 100, 156, 10, 400 ) == 115 );
    double fast = 100;
    double slow = 100;
    for ( int i = 0; i < 60; ++i )
        fast = cursorAxis( fast, 80, 1.0 / 60, 1000 );
    for ( int i = 0; i < 30; ++i )
        slow = cursorAxis( slow, 80, 1.0 / 30, 1000 );
    assert( std::abs( fast - slow ) < 1e-9 );
    assert( cursorAxis( 0, 21, 0.016, 400 ) > 0 );
}
