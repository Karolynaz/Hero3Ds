/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2019 - 2026                                             *
 *                                                                         *
 *   Free Heroes2 Engine: http://sourceforge.net/projects/fheroes2         *
 *   Copyright (C) 2009 by Andrey Afletdinov <fheroes2@gmail.com>          *
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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "castle.h" // IWYU pragma: associated
#include "cursor.h"
#include "dialog.h"
#include "game_assets.h"
#include "game_hotkeys.h"
#include "heroes.h"
#include "icn.h"
#include "image.h"
#include "kingdom.h"
#include "localevent.h"
#include "mageguild.h"
#include "math_base.h"
#include "pal.h"
#include "screen.h"
#include "settings.h"
#include "spell.h"
#include "spell_storage.h"
#include "statusbar.h"
#include "timing.h"
#include "translations.h"
#include "ui_3ds.h"
#include "ui_button.h"
#include "ui_dialog.h"
#include "ui_mage_guild.h"
#include "ui_text.h"
#include "ui_tool.h"

namespace
{
    const int32_t bottomBarOffsetY = 461;
}

Castle::MageGuildDialogResult Castle::_openMageGuild( const Heroes * hero ) const
{
#if defined( TARGET_NINTENDO_3DS )
    {
        const bool isEvil = Settings::Get().isEvilInterfaceEnabled();
        fheroes2::Display & display = fheroes2::Display::instance();
        const fheroes2::ImageRestorer screenRestorer( display, 0, 0, display.width(), display.height() );
        const fheroes2::Scope3DSCastleBuildView mageViewScope;

        const int guildLevel = GetLevelMageGuild();
        const bool haveLibraryCapability = HaveLibraryCapability();
        const bool hasLibrary = isLibraryBuilt();
        const bool canSwitchCastles = ( GetKingdom().GetCastles().size() > 1 );

        struct SpellSlot3DS final
        {
            int level{ 1 };
            Spell spell;
            fheroes2::Rect rect;
        };

        std::array<std::vector<SpellSlot3DS>, 5> levelSlots;
        for ( size_t lv = 0; lv < 5; ++lv ) {
            const int32_t spellsLevel = static_cast<int32_t>( lv ) + 1;
            const int32_t count = MageGuild::getMaxSpellsCount( spellsLevel, haveLibraryCapability );
            SpellStorage spells = GetMageGuild().GetSpells( guildLevel, hasLibrary, spellsLevel );
            spells.resize( count, Spell::NONE );

            const int32_t rowY = 25 + static_cast<int32_t>( lv ) * 42;
            const int32_t cardW = ( count > 0 ) ? std::min<int32_t>( 86, ( 354 - ( count - 1 ) * 3 ) / count ) : 86;
            for ( int32_t c = 0; c < count; ++c ) {
                const int32_t cx = 40 + c * ( cardW + 3 );
                levelSlots[lv].push_back( { spellsLevel, spells[c], { cx, rowY, cardW, 38 } } );
            }
        }

        size_t selRow = 0;
        size_t selCol = 0;

        const fheroes2::Sprite & stoneTile = Assets::getImage( isEvil ? ICN::STONEBAK_EVIL : ICN::STONEBAK, 0 );
        fheroes2::Image topBg( 400, 240 );
        topBg.reset();
        for ( int32_t y = 0; y < 240; y += stoneTile.height() ) {
            const int32_t h = std::min<int32_t>( stoneTile.height(), 240 - y );
            for ( int32_t x = 0; x < 400; x += stoneTile.width() ) {
                const int32_t w = std::min<int32_t>( stoneTile.width(), 400 - x );
                fheroes2::Copy( stoneTile, 0, 0, topBg, x, y, w, h );
            }
        }

        auto renderTopGuild = [&]() {
            fheroes2::Copy( topBg, 0, 0, display, 0, 0, 400, 240 );

            std::string topTitle = _( "Mage Guild, Level %{level}" );
            StringReplace( topTitle, "%{level}", guildLevel );
            if ( hasLibrary ) {
                topTitle += " + ";
                topTitle += _( "Library" );
            }
            const fheroes2::Text hdrTxt( topTitle, fheroes2::FontType::normalYellow() );
            hdrTxt.draw( 8, 5, display );

            if ( canSwitchCastles ) {
                const fheroes2::Text lrHint( _( "L/R: Town" ), fheroes2::FontType::smallWhite() );
                lrHint.draw( 392 - lrHint.width(), 8, display );
            }

            const uint8_t cardBg = fheroes2::GetColorId( 0x28, 0x16, 0x0C );
            const uint8_t lockedBg = fheroes2::GetColorId( 0x1A, 0x10, 0x08 );
            const uint8_t brightGold = fheroes2::GetColorId( 0xFF, 0xEA, 0x40 );
            const uint8_t warmGold = fheroes2::GetColorId( 0xC0, 0x94, 0x34 );
            const uint8_t darkFrame = fheroes2::GetColorId( 0x16, 0x0C, 0x06 );

            for ( size_t lv = 0; lv < 5; ++lv ) {
                const int32_t rowY = 25 + static_cast<int32_t>( lv ) * 42;
                const bool unlocked = static_cast<int32_t>( lv ) + 1 <= guildLevel;

                // Level label badge
                fheroes2::Fill( display, 6, rowY, 30, 38, unlocked ? fheroes2::GetColorId( 0x34, 0x1E, 0x10 ) : lockedBg );
                fheroes2::DrawRect( display, { 6, rowY, 30, 38 }, unlocked ? warmGold : darkFrame );
                const fheroes2::Text lvTxt( "Lv." + std::to_string( lv + 1 ), unlocked ? fheroes2::FontType::smallYellow() : fheroes2::FontType::smallWhite() );
                lvTxt.draw( 6 + ( 30 - lvTxt.width() ) / 2, rowY + 14, display );

                const auto & row = levelSlots[lv];
                for ( size_t c = 0; c < row.size(); ++c ) {
                    const SpellSlot3DS & slot = row[c];
                    const bool isSel = ( lv == selRow && c == selCol );
                    const bool hasSpell = slot.spell.isValid();

                    fheroes2::Fill( display, slot.rect.x, slot.rect.y, slot.rect.width, slot.rect.height, hasSpell ? cardBg : lockedBg );

                    if ( hasSpell ) {
                        const fheroes2::Sprite & spSpr = Assets::getImage( ICN::SPELLS, slot.spell.IndexSprite() );
                        if ( !spSpr.empty() && spSpr.width() > 0 && spSpr.height() > 0 ) {
                            const int32_t iconMax = 26;
                            const double scale = std::min( static_cast<double>( iconMax ) / spSpr.width(), static_cast<double>( iconMax ) / spSpr.height() );
                            const int32_t dw = std::max<int32_t>( 1, static_cast<int32_t>( spSpr.width() * scale ) );
                            const int32_t dh = std::max<int32_t>( 1, static_cast<int32_t>( spSpr.height() * scale ) );
                            fheroes2::Image scaled( dw, dh );
                            scaled.reset();
                            fheroes2::Resize( spSpr, scaled );
                            fheroes2::Blit( scaled, display, slot.rect.x + 3 + ( iconMax - dw ) / 2, slot.rect.y + 6 + ( iconMax - dh ) / 2 );
                        }

                        const int32_t nameX = slot.rect.x + 31;
                        const int32_t nameW = std::max<int32_t>( 10, slot.rect.width - 34 );
                        const fheroes2::Text spName( slot.spell.GetName(), fheroes2::FontType::smallWhite() );
                        spName.drawInRoi( nameX, slot.rect.y + 5, nameW, display, { nameX, slot.rect.y + 3, nameW, slot.rect.height - 6 } );
                    }
                    else {
                        const fheroes2::Text emptyTxt( unlocked ? "-" : _( "Locked" ), fheroes2::FontType::smallWhite() );
                        emptyTxt.draw( slot.rect.x + ( slot.rect.width - emptyTxt.width() ) / 2, slot.rect.y + 14, display );
                    }

                    if ( isSel ) {
                        fheroes2::DrawRect( display, slot.rect, brightGold );
                        fheroes2::DrawRect( display, { slot.rect.x + 1, slot.rect.y + 1, slot.rect.width - 2, slot.rect.height - 2 }, warmGold );
                    }
                    else {
                        fheroes2::DrawRect( display, slot.rect, unlocked ? warmGold : darkFrame );
                    }
                }
            }
        };

        std::string defaultStatusBarText = ( hero == nullptr || !hero->HaveSpellBook() )
                                               ? _( "The above spells are available here." )
                                               : _( "The spells the hero can learn have been added to their book." );

        fheroes2::ButtonSprite btnExit = fheroes2::make3DSButton( _( "Exit" ), { 101, 422, 118, 30 } );
        const fheroes2::Rect descBoxRect( 24, 334, 272, 42 );
        fheroes2::Image descBoxBg;
        std::string activeDescStr;
        int32_t descMaxScroll = 0;
        int32_t descLastOffset = -1;
        fheroes2::Time descTimer;

        auto drawScrollingDesc = [&]( const bool forceDraw ) -> bool {
            if ( activeDescStr.empty() || descBoxBg.empty() ) {
                return false;
            }
            int32_t offsetY = 0;
            if ( descMaxScroll > 0 ) {
                constexpr double pauseSec = 1.4;
                constexpr double scrollSpeed = 16.0;
                const double scrollSec = static_cast<double>( descMaxScroll ) / scrollSpeed;
                const double cycleSec = pauseSec + scrollSec + pauseSec;
                const double phase = std::fmod( descTimer.getS(), cycleSec );
                if ( phase < pauseSec ) {
                    offsetY = 0;
                }
                else if ( phase < pauseSec + scrollSec ) {
                    offsetY = std::min<int32_t>( descMaxScroll, static_cast<int32_t>( ( phase - pauseSec ) * scrollSpeed ) );
                }
                else {
                    offsetY = descMaxScroll;
                }
            }
            if ( !forceDraw && offsetY == descLastOffset ) {
                return false;
            }
            fheroes2::Copy( descBoxBg, 0, 0, display, descBoxRect.x, descBoxRect.y, descBoxRect.width, descBoxRect.height );
            const fheroes2::Text dTxt( activeDescStr, fheroes2::FontType::smallWhite() );
            dTxt.drawInRoi( descBoxRect.x, descBoxRect.y - offsetY, descBoxRect.width, display, descBoxRect );
            descLastOffset = offsetY;
            return true;
        };

        auto renderBottomGuild = [&]() {
            fheroes2::draw3DSPanel( { 0, 240, 320, 240 } );
            fheroes2::Text cName( GetName(), fheroes2::FontType::normalYellow() );
            if ( cName.width() > 280 ) {
                cName.fitToOneRow( 280 );
            }
            cName.draw( ( 320 - cName.width() ) / 2, 250, display );

            const uint8_t insetBg = fheroes2::GetColorId( 0x24, 0x14, 0x0A );
            const uint8_t insetGold = fheroes2::GetColorId( 0xC0, 0x94, 0x34 );

            const fheroes2::Rect box( 16, 268, 288, 112 );
            fheroes2::Fill( display, box.x, box.y, box.width, box.height, insetBg );
            fheroes2::DrawRect( display, box, insetGold );

            const SpellSlot3DS & slot = levelSlots[selRow][selCol];
            activeDescStr.clear();
            descMaxScroll = 0;

            if ( slot.spell.isValid() ) {
                const fheroes2::Rect iconBox( 24, 275, 52, 52 );
                fheroes2::Fill( display, iconBox.x, iconBox.y, iconBox.width, iconBox.height, fheroes2::GetColorId( 0x34, 0x20, 0x12 ) );
                fheroes2::DrawRect( display, iconBox, insetGold );

                const fheroes2::Sprite & spSpr = Assets::getImage( ICN::SPELLS, slot.spell.IndexSprite() );
                if ( !spSpr.empty() && spSpr.width() > 0 && spSpr.height() > 0 ) {
                    const int32_t maxD = 46;
                    if ( spSpr.width() <= maxD && spSpr.height() <= maxD ) {
                        fheroes2::Blit( spSpr, display, iconBox.x + ( iconBox.width - spSpr.width() ) / 2,
                                        iconBox.y + ( iconBox.height - spSpr.height() ) / 2 );
                    }
                    else {
                        const double scale = std::min( static_cast<double>( maxD ) / spSpr.width(), static_cast<double>( maxD ) / spSpr.height() );
                        const int32_t dw = std::max<int32_t>( 1, static_cast<int32_t>( spSpr.width() * scale ) );
                        const int32_t dh = std::max<int32_t>( 1, static_cast<int32_t>( spSpr.height() * scale ) );
                        fheroes2::Image scaled( dw, dh );
                        scaled.reset();
                        fheroes2::Resize( spSpr, scaled );
                        fheroes2::Blit( scaled, display, iconBox.x + ( iconBox.width - dw ) / 2, iconBox.y + ( iconBox.height - dh ) / 2 );
                    }
                }

                fheroes2::Text spTitle( slot.spell.GetName(), fheroes2::FontType::normalYellow() );
                if ( spTitle.width() > 212 ) {
                    spTitle.fitToOneRow( 212 );
                }
                spTitle.draw( 84, 276, display );

                std::string meta = std::string( _( "Level" ) ) + " " + std::to_string( slot.spell.Level() ) + "   SP: "
                                   + std::to_string( slot.spell.spellPoints( hero ) );
                const fheroes2::Text metaTxt( std::move( meta ), fheroes2::FontType::smallYellow() );
                metaTxt.draw( 84, 296, display );

                const std::string kind = slot.spell.isCombat() ? _( "Combat Spell" ) : _( "Adventure Spell" );
                const fheroes2::Text kindTxt( kind, fheroes2::FontType::smallWhite() );
                kindTxt.draw( 84, 312, display );

                activeDescStr = slot.spell.GetDescription();
            }
            else {
                std::string lockedMsg = _( "Requires Mage Guild, Level %{level}." );
                StringReplace( lockedMsg, "%{level}", slot.level );
                activeDescStr = std::move( lockedMsg );
            }

            descBoxBg = fheroes2::Crop( display, descBoxRect.x, descBoxRect.y, descBoxRect.width, descBoxRect.height );
            const fheroes2::Text descTxt( activeDescStr, fheroes2::FontType::smallWhite() );
            descMaxScroll = std::max<int32_t>( 0, descTxt.height( descBoxRect.width ) - descBoxRect.height );
            descLastOffset = -1;
            descTimer.reset();
            drawScrollingDesc( true );

            // Hero status strip
            const fheroes2::Rect stBox( 16, 384, 288, 32 );
            fheroes2::Fill( display, stBox.x, stBox.y, stBox.width, stBox.height, insetBg );
            fheroes2::DrawRect( display, stBox, insetGold );
            const fheroes2::Text stTxt( defaultStatusBarText, fheroes2::FontType::smallWhite() );
            stTxt.draw( stBox.x + 8, stBox.y + 4, stBox.width - 16, display );

            btnExit.draw();
        };

        renderTopGuild();
        renderBottomGuild();
        display.render();

        LocalEvent & le = LocalEvent::Get();
        while ( le.HandleEvents() ) {
            if ( Game::HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_CANCEL ) || le.isKeyPressed( fheroes2::Key::KEY_ESCAPE ) ) {
                break;
            }

            if ( canSwitchCastles ) {
                if ( le.isKeyPressed( fheroes2::Key::KEY_PAGE_UP ) ) {
                    return MageGuildDialogResult::PrevMageGuildWindow;
                }
                if ( le.isKeyPressed( fheroes2::Key::KEY_PAGE_DOWN ) ) {
                    return MageGuildDialogResult::NextMageGuildWindow;
                }
            }

            bool selectionChanged = false;
            if ( le.isKeyPressed( fheroes2::Key::KEY_RIGHT ) || Game::HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_RIGHT ) ) {
                const size_t n = levelSlots[selRow].size();
                if ( n > 0 ) {
                    selCol = ( selCol + 1 ) % n;
                    selectionChanged = true;
                }
            }
            else if ( le.isKeyPressed( fheroes2::Key::KEY_LEFT ) || Game::HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_LEFT ) ) {
                const size_t n = levelSlots[selRow].size();
                if ( n > 0 ) {
                    selCol = ( selCol + n - 1 ) % n;
                    selectionChanged = true;
                }
            }
            else if ( le.isKeyPressed( fheroes2::Key::KEY_DOWN ) || Game::HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_DOWN ) ) {
                selRow = ( selRow + 1 ) % 5;
                if ( selCol >= levelSlots[selRow].size() ) {
                    selCol = levelSlots[selRow].empty() ? 0 : levelSlots[selRow].size() - 1;
                }
                selectionChanged = true;
            }
            else if ( le.isKeyPressed( fheroes2::Key::KEY_UP ) || Game::HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_UP ) ) {
                selRow = ( selRow + 4 ) % 5;
                if ( selCol >= levelSlots[selRow].size() ) {
                    selCol = levelSlots[selRow].empty() ? 0 : levelSlots[selRow].size() - 1;
                }
                selectionChanged = true;
            }

            btnExit.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( btnExit.area() ) );
            if ( le.MouseClickLeft( btnExit.area() ) ) {
                break;
            }

            if ( selectionChanged ) {
                renderTopGuild();
                renderBottomGuild();
                display.render();
            }
            else if ( drawScrollingDesc( false ) ) {
                display.render( descBoxRect );
            }
        }
        return MageGuildDialogResult::DoNothing;
    }
