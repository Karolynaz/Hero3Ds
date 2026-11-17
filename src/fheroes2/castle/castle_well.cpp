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
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "army.h"
#include "army_troop.h"
#include "battle_cell.h"
#include "castle.h" // IWYU pragma: associated
#include "cursor.h"
#include "dialog.h"
#include "game_assets.h"
#include "game_delays.h"
#include "game_hotkeys.h"
#include "heroes.h"
#include "icn.h"
#include "image.h"
#include "kingdom.h"
#include "localevent.h"
#include "math_base.h"
#include "monster.h"
#include "monster_anim.h"
#include "resource.h"
#include "screen.h"
#include "settings.h"
#include "speed.h"
#include "translations.h"
#include "ui_3ds.h"
#include "ui_button.h"
#include "ui_castle.h"
#include "ui_dialog.h"
#include "ui_kingdom.h"
#include "ui_text.h"

namespace
{
    const int32_t bottomBarOffsetY = 461;

    const std::array<uint32_t, Castle::maxNumOfDwellings> castleDwellings
        = { DWELLING_MONSTER1, DWELLING_MONSTER2, DWELLING_MONSTER3, DWELLING_MONSTER4, DWELLING_MONSTER5, DWELLING_MONSTER6 };

    uint32_t howManyRecruitMonster( const Castle & castle, Troops & tempCastleArmy, Troops & tempHeroArmy, const uint32_t dw, const Funds & add, Funds & res )
    {
        const Monster monsters( castle.GetRace(), castle.GetActualDwelling( dw ) );
        if ( !tempCastleArmy.CanJoinTroop( monsters ) && !tempHeroArmy.CanJoinTroop( monsters ) ) {
            return 0;
        }

        uint32_t count = castle.getMonstersInDwelling( dw );
        Funds payment;

        const Kingdom & kingdom = castle.GetKingdom();

        while ( count ) {
            payment = monsters.GetCost() * count;
            res = payment;
            payment += add;
            if ( kingdom.AllowPayment( payment ) ) {
                break;
            }
            --count;
        }

        if ( count > 0 ) {
            if ( tempCastleArmy.CanJoinTroop( monsters ) ) {
                tempCastleArmy.JoinTroop( monsters, count, false );
            }
            else if ( tempHeroArmy.CanJoinTroop( monsters ) ) {
                tempHeroArmy.JoinTroop( monsters, count, false );
            }
        }

        return count;
    }

    BuildingType getPressedBuildingHotkey()
    {
        if ( HotKeyPressEvent( Game::HotKeyEvent::TOWN_DWELLING_LEVEL_1 ) ) {
            return DWELLING_MONSTER1;
        }
        if ( HotKeyPressEvent( Game::HotKeyEvent::TOWN_DWELLING_LEVEL_2 ) ) {
            return DWELLING_MONSTER2;
        }
        if ( HotKeyPressEvent( Game::HotKeyEvent::TOWN_DWELLING_LEVEL_3 ) ) {
            return DWELLING_MONSTER3;
        }
        if ( HotKeyPressEvent( Game::HotKeyEvent::TOWN_DWELLING_LEVEL_4 ) ) {
            return DWELLING_MONSTER4;
        }
        if ( HotKeyPressEvent( Game::HotKeyEvent::TOWN_DWELLING_LEVEL_5 ) ) {
            return DWELLING_MONSTER5;
        }
        if ( HotKeyPressEvent( Game::HotKeyEvent::TOWN_DWELLING_LEVEL_6 ) ) {
            return DWELLING_MONSTER6;
        }

        return BUILD_NOTHING;
    }
}

