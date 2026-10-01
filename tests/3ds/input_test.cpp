// SPDX-License-Identifier: GPL-2.0-or-later
#include "input_3ds.h"
#include <cassert>
#include <cmath>

int main()
{
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
    for ( int i = 0; i < 60; ++i ) fast = cursorAxis( fast, 80, 1.0 / 60, 1000 );
    for ( int i = 0; i < 30; ++i ) slow = cursorAxis( slow, 80, 1.0 / 30, 1000 );
    assert( std::abs( fast - slow ) < 1e-9 );
    assert( cursorAxis( 0, 21, 0.016, 400 ) > 0 );
}
