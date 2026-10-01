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
#include "viewport.h"
namespace fh3ds
{
    enum class Action
    {
        None,
        LeftClick,
        RightClick
    };
    struct InputState
    {
        Point cursor{ 320, 240 };
        bool touching = false;
        Point previousTouch{ 0, 0 };
        Action touch( int x, int y, bool down )
        {
            if ( !down ) {
                touching = false;
                return Action::None;
            }
            const bool started = !touching;
            touching = true;
            if ( y >= 200 ) {
                previousTouch = { x, y };
                return started ? ( x < 160 ? Action::LeftClick : Action::RightClick ) : Action::None;
            }
            if ( !started && previousTouch.y < 200 ) {
                cursor.x = std::clamp( cursor.x + ( x - previousTouch.x ) * 2, 0, 639 );
                cursor.y = std::clamp( cursor.y + ( y - previousTouch.y ) * 2, 0, 479 );
            }
            previousTouch = { x, y };
            return Action::None;
        }
        void move( int dx, int dy )
        {
            cursor.x = std::clamp( cursor.x + dx, 0, 639 );
            cursor.y = std::clamp( cursor.y + dy, 0, 479 );
        }
    };
}
