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