bool Castle::_recruitCastleMax( const Troops & currentCastleArmy )
{
    std::vector<Troop> totalRecruitmentResult;
    Funds currentMonsterCost;
    Funds totalMonstersCost;
    std::string monstersRecruitedText;

    Troops tempCastleArmy( currentCastleArmy );
    Troops tempGuestArmy;

    const Heroes * hero = GetHero();

    if ( hero ) {
        tempGuestArmy.Insert( hero->GetArmy().getTroops() );
    }

    // In this loop we should go in reverse order - from strongest monsters to weakest in order to purchase most of the best monsters.
    for ( size_t id = 0; id < maxNumOfDwellings; ++id ) {
        const uint32_t dwellingType = castleDwellings[maxNumOfDwellings - id - 1];
        const uint32_t recruitableNumber = howManyRecruitMonster( *this, tempCastleArmy, tempGuestArmy, dwellingType, totalMonstersCost, currentMonsterCost );

        if ( recruitableNumber == 0 ) {
            continue;
        }
        const Monster recruitableMonster( _race, GetActualDwelling( dwellingType ) );

        totalRecruitmentResult.emplace_back( recruitableMonster, recruitableNumber );
        totalMonstersCost += currentMonsterCost;

        monstersRecruitedText += recruitableMonster.GetPluralName( recruitableNumber );
        monstersRecruitedText += " : ";
        monstersRecruitedText += std::to_string( recruitableNumber );
        monstersRecruitedText += '\n';
    }

    const fheroes2::FontType normalWhite = fheroes2::FontType::normalWhite();

    if ( monstersRecruitedText.empty() ) {
        bool isCreaturePresent = false;
        bool canAffordOneCreature = false;

        for ( const uint32_t dwellingType : castleDwellings ) {
            if ( getMonstersInDwelling( dwellingType ) > 0 ) {
                const Monster monsters( _race, dwellingType );
                const Funds payment = monsters.GetCost();

                if ( GetKingdom().AllowPayment( payment ) ) {
                    canAffordOneCreature = true;
                }
                isCreaturePresent = true;
                break;
            }
        }
        if ( isCreaturePresent ) {
            if ( !canAffordOneCreature ) {
                fheroes2::showStandardTextMessage( {}, _( "Not enough resources to recruit creatures." ), Dialog::OK );
            }
            else {
                fheroes2::showStandardTextMessage( {}, _( "You are unable to recruit at this time, your ranks are full." ), Dialog::OK );
            }
        }
        else {
            fheroes2::showStandardTextMessage( {}, _( "No creatures available for purchase." ), Dialog::OK );
        }
    }
    else if ( fheroes2::showResourceMessage( fheroes2::Text( _( "Recruit Creatures" ), fheroes2::FontType::normalYellow() ),
                                             fheroes2::Text( std::move( monstersRecruitedText ), normalWhite ), Dialog::YES | Dialog::NO, totalMonstersCost )
              == Dialog::YES ) {
        for ( const Troop & troop : totalRecruitmentResult ) {
            RecruitMonster( troop, false );
        }

        return true;
    }

    return false;
}

