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
#include <array>
#include <memory>
#include <vector>
#if defined( TARGET_NINTENDO_3DS )
#include <3ds.h>
#endif

#include "audio.h"
#include "audio_manager.h"
#include "dialog.h"
#include "game.h"
#include "game_delays.h"
#include "image.h"
#include "localevent.h"
#include "pal.h"
#include "screen.h"
#include "settings.h"
#include "translations.h"
#include "ui_3ds_assets.h"
#include "ui_dialog.h"
#include "ui_language.h"
#include "ui_text.h"
#include "zzlib.h"

namespace
{
    const fheroes2::Image & decodeAsset( const fheroes2::threeDSUIAssets::Asset & asset )
    {
        struct CachedSource
        {
            const uint8_t * data{ nullptr };
            fheroes2::Image image;
        };

        static std::vector<CachedSource> cache;
        for ( const auto & entry : cache ) {
            if ( entry.data == asset.data ) {
                return entry.image;
            }
        }

        CachedSource decoded;
        decoded.data = asset.data;
        decoded.image.resize( asset.width, asset.height );
        decoded.image._disableTransformLayer();
        const auto pixels = Compression::unzipData( asset.data, asset.size, static_cast<size_t>( asset.width ) * asset.height );
        if ( pixels.size() == static_cast<size_t>( asset.width ) * asset.height ) {
            std::copy( pixels.begin(), pixels.end(), decoded.image.image() );
        }
        else {
            decoded.image.fill( fheroes2::GetColorId( 70, 40, 20 ) );
        }

        cache.push_back( std::move( decoded ) );
        return cache.back().image;
    }

    fheroes2::Sprite resize9Slice( const fheroes2::Image & src, const int width, const int height, const int border )
    {
        fheroes2::Sprite result( width, height, 0, 0 );
        result._disableTransformLayer();
        if ( width <= 0 || height <= 0 ) {
            return result;
        }

        if ( src.width() == width && src.height() == height ) {
            fheroes2::Copy( src, result );
            return result;
        }

        const int bx = std::min<int>( { border, src.width() / 2 - 1, width / 2 } );
        const int by = std::min<int>( { border, src.height() / 2 - 1, height / 2 } );
        if ( bx <= 0 || by <= 0 ) {
            result.reset();
            fheroes2::Resize( src, result );
            return result;
        }

        const std::array<int, 4> sxBounds{ 0, bx, src.width() - bx, src.width() };
        const std::array<int, 4> syBounds{ 0, by, src.height() - by, src.height() };
        const std::array<int, 4> dxBounds{ 0, bx, width - bx, width };
        const std::array<int, 4> dyBounds{ 0, by, height - by, height };

        const uint8_t * srcPixels = src.image();
        uint8_t * dstPixels = result.image();
        const int srcW = src.width();

        for ( size_t r = 0; r < 3; ++r ) {
            const int dy0 = dyBounds[r];
            const int dh = dyBounds[r + 1] - dy0;
            const int sy0 = syBounds[r];
            const int sh = syBounds[r + 1] - sy0;
            if ( dh <= 0 || sh <= 0 ) {
                continue;
            }
            for ( size_t c = 0; c < 3; ++c ) {
                const int dx0 = dxBounds[c];
                const int dw = dxBounds[c + 1] - dx0;
                const int sx0 = sxBounds[c];
                const int sw = sxBounds[c + 1] - sx0;
                if ( dw <= 0 || sw <= 0 ) {
                    continue;
                }
                for ( int y = 0; y < dh; ++y ) {
                    const int sy = sy0 + ( y * sh ) / dh;
                    const uint8_t * srcRow = srcPixels + static_cast<size_t>( sy ) * srcW + sx0;
                    uint8_t * dstRow = dstPixels + static_cast<size_t>( dy0 + y ) * width + dx0;
                    for ( int x = 0; x < dw; ++x ) {
                        dstRow[x] = srcRow[( x * sw ) / dw];
                    }
                }
            }
        }
        return result;
    }

