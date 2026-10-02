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
// Included inside screen.cpp's anonymous namespace. Display remains the engine's
// indexed software canvas; only presentation depends on libctru.
class RenderCursor final : public fheroes2::Cursor
{
public:
    static RenderCursor * create()
    {
        auto * cursor = new RenderCursor;
        cursor->forceSoftwareEmulation();
        return cursor;
    }
};

class RenderEngine final : public fheroes2::BaseRenderEngine
{
public:
    static RenderEngine * create()
    {
        return new RenderEngine;
    }
    std::vector<fheroes2::ResolutionInfo> getAvailableResolutions() const override
    {
        return { { 640, 480 } };
    }
    fheroes2::Rect getActiveWindowROI() const override
    {
        return { 0, 0, 640, 480 };
    }
    fheroes2::Size getCurrentScreenResolution() const override
    {
        return { 640, 480 };
    }
    bool isFullScreen() const override
    {
        return true;
    }
    void toggleFullScreen() override {}
    void setVSync( const bool enabled ) override
    {
        _vsync = enabled;
    }

protected:
    bool allocate( fheroes2::ResolutionInfo & info, bool ) override
    {
        info = { 640, 480 };
        linkRenderSurface( nullptr );
        for ( size_t i = 0; i < _palette.size(); ++i )
            _palette[i] = static_cast<uint8_t>( i );
        return true;
    }
    void updatePalette( const std::vector<uint8_t> & ids ) override
    {
        if ( ids.size() == _palette.size() )
            std::copy( ids.begin(), ids.end(), _palette.begin() );
    }
    void render( const fheroes2::Display & display, const fheroes2::Rect & ) override
    {
        if ( display.empty() )
            return;
        uint8_t * top = gfxGetFramebuffer( GFX_TOP, GFX_LEFT, nullptr, nullptr );
        uint8_t * bottom = gfxGetFramebuffer( GFX_BOTTOM, GFX_LEFT, nullptr, nullptr );
        std::array<uint32_t, 256> colors{};
        for ( size_t i = 0; i < colors.size(); ++i ) {
            const auto & color = currentRGBPalette[_palette[i]];
            colors[i] = ( uint32_t( color.r ) << 16 ) | ( uint32_t( color.g ) << 8 ) | color.b;
        }
        fheroes2::threeDS::renderFramebuffers( display.image(), display.width(), display.height(), colors, top, bottom, fheroes2::is3DSAdventureLayout(),
                                               fheroes2::get3DSAdventureSnapshot() );
        gfxFlushBuffers();
        gfxSwapBuffers();
        if ( _vsync )
            gspWaitForVBlank();
    }

private:
    std::array<uint8_t, 256> _palette{};
    bool _vsync{ true };
};