void Castle::_openWell()
{
#if defined( TARGET_NINTENDO_3DS )
    {
        const bool isEvil = Settings::Get().isEvilInterfaceEnabled();
        fheroes2::Display & display = fheroes2::Display::instance();
        const fheroes2::ImageRestorer screenRestorer( display, 0, 0, display.width(), display.height() );
        const fheroes2::Scope3DSCastleBuildView wellViewScope;

        const std::array<Monster, maxNumOfDwellings> castleMonster
            = { Monster( _race, GetActualDwelling( castleDwellings[0] ) ), Monster( _race, GetActualDwelling( castleDwellings[1] ) ),
                Monster( _race, GetActualDwelling( castleDwellings[2] ) ), Monster( _race, GetActualDwelling( castleDwellings[3] ) ),
                Monster( _race, GetActualDwelling( castleDwellings[4] ) ), Monster( _race, GetActualDwelling( castleDwellings[5] ) ) };

        std::array<fheroes2::RandomMonsterAnimation, maxNumOfDwellings> monsterAnimInfo
            = { fheroes2::RandomMonsterAnimation( castleMonster[0], false ), fheroes2::RandomMonsterAnimation( castleMonster[1], false ),
                fheroes2::RandomMonsterAnimation( castleMonster[2], false ), fheroes2::RandomMonsterAnimation( castleMonster[3], false ),
                fheroes2::RandomMonsterAnimation( castleMonster[4], false ), fheroes2::RandomMonsterAnimation( castleMonster[5], false ) };

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

        size_t selectedIdx = 0;
        for ( size_t i = 0; i < maxNumOfDwellings; ++i ) {
            if ( ( _constructedBuildings & castleDwellings[i] ) && _dwelling[i] > 0 ) {
                selectedIdx = i;
                break;
            }
        }

        const auto cardRectFor = []( const size_t idx ) -> fheroes2::Rect {
            const int32_t col = static_cast<int32_t>( idx % 3 );
            const int32_t row = static_cast<int32_t>( idx / 3 );
            return { 7 + col * 131, 8 + row * 116, 124, 108 };
        };

        auto dwellingGrowth = [&]( const size_t idx ) -> uint32_t {
            uint32_t grown = castleMonster[idx].GetGrown();
            grown += ( _constructedBuildings & BUILD_WELL ) ? GetGrownWell() : 0;
            if ( idx == 0 && ( _constructedBuildings & BUILD_WEL2 ) ) {
                grown += GetGrownWel2();
            }
            return grown;
        };

        auto renderTopWell = [&]( const bool advanceAnim ) {
            fheroes2::Copy( topBg, 0, 0, display, 0, 0, 400, 240 );
            const uint8_t cardBg = fheroes2::GetColorId( 0x26, 0x16, 0x0C );
            const uint8_t unbuiltBg = fheroes2::GetColorId( 0x1C, 0x10, 0x0A );
            const uint8_t brightGold = fheroes2::GetColorId( 0xFF, 0xEA, 0x40 );
            const uint8_t warmGold = fheroes2::GetColorId( 0xC0, 0x94, 0x34 );
            const uint8_t darkFrame = fheroes2::GetColorId( 0x18, 0x0E, 0x06 );

            for ( size_t i = 0; i < maxNumOfDwellings; ++i ) {
                const fheroes2::Rect card = cardRectFor( i );
                const bool built = ( _constructedBuildings & castleDwellings[i] ) != 0;
                const bool sel = ( i == selectedIdx );

                fheroes2::Fill( display, card.x, card.y, card.width, card.height, built ? cardBg : unbuiltBg );

                // Top header strip
                fheroes2::Fill( display, card.x + 2, card.y + 2, card.width - 4, 15, fheroes2::GetColorId( 0x34, 0x1E, 0x10 ) );
                fheroes2::Text title( castleMonster[i].GetMultiName(), built ? fheroes2::FontType::smallYellow() : fheroes2::FontType::smallWhite() );
                if ( title.width() > card.width - 8 ) {
                    title.fitToOneRow( card.width - 8 );
                }
                title.draw( card.x + ( card.width - title.width() ) / 2, card.y + 4, display );

                // Creature animated sprite centered
                const fheroes2::Sprite & monSpr = Assets::getImage( monsterAnimInfo[i].icnFile(), monsterAnimInfo[i].frameId() );
                fheroes2::drawFittedSprite3DS( monSpr, display, { card.x + 6, card.y + 19, card.width - 12, 66 } );
                if ( advanceAnim ) {
                    monsterAnimInfo[i].increment();
                }

                // Bottom info bar
                const int32_t barY = card.y + card.height - 20;
                if ( built ) {
                    fheroes2::Fill( display, card.x + 2, barY, card.width - 4, 18, fheroes2::GetColorId( 0x1A, 0x2E, 0x14 ) );
                    const fheroes2::Text availTxt( _( "Avail:" ) + std::string( " " ) + std::to_string( _dwelling[i] ), fheroes2::FontType::smallWhite() );
                    availTxt.draw( card.x + 6, barY + 4, display );
                    const fheroes2::Text grownTxt( "+" + std::to_string( dwellingGrowth( i ) ) + "/wk", fheroes2::FontType::smallYellow() );
                    grownTxt.draw( card.x + card.width - 6 - grownTxt.width(), barY + 4, display );
                }
                else {
                    fheroes2::Fill( display, card.x + 2, barY, card.width - 4, 18, fheroes2::GetColorId( 0x48, 0x16, 0x14 ) );
                    const fheroes2::Text nbTxt( _( "Not Built" ), fheroes2::FontType::smallWhite() );
                    nbTxt.draw( card.x + ( card.width - nbTxt.width() ) / 2, barY + 4, display );
                }

                if ( sel ) {
                    fheroes2::DrawRect( display, card, brightGold );
                    fheroes2::DrawRect( display, { card.x + 1, card.y + 1, card.width - 2, card.height - 2 }, warmGold );
                }
                else {
                    fheroes2::DrawRect( display, card, darkFrame );
                }
            }
        };

        fheroes2::ButtonSprite btnRecruit = fheroes2::make3DSButton( _( "Recruit" ), { 24, 386, 126, 32 } );
        fheroes2::ButtonSprite btnBuyAll = fheroes2::make3DSButton( _( "Buy All" ), { 170, 386, 126, 32 } );
        fheroes2::Rect footerExitLocalRect;
        fheroes2::Rect exitDisplayRect;
        fheroes2::Rect resDisplayRect;
        bool exitPressed = false;

        auto renderBottomFooter = [&]() {
            const fheroes2::Image footerImg = fheroes2::render3DSCastleResourceFooter( GetKingdom().GetFunds(), exitPressed, footerExitLocalRect );
            fheroes2::Copy( footerImg, 0, 0, display, 0, 432, footerImg.width(), footerImg.height() );
            exitDisplayRect = { footerExitLocalRect.x, 432 + footerExitLocalRect.y, footerExitLocalRect.width, footerExitLocalRect.height };
            resDisplayRect = { 0, 432, footerExitLocalRect.x, footerImg.height() };
        };

        auto renderBottomWell = [&]() {
            fheroes2::draw3DSPanel( { 0, 240, 320, 240 } );
            fheroes2::Text hdr( GetName() + " - " + _( "Well" ), fheroes2::FontType::normalYellow() );
            if ( hdr.width() > 280 ) {
                hdr.fitToOneRow( 280 );
            }
            hdr.draw( ( 320 - hdr.width() ) / 2, 250, display );

            const Monster & mon = castleMonster[selectedIdx];
            const bool built = ( _constructedBuildings & castleDwellings[selectedIdx] ) != 0;

            const fheroes2::Rect box( 16, 269, 288, 108 );
            fheroes2::Fill( display, box.x, box.y, box.width, box.height, fheroes2::GetColorId( 0x24, 0x14, 0x0A ) );
            fheroes2::DrawRect( display, box, fheroes2::GetColorId( 0xC0, 0x94, 0x34 ) );

            const fheroes2::Text monTitle( mon.GetMultiName(), fheroes2::FontType::normalYellow() );
            monTitle.draw( box.x + ( box.width - monTitle.width() ) / 2, box.y + 6, display );

            std::string dmg = std::to_string( mon.GetDamageMin() );
            if ( mon.GetDamageMin() != mon.GetDamageMax() ) {
                dmg += '-' + std::to_string( mon.GetDamageMax() );
            }

            const std::array<std::pair<std::string, std::string>, 6> leftStats = { {
                { _( "Attack" ), std::to_string( mon.GetAttack() ) },
                { _( "Defense" ), std::to_string( mon.GetDefense() ) },
                { _( "Damage" ), dmg },
                { _( "HP" ), std::to_string( mon.GetHitPoints() ) },
                { _( "Speed" ), Speed::String( mon.GetSpeed() ) },
                { _( "Cost" ), std::to_string( mon.GetCost().gold ) + 'g' },
            } };

            int32_t y = box.y + 26;
            for ( size_t r = 0; r < 3; ++r ) {
                const fheroes2::Text l1( leftStats[r * 2].first + ": " + leftStats[r * 2].second, fheroes2::FontType::smallWhite() );
                const fheroes2::Text l2( leftStats[r * 2 + 1].first + ": " + leftStats[r * 2 + 1].second, fheroes2::FontType::smallWhite() );
                l1.draw( box.x + 12, y, display );
                l2.draw( box.x + 156, y, display );
                y += 16;
            }

            const std::string statusLine
                = built ? ( std::string( _( "Available:" ) ) + " " + std::to_string( _dwelling[selectedIdx] ) + "   (" + _( "Growth" ) + ": +"
                            + std::to_string( dwellingGrowth( selectedIdx ) ) + "/" + _( "week" ) + ")" )
                        : _( "This creature dwelling has not been built yet." );
            const fheroes2::Text stTxt( statusLine, built ? fheroes2::FontType::smallYellow() : fheroes2::FontType::smallWhite() );
            stTxt.draw( box.x + ( box.width - stTxt.width() ) / 2, box.y + 84, display );

            if ( built ) {
                btnRecruit.enable();
            }
            else {
                btnRecruit.disable();
            }
            btnRecruit.draw();
            btnBuyAll.draw();
            renderBottomFooter();
        };

        renderTopWell( false );
        renderBottomWell();
        display.render();

        LocalEvent & le = LocalEvent::Get();
        Game::passAnimationDelay( Game::DelayType::CASTLE_UNIT_DELAY );

        while ( le.HandleEvents( Game::isDelayNeeded( { Game::DelayType::CASTLE_UNIT_DELAY } ) ) ) {
            if ( Game::HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_CANCEL ) || le.isKeyPressed( fheroes2::Key::KEY_ESCAPE ) ) {
                break;
            }

            bool selectionChanged = false;
            bool needFooterRedraw = false;

            if ( le.isKeyPressed( fheroes2::Key::KEY_RIGHT ) || Game::HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_RIGHT ) ) {
                selectedIdx = ( selectedIdx + 1 ) % maxNumOfDwellings;
                selectionChanged = true;
            }
            else if ( le.isKeyPressed( fheroes2::Key::KEY_LEFT ) || Game::HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_LEFT ) ) {
                selectedIdx = ( selectedIdx + maxNumOfDwellings - 1 ) % maxNumOfDwellings;
                selectionChanged = true;
            }
            else if ( le.isKeyPressed( fheroes2::Key::KEY_DOWN ) || Game::HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_DOWN )
                      || le.isKeyPressed( fheroes2::Key::KEY_UP ) || Game::HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_UP ) ) {
                selectedIdx = ( selectedIdx + 3 ) % maxNumOfDwellings;
                selectionChanged = true;
            }

            const bool built = ( _constructedBuildings & castleDwellings[selectedIdx] ) != 0;
            if ( built ) {
                btnRecruit.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( btnRecruit.area() ) );
            }
            btnBuyAll.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( btnBuyAll.area() ) );

            const BuildingType pressedHotkey = getPressedBuildingHotkey();
            if ( ( built && ( le.MouseClickLeft( btnRecruit.area() ) || le.isKeyPressed( fheroes2::Key::KEY_ENTER )
                              || Game::HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_OKAY ) ) )
                 || ( pressedHotkey != BUILD_NOTHING && ( _constructedBuildings & pressedHotkey ) ) ) {
                size_t dwIdx = selectedIdx;
                if ( pressedHotkey != BUILD_NOTHING ) {
                    for ( size_t i = 0; i < maxNumOfDwellings; ++i ) {
                        if ( castleDwellings[i] == pressedHotkey ) {
                            dwIdx = i;
                            break;
                        }
                    }
                }
                RecruitMonster( Dialog::RecruitMonster( castleMonster[dwIdx], _dwelling[dwIdx], true, 2 ) );
                le.reset();
                renderTopWell( false );
                renderBottomWell();
                display.render();
                continue;
            }

            if ( le.MouseClickLeft( btnBuyAll.area() ) || HotKeyPressEvent( Game::HotKeyEvent::TOWN_WELL_BUY_ALL ) ) {
                _recruitCastleMax( GetArmy() );
                renderTopWell( false );
                renderBottomWell();
                display.render();
                continue;
            }

            if ( le.MouseClickLeft( resDisplayRect ) ) {
                {
                    const fheroes2::Scope3DSStandardLayout incomeScope;
                    const CursorRestorer incomeCursor( true, Cursor::POINTER );
                    fheroes2::showKingdomIncome( GetKingdom(), Dialog::OK );
                    le.reset();
                }
                renderTopWell( false );
                renderBottomWell();
                display.render();
                continue;
            }

            const bool exitHeld = le.isMouseLeftButtonPressedAndHeldInArea( exitDisplayRect );
            if ( exitHeld != exitPressed ) {
                exitPressed = exitHeld;
                needFooterRedraw = true;
            }
            if ( le.MouseClickLeft( exitDisplayRect ) ) {
                break;
            }

            if ( selectionChanged ) {
                renderTopWell( false );
                renderBottomWell();
                display.render();
            }
            else if ( Game::validateAnimationDelay( Game::DelayType::CASTLE_UNIT_DELAY ) ) {
                renderTopWell( true );
                if ( needFooterRedraw ) {
                    renderBottomFooter();
                }
                display.render();
            }
            else if ( needFooterRedraw ) {
                renderBottomFooter();
                display.render( { 0, 432, 320, 48 } );
            }
        }
        return;
    }