    fheroes2::Sprite artwork( const fheroes2::threeDSUIAssets::Asset & asset, const int width, const int height, const int border )
    {
        struct CachedScaled
        {
            const uint8_t * data{ nullptr };
            int width{ 0 };
            int height{ 0 };
            int border{ 0 };
            fheroes2::Sprite sprite;
        };

        static std::vector<CachedScaled> cache;
        for ( const auto & entry : cache ) {
            if ( entry.data == asset.data && entry.width == width && entry.height == height && entry.border == border ) {
                return entry.sprite;
            }
        }

        auto sprite = resize9Slice( decodeAsset( asset ), width, height, border );
        if ( cache.size() >= 32 ) {
            cache.erase( cache.begin() );
        }
        cache.push_back( { asset.data, width, height, border, sprite } );
        return sprite;
    }
}

namespace fheroes2
{
    void draw3DSPanel( const Rect & area, const std::string & title )
    {
        const auto & asset = area.width > 320 ? threeDSUIAssets::panel_top : threeDSUIAssets::panel_bottom;
        const auto panel = artwork( asset, area.width, area.height, 24 );
        Copy( panel, 0, 0, Display::instance(), area.x, area.y, area.width, area.height );
        if ( !title.empty() ) {
            Text text( title, FontType::normalYellow() );
            text.draw( area.x + 20, area.y + 20, area.width - 40, Display::instance() );
        }
    }

    ButtonSprite make3DSButton( const std::string & label, const Rect & area )
    {
        auto released = artwork( threeDSUIAssets::button_default, area.width, area.height, 10 );
        auto focused = artwork( threeDSUIAssets::button_focused, area.width, area.height, 10 );
        auto pressed = artwork( threeDSUIAssets::button_pressed, area.width, area.height, 10 );
        auto disabled = artwork( threeDSUIAssets::button_disabled, area.width, area.height, 10 );
        const Text releasedText( label, FontType::normalWhite() );
        const Text focusedText( label, FontType::normalYellow() );
        const Text disabledText( label, FontType( FontSize::NORMAL, FontColor::GRAY ) );
        const int y = std::max<int32_t>( 2, ( area.height - releasedText.height( area.width - 12 ) ) / 2 );
        releasedText.draw( 6, y, area.width - 12, released );
        focusedText.draw( 6, y, area.width - 12, focused );
        releasedText.draw( 7, y + 1, area.width - 12, pressed );
        disabledText.draw( 6, y, area.width - 12, disabled );
        ButtonSprite button( area.x, area.y, std::move( released ), std::move( pressed ), std::move( disabled ) );
        button.setFocusedSprite( std::move( focused ) );
        return button;
    }