#endif
    fheroes2::Display & display = fheroes2::Display::instance();

    // setup cursor
    const CursorRestorer cursorRestorer( true, Cursor::POINTER );

    const fheroes2::ImageRestorer restorer( display, ( display.width() - fheroes2::Display::DEFAULT_WIDTH ) / 2,
                                            ( display.height() - fheroes2::Display::DEFAULT_HEIGHT ) / 2, fheroes2::Display::DEFAULT_WIDTH,
                                            fheroes2::Display::DEFAULT_HEIGHT );

    const fheroes2::Point dialogOffset{ restorer.x(), restorer.y() };
    const fheroes2::Point bottomUIOffset{ dialogOffset.x, dialogOffset.y + bottomBarOffsetY };

    const bool isEvilInterface = Settings::Get().isEvilInterfaceEnabled();

    fheroes2::Blit( Assets::getImage( isEvilInterface ? ICN::STONEBAK_EVIL : ICN::STONEBAK, 0 ), display, dialogOffset.x, dialogOffset.y );

    // Create Previous Castle button.
    fheroes2::Button buttonPrevCastle( bottomUIOffset.x, bottomUIOffset.y, ICN::SMALLBAR, 1, 2 );
    fheroes2::TimedEventValidator timedButtonPrevCastle( [&buttonPrevCastle]() { return buttonPrevCastle.isPressed(); } );
    buttonPrevCastle.subscribe( &timedButtonPrevCastle );

    // Exit button.
    const int32_t exitWidth = Assets::getImage( ICN::BUTTON_GUILDWELL_EXIT, 0 ).width();
    fheroes2::Button buttonExit( bottomUIOffset.x + fheroes2::Display::DEFAULT_WIDTH - exitWidth, bottomUIOffset.y, ICN::BUTTON_GUILDWELL_EXIT, 0, 1 );

    // Create Next Castle button.
    fheroes2::Button buttonNextCastle( buttonExit.area().x - Assets::getImage( ICN::SMALLBAR, 3 ).width(), bottomUIOffset.y, ICN::SMALLBAR, 3, 4 );
    fheroes2::TimedEventValidator timedButtonNextCastle( [&buttonNextCastle]() { return buttonNextCastle.isPressed(); } );
    buttonNextCastle.subscribe( &timedButtonNextCastle );

    // Create the status bar UI element.
    const fheroes2::Sprite & bottomBar = Assets::getImage( ICN::SMALLBAR, 0 );
    const int32_t barHeight = bottomBar.height();
    const fheroes2::Point bottonBarOffset{ buttonPrevCastle.area().x + buttonPrevCastle.area().width, bottomUIOffset.y };
    const int32_t bottomBarWidth{ fheroes2::Display::DEFAULT_WIDTH - exitWidth - buttonNextCastle.area().width - buttonPrevCastle.area().width };

    // ICN::SMALLBAR image's first column contains all black pixels. This should not be drawn.
    fheroes2::Copy( bottomBar, 1, 0, display, bottonBarOffset.x, bottonBarOffset.y, bottomBarWidth / 2, barHeight );
    fheroes2::Copy( bottomBar, bottomBar.width() - ( bottomBarWidth - bottomBarWidth / 2 ), 0, display, bottonBarOffset.x + bottomBarWidth / 2, bottonBarOffset.y,
                    bottomBarWidth - bottomBarWidth / 2, barHeight );

    StatusBar statusBar;
    // Status bar must be smaller due to extra art on both sides.
    statusBar.setRoi( { bottomUIOffset.x + 16, bottomUIOffset.y, bottomBarWidth, 0 } );

    // Default text for status bar.
    std::string defaultStatusBarText;
    if ( hero == nullptr || !hero->HaveSpellBook() ) {
        defaultStatusBarText = _( "The above spells are available here." );
    }
    else {
        defaultStatusBarText = _( "The spells the hero can learn have been added to their book." );
    }

    const int guildLevel = GetLevelMageGuild();

    fheroes2::renderMageGuildBuilding( _race, guildLevel, dialogOffset );

    const bool haveLibraryCapability = HaveLibraryCapability();
    const bool hasLibrary = isLibraryBuilt();

    std::array<std::unique_ptr<fheroes2::SpellsInOneRow>, 5> spellRows;

    for ( size_t levelIndex = 0; levelIndex < spellRows.size(); ++levelIndex ) {
        const int32_t spellsLevel = static_cast<int32_t>( levelIndex ) + 1;
        const int32_t count = MageGuild::getMaxSpellsCount( spellsLevel, haveLibraryCapability );

        SpellStorage spells = GetMageGuild().GetSpells( guildLevel, hasLibrary, spellsLevel );
        spells.resize( count, Spell::NONE );

        spellRows[levelIndex] = std::make_unique<fheroes2::SpellsInOneRow>( std::move( spells ) );

        spellRows[levelIndex]->setPosition( { dialogOffset.x + 250, dialogOffset.y + 365 - 90 * static_cast<int32_t>( levelIndex ) } );
        spellRows[levelIndex]->redraw( display );
    }

    if ( GetKingdom().GetCastles().size() < 2 ) {
        buttonPrevCastle.disable();
        buttonNextCastle.disable();
    }

    buttonPrevCastle.draw();
    buttonNextCastle.draw();

    buttonExit.draw();

    LocalEvent & le = LocalEvent::Get();

    auto updateStatusBar = [&le, &buttonExit, &buttonPrevCastle, &buttonNextCastle, &statusBar, &defaultStatusBarText]() {
        std::string statusMessage;

        if ( le.isMouseCursorPosInArea( buttonExit.area() ) ) {
            statusMessage = _( "Exit Mage Guild" );
        }
        else if ( buttonPrevCastle.isEnabled() && le.isMouseCursorPosInArea( buttonPrevCastle.area() ) ) {
            statusMessage = _( "Show previous town" );
        }
        else if ( buttonNextCastle.isEnabled() && le.isMouseCursorPosInArea( buttonNextCastle.area() ) ) {
            statusMessage = _( "Show next town" );
        }

        if ( statusMessage.empty() ) {
            statusBar.ShowMessage( defaultStatusBarText );
        }
        else {
            statusBar.ShowMessage( statusMessage );
            statusMessage.clear();
        }
    };

    updateStatusBar();

    display.render( restorer.rect() );

    // message loop
    while ( le.HandleEvents() ) {
        if ( le.MouseClickLeft( buttonExit.area() ) || Game::HotKeyCloseWindow() ) {
            break;
        }

        buttonExit.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonExit.area() ) );

        if ( buttonPrevCastle.isEnabled() ) {
            buttonPrevCastle.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonPrevCastle.area() ) );

            if ( le.MouseClickLeft( buttonPrevCastle.area() ) || HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_LEFT ) || timedButtonPrevCastle.isDelayPassed() ) {
                return MageGuildDialogResult::PrevMageGuildWindow;
            }
        }
        if ( buttonNextCastle.isEnabled() ) {
            buttonNextCastle.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonNextCastle.area() ) );

            if ( le.MouseClickLeft( buttonNextCastle.area() ) || HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_RIGHT ) || timedButtonNextCastle.isDelayPassed() ) {
                return MageGuildDialogResult::NextMageGuildWindow;
            }
        }

        spellRows[0]->queueEventProcessing( false ) || spellRows[1]->queueEventProcessing( false ) || spellRows[2]->queueEventProcessing( false )
            || spellRows[3]->queueEventProcessing( false ) || spellRows[4]->queueEventProcessing( false );

        if ( le.isMouseRightButtonPressedInArea( buttonExit.area() ) ) {
            fheroes2::showStandardTextMessage( _( "Exit" ), _( "Exit Mage Guild" ), Dialog::ZERO );
        }
        else if ( le.isMouseRightButtonPressedInArea( buttonNextCastle.area() ) ) {
            fheroes2::showStandardTextMessage( _( "Show next town" ), _( "Click to show the next town." ), Dialog::ZERO );
        }
        else if ( le.isMouseRightButtonPressedInArea( buttonPrevCastle.area() ) ) {
            fheroes2::showStandardTextMessage( _( "Show previous town" ), _( "Click to show the previous town." ), Dialog::ZERO );
        }

        updateStatusBar();
    }

    return MageGuildDialogResult::DoNothing;
}