#endif
    fheroes2::Display & display = fheroes2::Display::instance();

    // setup cursor
    const CursorRestorer cursorRestorer( true, Cursor::POINTER );

    const fheroes2::ImageRestorer restorer( display, ( display.width() - fheroes2::Display::DEFAULT_WIDTH ) / 2,
                                            ( display.height() - fheroes2::Display::DEFAULT_HEIGHT ) / 2, fheroes2::Display::DEFAULT_WIDTH,
                                            fheroes2::Display::DEFAULT_HEIGHT );

    const fheroes2::Rect roi = restorer.rect();

    const int32_t buttonOffsetY = roi.y + bottomBarOffsetY;

    // MAX button.
    fheroes2::Button buttonMax( roi.x, buttonOffsetY, ICN::BUTTON_WELL_MAX, 0, 1 );

    // EXIT button.
    fheroes2::Button buttonExit( roi.x + roi.width - Assets::getImage( ICN::BUTTON_GUILDWELL_EXIT, 0 ).width(), buttonOffsetY, ICN::BUTTON_GUILDWELL_EXIT, 0, 1 );

    const std::array<fheroes2::Rect, maxNumOfDwellings> rectMonster
        = { fheroes2::Rect( roi.x + 20, roi.y + 18, 288, 124 ),   fheroes2::Rect( roi.x + 20, roi.y + 168, 288, 124 ),
            fheroes2::Rect( roi.x + 20, roi.y + 318, 288, 124 ),  fheroes2::Rect( roi.x + 334, roi.y + 18, 288, 124 ),
            fheroes2::Rect( roi.x + 334, roi.y + 168, 288, 124 ), fheroes2::Rect( roi.x + 334, roi.y + 318, 288, 124 ) };

    const std::array<Monster, maxNumOfDwellings> castleMonster
        = { Monster( _race, GetActualDwelling( castleDwellings[0] ) ), Monster( _race, GetActualDwelling( castleDwellings[1] ) ),
            Monster( _race, GetActualDwelling( castleDwellings[2] ) ), Monster( _race, GetActualDwelling( castleDwellings[3] ) ),
            Monster( _race, GetActualDwelling( castleDwellings[4] ) ), Monster( _race, GetActualDwelling( castleDwellings[5] ) ) };

    std::array<fheroes2::RandomMonsterAnimation, maxNumOfDwellings> monsterAnimInfo
        = { fheroes2::RandomMonsterAnimation( castleMonster[0], false ), fheroes2::RandomMonsterAnimation( castleMonster[1], false ),
            fheroes2::RandomMonsterAnimation( castleMonster[2], false ), fheroes2::RandomMonsterAnimation( castleMonster[3], false ),
            fheroes2::RandomMonsterAnimation( castleMonster[4], false ), fheroes2::RandomMonsterAnimation( castleMonster[5], false ) };

    fheroes2::Image background;
    background._disableTransformLayer();
    background.resize( roi.width, roi.height );
    _wellRedrawBackground( background );

    LocalEvent & le = LocalEvent::Get();

    Game::passAnimationDelay( Game::DelayType::CASTLE_UNIT_DELAY );

    while ( le.HandleEvents( Game::isDelayNeeded( { Game::DelayType::CASTLE_UNIT_DELAY } ) ) ) {
        buttonExit.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonExit.area() ) );

        buttonMax.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonMax.area() ) );
        const BuildingType pressedHotkeyBuildingID = getPressedBuildingHotkey();

        if ( le.MouseClickLeft( buttonExit.area() ) || Game::HotKeyCloseWindow() ) {
            break;
        }
        if ( le.MouseClickLeft( buttonMax.area() ) || HotKeyPressEvent( Game::HotKeyEvent::TOWN_WELL_BUY_ALL ) ) {
            const Troops & currentArmy = GetArmy();
            if ( _recruitCastleMax( currentArmy ) ) {
                // Update available monster count on background.

                for ( const uint32_t dwellingType : castleDwellings ) {
                    _wellRedrawAvailableMonsters( dwellingType, true, background );
                }
            }
        }

        for ( size_t dwellingId = 0; dwellingId < maxNumOfDwellings; ++dwellingId ) {
            if ( _constructedBuildings & castleDwellings[dwellingId] ) {
                if ( le.MouseClickLeft( rectMonster[dwellingId] ) || pressedHotkeyBuildingID == castleDwellings[dwellingId] ) {
                    if ( RecruitMonster( Dialog::RecruitMonster( castleMonster[dwellingId], _dwelling[dwellingId], true, 2 ) ) ) {
                        // Update available monster count on background.

                        _wellRedrawAvailableMonsters( castleDwellings[dwellingId], true, background );
                    }

                    break;
                }

                if ( le.isMouseRightButtonPressedInArea( rectMonster[dwellingId] ) ) {
                    Dialog::DwellingInfo( castleMonster[dwellingId], _dwelling[dwellingId] );

                    break;
                }
            }
        }

        if ( Game::validateAnimationDelay( Game::DelayType::CASTLE_UNIT_DELAY ) ) {
            fheroes2::Copy( background, 0, 0, display, roi.x, roi.y, roi.width, roi.height );

            _wellRedrawMonsterAnimation( roi, monsterAnimInfo );

            buttonMax.draw();
            buttonExit.draw();
            display.render( roi );
        }

        if ( le.isMouseRightButtonPressedInArea( buttonExit.area() ) ) {
            fheroes2::showStandardTextMessage( _( "Exit" ), _( "Exit this menu." ), Dialog::ZERO );
        }

        if ( le.isMouseRightButtonPressedInArea( buttonMax.area() ) ) {
            fheroes2::showStandardTextMessage( _( "Max" ), _( "Hire all creatures in the town." ), Dialog::ZERO );
        }
    }
}