    void drawFittedSprite3DS( const Image & src, Image & dst, const Rect & area )
    {
        if ( src.empty() || src.width() <= 0 || src.height() <= 0 || area.width <= 0 || area.height <= 0 ) {
            return;
        }

        int32_t cropX = 0;
        int32_t cropY = 0;
        int32_t cropW = src.width();
        int32_t cropH = src.height();

        // Trim transparent padding and discard isolated stray 1-2px ICN artifacts (e.g. ranged projectile dots)
        // separated from the dense main creature body by a >= 6px empty gap.
        if ( !src.singleLayer() ) {
            const uint8_t * tr = src.transform();
            const int32_t w = src.width();
            const int32_t h = src.height();
            std::vector<int32_t> colCounts( w, 0 );
            std::vector<int32_t> rowCounts( h, 0 );
            for ( int32_t y = 0; y < h; ++y ) {
                const uint8_t * row = tr + static_cast<size_t>( y ) * w;
                for ( int32_t x = 0; x < w; ++x ) {
                    if ( row[x] != 1 ) {
                        ++colCounts[x];
                        ++rowCounts[y];
                    }
                }
            }
            const auto findCoreSpan = []( const std::vector<int32_t> & counts ) -> std::pair<int32_t, int32_t> {
                const int32_t n = static_cast<int32_t>( counts.size() );
                int32_t peakIdx = 0;
                for ( int32_t i = 1; i < n; ++i ) {
                    if ( counts[i] > counts[peakIdx] ) {
                        peakIdx = i;
                    }
                }
                if ( counts[peakIdx] == 0 ) {
                    return { 0, n - 1 };
                }
                int32_t lo = peakIdx;
                int32_t gap = 0;
                for ( int32_t i = peakIdx - 1; i >= 0; --i ) {
                    if ( counts[i] > 0 ) {
                        if ( gap >= 6 && counts[i] <= 3 ) {
                            break;
                        }
                        lo = i;
                        gap = 0;
                    }
                    else {
                        ++gap;
                    }
                }
                int32_t hi = peakIdx;
                gap = 0;
                for ( int32_t i = peakIdx + 1; i < n; ++i ) {
                    if ( counts[i] > 0 ) {
                        if ( gap >= 6 && counts[i] <= 3 ) {
                            break;
                        }
                        hi = i;
                        gap = 0;
                    }
                    else {
                        ++gap;
                    }
                }
                return { lo, hi };
            };

            const auto [minX, maxX] = findCoreSpan( colCounts );
            const auto [minY, maxY] = findCoreSpan( rowCounts );
            cropX = minX;
            cropY = minY;
            cropW = std::max<int32_t>( 1, maxX - minX + 1 );
            cropH = std::max<int32_t>( 1, maxY - minY + 1 );
        }

        const Sprite cropped = ( cropX == 0 && cropY == 0 && cropW == src.width() && cropH == src.height() ) ? Sprite() : Crop( src, cropX, cropY, cropW, cropH );
        const Image & drawSrc = cropped.empty() ? src : static_cast<const Image &>( cropped );

        if ( drawSrc.width() <= area.width && drawSrc.height() <= area.height ) {
            Blit( drawSrc, dst, area.x + ( area.width - drawSrc.width() ) / 2, area.y + ( area.height - drawSrc.height() ) / 2 );
            return;
        }

        const double scale = std::min( static_cast<double>( area.width ) / drawSrc.width(), static_cast<double>( area.height ) / drawSrc.height() );
        const int32_t dw = std::max<int32_t>( 1, static_cast<int32_t>( drawSrc.width() * scale ) );
        const int32_t dh = std::max<int32_t>( 1, static_cast<int32_t>( drawSrc.height() * scale ) );
        Image scaled( dw, dh );
        scaled.reset();
        Resize( drawSrc, scaled );
        Blit( scaled, dst, area.x + ( area.width - dw ) / 2, area.y + ( area.height - dh ) / 2 );
    }

