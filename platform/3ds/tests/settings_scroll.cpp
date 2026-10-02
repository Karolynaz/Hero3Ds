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
// Link against native game/engine objects, excluding game/fheroes2.cpp's main.
#include <cassert>

#include "settings.h"

int main()
{
    Settings & settings = Settings::Get();
    assert( settings.ScrollSpeed() == SCROLL_SPEED_NORMAL );
    // Loading the old handheld default must restore physical camera controls.
    settings.SetScrollSpeed( SCROLL_SPEED_NONE );
    assert( settings.ScrollSpeed() == SCROLL_SPEED_NORMAL );
    settings.SetScrollSpeed( -1 );
    assert( settings.ScrollSpeed() == SCROLL_SPEED_NORMAL );
    settings.SetScrollSpeed( SCROLL_SPEED_SLOW );
    assert( settings.ScrollSpeed() == SCROLL_SPEED_SLOW );
    settings.SetScrollSpeed( SCROLL_SPEED_VERY_FAST + 1 );
    assert( settings.ScrollSpeed() == SCROLL_SPEED_VERY_FAST );
}