void Castle::_wellRedrawAvailableMonsters( const uint32_t dwellingType, const bool restoreBackground, fheroes2::Image & background ) const
{
    if ( !( _constructedBuildings & dwellingType ) ) {
        // This building has not been built.
        return;
    }

    uint32_t population = 0;
    fheroes2::Point offset{ 20, 121 };

    switch ( dwellingType ) {
    case DWELLING_MONSTER1:
        population = _dwelling[0];
        break;
    case DWELLING_MONSTER2:
        population = _dwelling[1];
        offset.y += 150;
        break;
    case DWELLING_MONSTER3:
        population = _dwelling[2];
        offset.y += 300;
        break;
    case DWELLING_MONSTER4:
        offset.x += 314;
        population = _dwelling[3];
        break;
    case DWELLING_MONSTER5:
        population = _dwelling[4];
        offset.x += 314;
        offset.y += 150;
        break;
    case DWELLING_MONSTER6:
        population = _dwelling[5];
        offset.x += 314;
        offset.y += 300;
        break;
    default:
        // Have you added a new dwelling?
        assert( 0 );
        break;
    }

    if ( restoreBackground ) {
        // Restore background under "Available: <number>" text.
        const fheroes2::Sprite & wellBackground = Assets::getImage( Settings::Get().isEvilInterfaceEnabled() ? ICN::WELLBKG_EVIL : ICN::WELLBKG, 0 );

        fheroes2::Copy( wellBackground, offset.x, offset.y, background, offset.x, offset.y, 137, fheroes2::getFontHeight( fheroes2::FontSize::NORMAL ) );
    }

    std::string textString = _( "Available" );
    textString += ':';

    fheroes2::Text text( std::to_string( population ), fheroes2::FontType::normalYellow() );
    const int32_t populationOffsetX{ 109 - text.width() / 2 };
    text.draw( offset.x + populationOffsetX, offset.y, background );

    const int32_t textLength{ 106 };
    const int32_t allowedTextLength{ std::min( populationOffsetX, textLength ) };

    text.set( std::move( textString ), fheroes2::FontType::smallWhite() );
    if ( text.width() > allowedTextLength ) {
        text.fitToOneRow( allowedTextLength );
        text.draw( offset.x, offset.y + 2, background );
    }
    else {
        text.draw( offset.x + ( textLength - text.width() ) / 2, offset.y + 2, background );
    }
}