    int show3DSMenu( const std::string & title, const std::vector<std::string> & labels, const std::vector<bool> & enabled, const bool cancel,
                     const std::string & information, const int initialSelection )
    {
        if ( labels.size() > 6 ) {
            size_t first = static_cast<size_t>( std::max( 0, initialSelection ) ) / 4 * 4;
            if ( first >= labels.size() ) {
                first = 0;
            }
            while ( true ) {
                const size_t count = std::min<size_t>( 4, labels.size() - first );
                std::vector<std::string> pageLabels( labels.begin() + first, labels.begin() + first + count );
                std::vector<bool> pageEnabled;
                for ( size_t i = first; i < first + count; ++i ) {
                    pageEnabled.push_back( i >= enabled.size() || enabled[i] );
                }
                pageLabels.emplace_back( _( "Previous page" ) );
                pageLabels.emplace_back( _( "Next page" ) );
                pageEnabled.push_back( first > 0 );
                pageEnabled.push_back( first + count < labels.size() );
                const int requested = initialSelection - static_cast<int>( first );
                const int selected
                    = show3DSMenu( title, pageLabels, pageEnabled, cancel, information, requested >= 0 && static_cast<size_t>( requested ) < count ? requested : 0 );
                if ( selected < 0 ) {
                    return -1;
                }
                if ( static_cast<size_t>( selected ) < count ) {
                    return static_cast<int>( first ) + selected;
                }
                if ( static_cast<size_t>( selected ) == count && first > 0 ) {
                    first -= 4;
                }
                else if ( first + count < labels.size() ) {
                    first += count;
                }
            }
        }
        const Scope3DSNativeDialog scope( !information.empty() );
        const Text infoText( information, FontType::normalWhite() );
        const Rect infoRoi{ 30, 60, 340, 140 };
        const int maximumScroll = information.empty() ? 0 : std::max<int32_t>( 0, infoText.height( infoRoi.width ) - infoRoi.height );
        int informationScroll = 0;
        if ( !information.empty() ) {
            draw3DSPanel( { 10, 10, 380, 220 }, title );
        }
        ImageRestorer infoBackground( Display::instance(), infoRoi.x, infoRoi.y, infoRoi.width, infoRoi.height );
        const auto drawInformation = [&]() {
            if ( information.empty() ) {
                return;
            }
            infoBackground.restore();
            infoText.drawInRoi( infoRoi.x, infoRoi.y - informationScroll, infoRoi.width, Display::instance(), infoRoi );
        };
        drawInformation();
        set3DSDialogDimBackground( false );
        set3DSDialogCancelable( cancel );
        draw3DSPanel( { 0, 240, 320, 240 }, title );
        const int columns = labels.size() > 4 ? 2 : 1;
        const int rows = ( static_cast<int>( labels.size() ) + columns - 1 ) / columns;
        const int height = std::min<int32_t>( 42, 176 / std::max<int32_t>( rows, 1 ) - 6 );
        std::vector<std::unique_ptr<ButtonSprite>> buttons;
        std::vector<const ButtonBase *> focusButtons;
        buttons.reserve( labels.size() );
        focusButtons.reserve( labels.size() );
        for ( size_t i = 0; i < labels.size(); ++i ) {
            const int row = static_cast<int>( i ) / columns;
            const int col = static_cast<int>( i ) % columns;
            const Rect area( columns == 1 ? 24 : 8 + col * 156, 279 + row * ( height + 6 ), columns == 1 ? 272 : 148, height );
            buttons.emplace_back( std::make_unique<ButtonSprite>( make3DSButton( labels[i], area ) ) );
            if ( i < enabled.size() && !enabled[i] )
                buttons.back()->disable();
            buttons.back()->draw();
            focusButtons.push_back( buttons.back().get() );
        }
        if ( initialSelection >= 0 && static_cast<size_t>( initialSelection ) < buttons.size() && buttons[initialSelection]->isEnabled() ) {
            const void * previousFocus = get3DSDialogFocus();
            set3DSDialogFocus( buttons[initialSelection].get() );
            for ( auto & button : buttons ) {
                if ( button.get() == previousFocus || button.get() == buttons[initialSelection].get() ) {
                    button->draw();
                }
            }
        }
        if ( maximumScroll > 0 ) {
            Text( _( "L/R: scroll information" ), FontType::smallWhite() ).draw( 24, 456, 272, Display::instance() );
        }
        Display::instance().render();
        const void * lastFocus = get3DSDialogFocus();
        auto & events = LocalEvent::Get();
        while ( events.HandleEvents() ) {
#if defined( TARGET_NINTENDO_3DS )
            if ( maximumScroll > 0 ) {
                const auto down = hidKeysDown();
                const int previousScroll = informationScroll;
                if ( down & KEY_L ) {
                    informationScroll = std::max( 0, informationScroll - 17 );
                }
                if ( down & KEY_R ) {
                    informationScroll = std::min( maximumScroll, informationScroll + 17 );
                }
                if ( informationScroll != previousScroll ) {
                    drawInformation();
                    Display::instance().render();
                }
            }
            sync3DSDialogFocus( lastFocus, focusButtons.data(), focusButtons.size() );
#endif
            if ( cancel && events.isKeyPressed( Key::KEY_ESCAPE ) )
                return -1;
            for ( size_t i = 0; i < buttons.size(); ++i ) {
                auto & button = *buttons[i];
                button.drawOnState( events.isMouseLeftButtonPressedAndHeldInArea( button.area() ) );
                if ( button.isEnabled() && events.MouseClickLeft( button.area() ) )
                    return static_cast<int>( i );
            }
        }
        return -1;
    }
    bool show3DSAudioSettings( const bool fromAdventureMap )
    {
        auto & conf = Settings::Get();
        bool changed = false;
        int page = 0;
        int initialFocus = 0;
        while ( true ) {
            const std::string information = std::string( _( "Music" ) ) + ": " + std::to_string( conf.MusicVolume() ) + "/10\n" + _( "Effects" ) + ": "
                                            + std::to_string( conf.SoundVolume() ) + "/10\n\n" + _( "Select a volume to change it. Values cycle from 0 (off) to 10." );
            const int choice
                = show3DSMenu( _( "Audio Settings" ),
                               page == 0 ? std::vector<std::string>{ _( "Music" ) + std::string( ": " ) + std::to_string( conf.MusicVolume() ),
                                                                     _( "Effects" ) + std::string( ": " ) + std::to_string( conf.SoundVolume() ), _( "More" ) }
                                         : std::vector<std::string>{ _( "Music Type" ) + std::string( ": " )
                                                                         + ( conf.MusicType() == MUSIC_EXTERNAL ? _( "External" ) : _( "MIDI" ) ),
                                                                     _( "3D Audio" ) + std::string( ": " ) + ( conf.is3DAudioEnabled() ? _( "On" ) : _( "Off" ) ),
                                                                     _( "Back" ) },
                               {}, true, information, initialFocus );
            if ( choice < 0 ) {
                return changed;
            }
            if ( choice == 2 ) {
                page = 1 - page;
                initialFocus = 0;
                continue;
            }
            initialFocus = choice;
            if ( page == 0 && choice == 0 ) {
                conf.SetMusicVolume( ( conf.MusicVolume() + 1 ) % 11 );
                Music::setVolume( 100 * conf.MusicVolume() / 10 );
            }
            else if ( page == 0 ) {
                conf.SetSoundVolume( ( conf.SoundVolume() + 1 ) % 11 );
                Mixer::setVolume( 100 * conf.SoundVolume() / 10 );
            }
            else if ( choice == 0 ) {
                int type = conf.MusicType() + 1;
                if ( type == MUSIC_MIDI_EXPANSION && !conf.isPriceOfLoyaltySupported() ) {
                    ++type;
                }
                conf.SetMusicType( type > MUSIC_EXTERNAL ? 0 : type );
                AudioManager::PlayCurrentMusic();
            }
            else {
                conf.set3DAudio( !conf.is3DAudioEnabled() );
                if ( fromAdventureMap ) {
                    Game::EnvironmentSoundMixer();
                }
            }
            changed = true;
        }
    }

