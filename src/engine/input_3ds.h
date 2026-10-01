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
#include <cmath>

namespace fheroes2
{
    namespace input3DS
    {
        // Circle Pad samples nominally range from -156 to 156. Preserve a
        // fractional cursor position so a lightly tilted pad still moves.
        inline double cursorAxis( const double position, const int axis, const double seconds, const int extent )
        {
            constexpr int deadZone = 20;
            const int magnitude = std::min( std::abs( axis ), 156 );
            const double maximum = static_cast<double>( std::max( extent - 1, 0 ) );
            if ( magnitude <= deadZone ) {
                return std::clamp( position, 0.0, maximum );
            }
            const double velocity = ( magnitude - deadZone ) * 300.0 / ( 156 - deadZone );
            const double next = position + ( axis < 0 ? -velocity : velocity ) * std::clamp( seconds, 0.0, 0.05 );
            return std::clamp( next, 0.0, maximum );
        }
    }
}