void Castle::_wellRedrawBackground( fheroes2::Image & background ) const
{
    const fheroes2::Sprite & wellBackground = Assets::getImage( Settings::Get().isEvilInterfaceEnabled() ? ICN::WELLBKG_EVIL : ICN::WELLBKG, 0 );
    const int32_t backgroundWidth = wellBackground.width();

    fheroes2::Copy( wellBackground, 0, 0, background, 0, 0, backgroundWidth, bottomBarOffsetY );

    // TODO: Make Well bottom bar and buttons for Evil interface.

    // The original ICN::WELLBKG image has incorrect bottom message bar with no yellow outline. Also the original graphics did not have MAX button.
    const fheroes2::Sprite & bottomBar = Assets::getImage( ICN::SMALLBAR, 0 );
    const int32_t barHeight = bottomBar.height();
    const int32_t exitWidth = Assets::getImage( ICN::BUTTON_GUILDWELL_EXIT, 0 ).width();
    const int32_t buttonMaxWidth = Assets::getImage( ICN::BUTTON_WELL_MAX, 0 ).width();
    // ICN::SMALLBAR image's first column contains all black pixels. This should not be drawn.
    fheroes2::Copy( bottomBar, 1, 0, background, buttonMaxWidth, bottomBarOffsetY, backgroundWidth / 2 - buttonMaxWidth, barHeight );
    fheroes2::Copy( bottomBar, bottomBar.width() - backgroundWidth / 2 + exitWidth - 1, 0, background, backgroundWidth / 2, bottomBarOffsetY,
                    backgroundWidth / 2 - exitWidth + 1, barHeight );

    // The original assets Well background has a transparent line to the right of EXIT button and it is not covered by any other image. Fill it with the black color.
    fheroes2::Fill( background, background.width() - 1, bottomBarOffsetY, 1, bottomBar.height(), static_cast<uint8_t>( 0 ) );

    const fheroes2::Rect textRoi{ 0, bottomBarOffsetY, backgroundWidth, bottomBar.height() - 1 };
    fheroes2::Text text( _( "Town Population Information and Statistics" ), fheroes2::FontType::normalWhite() );
    text.drawInRoi( 315 - text.width() / 2, bottomBarOffsetY + 3, background, textRoi );

    const fheroes2::FontType statsFontType = fheroes2::FontType::smallWhite();

    for ( const uint32_t dwellingType : castleDwellings ) {
        // By default the 'icnIndex' and 'offset' values are set for DWELLING_MONSTER1.
        uint32_t icnIndex = 19;
        fheroes2::Point offset{ 0, 1 };

        switch ( dwellingType ) {
        case DWELLING_MONSTER1:
            break;
        case DWELLING_MONSTER2:
            offset.y = 151;
            icnIndex = DWELLING_UPGRADE2 & _constructedBuildings ? 25 : 20;
            break;
        case DWELLING_MONSTER3:
            offset.y = 301;
            icnIndex = DWELLING_UPGRADE3 & _constructedBuildings ? 26 : 21;
            break;
        case DWELLING_MONSTER4:
            offset.x = 314;
            icnIndex = DWELLING_UPGRADE4 & _constructedBuildings ? 27 : 22;
            break;
        case DWELLING_MONSTER5:
            offset.x = 314;
            offset.y = 151;
            icnIndex = DWELLING_UPGRADE5 & _constructedBuildings ? 28 : 23;
            break;
        case DWELLING_MONSTER6:
            offset.x = 314;
            offset.y = 301;
            icnIndex = DWELLING_UPGRADE7 & _constructedBuildings ? 30 : ( DWELLING_UPGRADE6 & _constructedBuildings ? 29 : 24 );
            break;
        default:
            // Have you added a new dwelling?
            assert( 0 );
            break;
        }

        const uint32_t actualDwellindType = GetActualDwelling( dwellingType );
        const Monster monster( _race, actualDwellindType );

        // Dwelling building image.
        fheroes2::Point renderPoint( offset.x + 21, offset.y + 35 );
        const fheroes2::Sprite & dwellingImage = Assets::getImage( ICN::getBuildingIcnId( _race ), icnIndex );
        fheroes2::Copy( dwellingImage, 0, 0, background, renderPoint.x, renderPoint.y, dwellingImage.width(), dwellingImage.height() );

        // Dwelling name.
        text.set( GetStringBuilding( actualDwellindType, _race ), statsFontType );
        renderPoint.x = offset.x + 86 - text.width() / 2;
        renderPoint.y = offset.y + 104;
        text.draw( renderPoint.x, renderPoint.y, background );

        // Creature name.
        text.set( monster.GetMultiName(), statsFontType );
        renderPoint.x = offset.x + 122 - text.width() / 2;
        renderPoint.y = offset.y + 19;
        text.draw( renderPoint.x, renderPoint.y, background );

        // attack
        std::string textString;
        textString = _( "Attack" );
        textString += ": ";
        textString += std::to_string( monster.GetAttack() );

        text.set( textString, statsFontType );

        // Set initial X and Y offset.
        renderPoint.x = offset.x + 269;
        renderPoint.y = offset.y + 22;

        text.draw( renderPoint.x - text.width() / 2, renderPoint.y, background );
        renderPoint.y += text.height( text.width() );

        // defense
        textString = _( "Defense" );
        textString += ": ";
        textString += std::to_string( monster.GetDefense() );

        text.set( textString, statsFontType );
        text.draw( renderPoint.x - text.width() / 2, renderPoint.y, background );
        renderPoint.y += text.height( text.width() );

        // damage
        textString = _( "Damage" );
        textString += ": ";

        const uint32_t monsterMinDamage = monster.GetDamageMin();
        const uint32_t monsterMaxDamage = monster.GetDamageMax();

        textString += std::to_string( monsterMinDamage );

        if ( monsterMinDamage != monsterMaxDamage ) {
            textString += '-';
            textString += std::to_string( monsterMaxDamage );
        }

        text.set( textString, statsFontType );
        text.draw( renderPoint.x - text.width() / 2, renderPoint.y, background );
        renderPoint.y += text.height( text.width() );

        // hp
        textString = _( "HP" );
        textString += ": ";
        textString += std::to_string( monster.GetHitPoints() );

        text.set( textString, statsFontType );
        text.draw( renderPoint.x - text.width() / 2, renderPoint.y, background );
        renderPoint.y += 2 * ( text.height( text.width() ) ); // skip a line

        // speed
        textString = _( "Speed" );
        textString += ':';

        text.set( textString, statsFontType );
        text.draw( renderPoint.x - text.width() / 2, renderPoint.y, background );
        renderPoint.y += text.height( text.width() );

        text.set( Speed::String( monster.GetSpeed() ), statsFontType );
        text.draw( renderPoint.x - text.width() / 2, renderPoint.y, background );
        renderPoint.y += 2 * ( text.height( text.width() ) ); // skip a line

        // If dwelling is built show growth and available population.
        if ( _constructedBuildings & dwellingType ) {
            uint32_t monsterGrown = monster.GetGrown();
            monsterGrown += _constructedBuildings & BUILD_WELL ? GetGrownWell() : 0;

            if ( DWELLING_MONSTER1 & dwellingType ) {
                monsterGrown += _constructedBuildings & BUILD_WEL2 ? GetGrownWel2() : 0;
            }

            text.set( _( "Growth" ), statsFontType );
            text.draw( renderPoint.x - text.width() / 2, renderPoint.y, background );
            renderPoint.y += text.height( text.width() );

            textString = "+ ";
            textString += std::to_string( monsterGrown );
            textString += " / ";
            textString += _( "week" );

            text.set( textString, statsFontType );
            text.draw( renderPoint.x - text.width() / 2, renderPoint.y, background );

            _wellRedrawAvailableMonsters( dwellingType, false, background );
        }
    }
}