    void show3DSSettings( const bool fromAdventureMap )
    {
        auto & conf = Settings::Get();
        int page = 0;
        int initialFocus = 0;
        bool changed = false;
        while ( true ) {
            std::vector<std::string> labels;
            std::string information;
            const std::string & lang = conf.getGameLanguage().empty() ? std::string( _( "English" ) ) : conf.getGameLanguage();
            if ( page == 0 ) {
                labels = { _( "Language" ), _( "Audio Settings" ), _( "Controls" ), _( "Next page" ) };
                information = _(
                    "Choose a setting on the lower screen.\n\nUse the D-pad or Circle Pad to select a button and A to confirm.\n\nB returns to the game or main menu." );
            }
            else if ( page == 1 ) {
                labels = { _( "Interface" ) + std::string( ": " )
                               + ( conf.getInterfaceType() == InterfaceType::DYNAMIC ? _( "Auto" )
                                   : conf.getInterfaceType() == InterfaceType::EVIL  ? _( "Evil" )
                                                                                     : _( "Good" ) ),
                           _( "Cursor" ) + std::string( ": " ) + ( conf.isMonochromeCursorEnabled() ? _( "Black & White" ) : _( "Color" ) ),
                           _( "Text Support Mode" ) + std::string( ": " ) + ( conf.isTextSupportModeEnabled() ? _( "On" ) : _( "Off" ) ), _( "Next page" ) };
                information = std::string( _( "Language" ) ) + ": " + lang + "\n\n"
                              + _( "Change the interface theme, pointer appearance, or text support mode." );
            }
            else if ( page == 2 ) {
                const std::string battleState
                    = conf.BattleAutoResolve() ? ( conf.BattleAutoSpellcast() ? _( "Auto" ) : _( "Auto (No Spells)" ) ) : _( "Manual" );
                labels = { _( "Hero Speed" ) + std::string( ": " ) + std::to_string( conf.HeroesMoveSpeed() ),
                           _( "Enemy Speed" ) + std::string( ": " ) + std::to_string( conf.AIMoveSpeed() ),
                           _( "Battles" ) + std::string( ": " ) + battleState, _( "Next page" ) };
                information = std::string( _( "Hero Speed" ) ) + ": " + std::to_string( conf.HeroesMoveSpeed() ) + "/10\n" + _( "Enemy Speed" ) + ": "
                              + std::to_string( conf.AIMoveSpeed() ) + "/10\n\n" + _( "Battles" ) + ": " + battleState + "\n"
                              + _( "Each selection cycles to the next setting." );
            }
            else {
                labels = { _( "Battle Speed" ) + std::string( ": " ) + std::to_string( conf.BattleSpeed() ),
                           _( "Grid" ) + std::string( ": " ) + ( conf.BattleShowGrid() ? _( "On" ) : _( "Off" ) ),
                           _( "Damage Info" ) + std::string( ": " ) + ( conf.isBattleShowDamageInfoEnabled() ? _( "On" ) : _( "Off" ) ), _( "First page" ) };
                information = _( "Adjust battle animation speed, show the battlefield grid, or display attack damage information." );
            }
            const int choice = show3DSMenu( _( "Settings" ), labels, {}, true, information, initialFocus );
            if ( choice < 0 ) {
                break;
            }
            if ( choice == 3 ) {
                page = ( page + 1 ) % 4;
                initialFocus = 0;
                continue;
            }
            initialFocus = choice;
            if ( page == 0 && choice == 0 ) {
                const auto languages = getSupportedLanguages();
                if ( !languages.empty() ) {
                    size_t first = 0;
                    while ( true ) {
                        const size_t count = std::min<size_t>( 5, languages.size() - first );
                        std::vector<std::string> languageLabels;
                        for ( size_t i = 0; i < count; ++i ) {
                            languageLabels.emplace_back( getLanguageAbbreviation( languages[first + i] ) );
                        }
                        if ( languages.size() > 5 ) {
                            languageLabels.emplace_back( _( "Next page" ) );
                        }
                        const int selection = show3DSMenu( _( "Language" ), languageLabels, {}, true, _( "Select the game language." ) );
                        if ( selection < 0 ) {
                            break;
                        }
                        if ( static_cast<size_t>( selection ) == count ) {
                            first = first + count >= languages.size() ? 0 : first + count;
                            continue;
                        }
                        conf.setGameLanguage( getLanguageAbbreviation( languages[first + selection] ) );
                        changed = true;
                        break;
                    }
                }
            }
            else if ( page == 0 && choice == 1 ) {
                changed |= show3DSAudioSettings( fromAdventureMap );
            }
            else if ( page == 0 ) {
                show3DSMenu(
                    _( "Controls" ), { _( "Okay" ) }, {}, true,
                    _( "Circle Pad: pointer | D-pad: pan/menu | A: confirm | B: back | R: inspect.\nWorld: Y Hero, L Town, X End Turn, Select File.\nCastle: X Build, Y Well, L Town, Select Menu.\nBattle: L Side, X Spell, Y Skip, Select Menu." ) );
            }
            else if ( page == 1 ) {
                if ( choice == 0 ) {
                    conf.switchToNextInterfaceType();
                }
                else if ( choice == 1 ) {
                    conf.setMonochromeCursor( !conf.isMonochromeCursorEnabled() );
                }
                else {
                    conf.setTextSupportMode( !conf.isTextSupportModeEnabled() );
                }
                changed = true;
            }
            else if ( page == 2 ) {
                if ( choice == 0 ) {
                    conf.SetHeroesMoveSpeed( ( conf.HeroesMoveSpeed() + 1 ) % 11 );
                }
                else if ( choice == 1 ) {
                    conf.SetAIMoveSpeed( ( conf.AIMoveSpeed() + 1 ) % 11 );
                }
                else if ( !conf.BattleAutoResolve() ) {
                    conf.setBattleAutoResolve( true );
                    conf.setBattleAutoSpellcast( true );
                }
                else if ( conf.BattleAutoSpellcast() ) {
                    conf.setBattleAutoSpellcast( false );
                }
                else {
                    conf.setBattleAutoResolve( false );
                }
                Game::UpdateGameSpeed();
                changed = true;
            }
            else {
                if ( choice == 0 ) {
                    conf.SetBattleSpeed( conf.BattleSpeed() % 10 + 1 );
                    Game::UpdateGameSpeed();
                }
                else if ( choice == 1 ) {
                    conf.SetBattleGrid( !conf.BattleShowGrid() );
                }
                else {
                    conf.setBattleDamageInfo( !conf.isBattleShowDamageInfoEnabled() );
                }
                changed = true;
            }
        }
        if ( changed ) {
            conf.Save( Settings::configFileName );
        }
    }

}
