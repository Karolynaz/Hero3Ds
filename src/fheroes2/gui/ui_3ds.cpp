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
        auto pressed = released;
        auto focused = released;
        auto disabled = released;
        ApplyPalette( pressed, PAL::GetPalette( PAL::PaletteType::DARKENING ) );
        ApplyPalette( focused, PAL::GetPalette( PAL::PaletteType::DARKENING ) );
        ApplyPalette( disabled, PAL::GetPalette( PAL::PaletteType::DARKENING ) );
        ApplyPalette( disabled, PAL::GetPalette( PAL::PaletteType::DARKENING ) );
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
        for ( size_t i = 0; i < labels.size(); ++i ) {
            const int row = static_cast<int>( i ) / columns;
            const int col = static_cast<int>( i ) % columns;
            const Rect area( columns == 1 ? 24 : 12 + col * 154, 279 + row * ( height + 6 ), columns == 1 ? 272 : 142, height );
            buttons.emplace_back( std::make_unique<ButtonSprite>( make3DSButton( labels[i], area ) ) );
            if ( i < enabled.size() && !enabled[i] )
                buttons.back()->disable();
            buttons.back()->draw();
        }
        if ( initialSelection >= 0 && static_cast<size_t>( initialSelection ) < buttons.size() && buttons[initialSelection]->isEnabled() ) {
            set3DSDialogFocus( buttons[initialSelection].get() );
            buttons[initialSelection]->draw();
        }
        if ( maximumScroll > 0 ) {
            Text( _( "L/R: scroll information" ), FontType::smallWhite() ).draw( 24, 456, 272, Display::instance() );
        }
        Display::instance().render();
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
#endif
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
    bool show3DSAudioSettings( const bool fromAdventureMap )
    {
        auto & conf = Settings::Get();
        bool changed = false;
        int page = 0;
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
                               {}, true, information );
            if ( choice < 0 ) {
                return changed;
            }
            if ( choice == 2 ) {
                page = 1 - page;
                continue;
            }
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
        bool changed = false;
        while ( true ) {
            std::vector<std::string> labels;
            std::string information;
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
                information = std::string( _( "Language" ) ) + ": " + conf.getGameLanguage() + "\n\n"
                              + _( "Change the interface theme, pointer appearance, or text support mode." );
            }
            else if ( page == 2 ) {
                labels = { _( "Hero Speed" ) + std::string( ": " ) + std::to_string( conf.HeroesMoveSpeed() ),
                           _( "Enemy Speed" ) + std::string( ": " ) + std::to_string( conf.AIMoveSpeed() ), _( "Battles" ), _( "Next page" ) };
                information = std::string( _( "Hero Speed" ) ) + ": " + std::to_string( conf.HeroesMoveSpeed() ) + "/10\n" + _( "Enemy Speed" ) + ": "
                              + std::to_string( conf.AIMoveSpeed() ) + "/10\n\n" + _( "Battles" ) + ": " + ( conf.BattleAutoResolve() ? _( "Auto" ) : _( "Manual" ) )
                              + "\n" + _( "Each selection cycles to the next setting." );
            }
            else {
                labels = { _( "Battle Speed" ) + std::string( ": " ) + std::to_string( conf.BattleSpeed() ),
                           _( "Grid" ) + std::string( ": " ) + ( conf.BattleShowGrid() ? _( "On" ) : _( "Off" ) ),
                           _( "Damage Info" ) + std::string( ": " ) + ( conf.isBattleShowDamageInfoEnabled() ? _( "On" ) : _( "Off" ) ), _( "First page" ) };
                information = _( "Adjust battle animation speed, show the battlefield grid, or display attack damage information." );
            }
            const int choice = show3DSMenu( _( "Settings" ), labels, {}, true, information );
            if ( choice < 0 ) {
                break;
            }
            if ( choice == 3 ) {
                page = ( page + 1 ) % 4;
                continue;
            }
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
                showStandardTextMessage( _( "Controls" ), _( "Circle Pad moves the pointer. D-pad moves the camera or selects buttons. A confirms. B cancels." ),
                                         Dialog::OK );
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