void Castle::_wellRedrawMonsterAnimation( const fheroes2::Rect & roi, std::array<fheroes2::RandomMonsterAnimation, maxNumOfDwellings> & monsterAnimInfo ) const
{
    fheroes2::Display & display = fheroes2::Display::instance();

    for ( size_t monsterId = 0; monsterId < maxNumOfDwellings; ++monsterId ) {
        fheroes2::Point outPos( roi.x, roi.y + 1 );

        switch ( monsterId ) {
        case 0:
            break;
        case 1:
            outPos.y += 150;
            break;
        case 2:
            outPos.y += 300;
            break;
        case 3:
            outPos.x += 314;
            break;
        case 4:
            outPos.x += 314;
            outPos.y += 150;
            break;
        case 5:
            outPos.x += 314;
            outPos.y += 300;
            break;
        default:
            // Have you added a new dwelling?
            assert( 0 );
            break;
        }

        const Monster monster( _race, GetActualDwelling( castleDwellings[monsterId] ) );

        // monster
        const bool flipMonsterSprite = ( monsterId > 2 );

        const fheroes2::Sprite & smonster = Assets::getImage( monsterAnimInfo[monsterId].icnFile(), monsterAnimInfo[monsterId].frameId() );
        if ( flipMonsterSprite ) {
            outPos.x += 193 - ( smonster.x() + smonster.width() ) + ( monster.isWide() ? Battle::Cell::widthPx / 2 : 0 ) + monsterAnimInfo[monsterId].offset();
        }
        else {
            outPos.x += 193 + smonster.x() - ( monster.isWide() ? Battle::Cell::widthPx / 2 : 0 ) - monsterAnimInfo[monsterId].offset();
        }

        outPos.y += 124 + smonster.y();

        fheroes2::Point inPos( 0, 0 );
        fheroes2::Size inSize( smonster.width(), smonster.height() );

        if ( fheroes2::FitToRoi( smonster, inPos, display, outPos, inSize, roi ) ) {
            fheroes2::Blit( smonster, inPos, display, outPos, inSize, flipMonsterSprite );
        }

        monsterAnimInfo[monsterId].increment();
    }
}
