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
#include "ui_3ds.h"

#include <algorithm>
#include <memory>

#include "image.h"
#include "localevent.h"
#include "screen.h"
#include "ui_3ds_assets.h"
#include "ui_text.h"
#include "zzlib.h"

namespace
{
    fheroes2::Sprite artwork( const fheroes2::threeDSUIAssets::Asset & asset, int width, int height )
    {
        fheroes2::Image original( asset.width, asset.height );
        original._disableTransformLayer();
        const auto pixels = Compression::unzipData( asset.data, asset.size, static_cast<size_t>( asset.width ) * asset.height );
        if ( pixels.size() == static_cast<size_t>( asset.width ) * asset.height )
            std::copy( pixels.begin(), pixels.end(), original.image() );
        else
            original.fill( fheroes2::GetColorId( 70, 40, 20 ) );
        fheroes2::Sprite result( width, height, 0, 0 );
        result._disableTransformLayer();
        result.reset();
        fheroes2::Resize( original, result );
        return result;
    }
}

namespace fheroes2
{
    void draw3DSPanel( const Rect & area, const std::string & title )
    {
        const auto & asset = area.width > 320 ? threeDSUIAssets::panel_top : threeDSUIAssets::panel_bottom;
        const auto panel = artwork( asset, area.width, area.height );
        Copy( panel, 0, 0, Display::instance(), area.x, area.y, area.width, area.height );
        if ( !title.empty() ) {
            Text text( title, FontType::normalYellow() );
            text.draw( area.x + 20, area.y + 20, area.width - 40, Display::instance() );
        }
    }

    ButtonSprite make3DSButton( const std::string & label, const Rect & area )
    {
        auto released = artwork( threeDSUIAssets::button_default, area.width, area.height );
        auto pressed = artwork( threeDSUIAssets::button_pressed, area.width, area.height );
        auto focused = artwork( threeDSUIAssets::button_pressed, area.width, area.height );
        auto disabled = artwork( threeDSUIAssets::button_disabled, area.width, area.height );
        Text text( label, FontType::normalWhite() );
        const int y = std::max<int32_t>( 2, ( area.height - text.height( area.width - 12 ) ) / 2 );
        text.draw( 6, y, area.width - 12, released );
        text.draw( 6, y, area.width - 12, focused );
        text.draw( 7, y + 1, area.width - 12, pressed );
        Text inactive( label, FontType::normalWhite() );
        inactive.draw( 6, y, area.width - 12, disabled );
        ButtonSprite button( area.x, area.y, std::move( released ), std::move( pressed ), std::move( disabled ) );
        button.setFocusedSprite( std::move( focused ) );
        return button;
    }

    int show3DSMenu( const std::string & title, const std::vector<std::string> & labels, const std::vector<bool> & enabled, const bool cancel )
    {
        const Scope3DSNativeDialog scope;
        set3DSDialogDimBackground( false );
        set3DSDialogCancelable( cancel );
        draw3DSPanel( { 0, 240, 320, 240 }, title );
        const int columns = labels.size() > 4 ? 2 : 1;
        const int rows = ( static_cast<int>( labels.size() ) + columns - 1 ) / columns;
        const int height = std::min<int32_t>( 42, 176 / std::max<int32_t>( rows, 1 ) - 6 );
        std::vector<std::unique_ptr<ButtonSprite>> buttons;
        for ( size_t i = 0; i < labels.size(); ++i ) {
            const int row = static_cast<int>( i ) / columns;
            const int col = static_cast<int>( i ) % columns;
            const Rect area( columns == 1 ? 24 : 12 + col * 154, 279 + row * ( height + 6 ), columns == 1 ? 272 : 142, height );
            buttons.emplace_back( std::make_unique<ButtonSprite>( make3DSButton( labels[i], area ) ) );
            if ( i < enabled.size() && !enabled[i] )
                buttons.back()->disable();
            buttons.back()->draw();
        }
        Display::instance().render();
        auto & events = LocalEvent::Get();
        while ( events.HandleEvents() ) {
            if ( cancel && events.isKeyPressed( Key::KEY_ESCAPE ) )
                return -1;
            for ( size_t i = 0; i < buttons.size(); ++i ) {
                auto & button = *buttons[i];
                button.drawOnState( events.isMouseLeftButtonPressedAndHeldInArea( button.area() ) );
                button.draw();
                if ( button.isEnabled() && events.MouseClickLeft( button.area() ) )
                    return static_cast<int>( i );
            }
        }
        return -1;
    }
}
