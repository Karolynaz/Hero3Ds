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
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "army.h"
#include "army_bar.h"
#include "artifact.h"
#include "color.h"
#include "cursor.h"
#include "dialog.h"
#include "dialog_selectitems.h"
#include "game_assets.h"
#include "game_hotkeys.h"
#include "game_interface.h"
#include "heroes.h" // IWYU pragma: associated
#include "heroes_base.h"
#include "heroes_indicator.h"
#include "icn.h"
#include "image.h"
#include "interface_gamearea.h"
#include "kingdom.h"
#include "localevent.h"
#include "map_format_info.h"
#include "math_base.h"
#include "pal.h"
#include "race.h"
#include "screen.h"
#include "settings.h"
#include "skill.h"
#include "skill_bar.h"
#include "statusbar.h"
#include "tools.h"
#include "translations.h"
#include "ui_3ds.h"
#include "ui_button.h"
#include "ui_constants.h"
#include "ui_dialog.h"
#include "ui_language.h"
#include "ui_text.h"
#include "ui_tool.h"
#include "ui_window.h"

namespace
{
    const int32_t spellPointsMaxValue{ 999 };
}

int Heroes::OpenDialog( const bool readonly, const bool fade, const bool disableDismiss, const bool disableSwitch, const bool renderBackgroundDialog, const bool isEditor,
                        const fheroes2::SupportedLanguage language )
{
#if defined( TARGET_NINTENDO_3DS )
    if ( !isEditor ) {
        const fheroes2::Scope3DSNativeDialog nativeDialog( true );
        Cursor::Get().SetThemes( Cursor::POINTER );
        auto & display = fheroes2::Display::instance();
        fheroes2::draw3DSPanel( { 10, 10, 380, 220 }, GetName() );
        fheroes2::draw3DSPanel( { 0, 240, 320, 240 }, _( "Hero" ) );
        PortraitRedraw( 28, 45, PORT_BIG, display );
        const auto drawStats = [&]() {
            fheroes2::Fill( display, 140, 44, 230, 100, fheroes2::GetColorId( 0x51, 0x31, 0x18 ) );
            const std::array<std::string, 4> stats{ std::string( _( "Attack" ) ) + ": " + std::to_string( GetAttack() ),
                                                    std::string( _( "Defense" ) ) + ": " + std::to_string( GetDefense() ),
                                                    std::string( _( "Spell Power" ) ) + ": " + std::to_string( GetPower() ),
                                                    std::string( _( "Knowledge" ) ) + ": " + std::to_string( GetKnowledge() ) };
            for ( size_t index = 0; index < stats.size(); ++index ) {
                fheroes2::Text( stats[index], fheroes2::FontType::normalWhite() ).draw( 144, 46 + static_cast<int32_t>( index ) * 22, 220, display );
            }
            fheroes2::Fill( display, 26, 142, 348, 22, fheroes2::GetColorId( 0x51, 0x31, 0x18 ) );
            const std::string points = std::string( _( "Spell Points" ) ) + ": " + std::to_string( GetSpellPoints() ) + "/" + std::to_string( GetMaxSpellPoints() );
            fheroes2::Text( std::string( _( "Level" ) ) + ": " + std::to_string( GetLevel() ), fheroes2::FontType::normalWhite() ).draw( 28, 144, 110, display );
            fheroes2::Text( points, fheroes2::FontType::normalWhite() ).draw( 144, 144, 228, display );
            fheroes2::Fill( display, 26, 208, 348, 18, fheroes2::GetColorId( 0x51, 0x31, 0x18 ) );
            const std::string traits = std::string( _( "Morale" ) ) + ": " + std::to_string( GetMorale() ) + "   " + _( "Luck" ) + ": " + std::to_string( GetLuck() );
            fheroes2::Text( traits, fheroes2::FontType::normalWhite() ).draw( 28, 210, 344, display );
        };
        drawStats();
        SecondarySkillsBar secondarySkills( *this, true, false, false );
        secondarySkills.setTableSize( { 8, 1 } );
        secondarySkills.setInBetweenItemsOffset( { 2, 0 } );
        secondarySkills.SetContent( _secondarySkills.ToVector() );
        secondarySkills.setRenderingOffset( { 50, 172 } );
        secondarySkills.Redraw( display );
        ArmyBar army( &_army, true, readonly, false, true );
        army.setTableSize( { 5, 1 } );
        army.setInBetweenItemsOffset( { 9, 0 } );
        army.setRenderingOffset( { 34, 268 } );
        army.Redraw( display );
        fheroes2::Text( _( "Artifacts" ), fheroes2::FontType::normalWhite() ).draw( 12, 315, 296, display );
        StatusBar status;
        status.setRoi( { 12, 416, 296, 0 } );
        // Use the original miniature assets at their native size. All fourteen
        // inventory slots remain visible and refer to the original hero bag.
        ArtifactsBar artifacts( this, true, readonly, false, true, &status );
        artifacts.setTableSize( { 7, 2 } );
        artifacts.setInBetweenItemsOffset( { 7, 6 } );
        artifacts.SetContent( GetBagArtifacts() );
        artifacts.setRenderingOffset( { 20, 337 } );
        artifacts.Redraw( display );
        std::array<uint8_t, 19> cellIds{};
        for ( size_t index = 0; index < 5; ++index ) {
            fheroes2::register3DSDialogButton( &cellIds[index], { 34 + static_cast<int32_t>( index ) * 52, 268, 43, 43 }, true );
        }
        for ( size_t index = 0; index < 14; ++index ) {
            fheroes2::register3DSDialogButton( &cellIds[5 + index], { 20 + static_cast<int32_t>( index % 7 ) * 41, 337 + static_cast<int32_t>( index / 7 ) * 40, 34, 34 },
                                               true );
        }
        auto previous = fheroes2::make3DSButton( _( "Prev" ), { 7, 440, 72, 34 } );
        auto next = fheroes2::make3DSButton( _( "Next" ), { 85, 440, 72, 34 } );
        auto dismiss = fheroes2::make3DSButton( _( "Dismiss" ), { 163, 440, 72, 34 } );
        auto exit = fheroes2::make3DSButton( _( "Exit" ), { 241, 440, 72, 34 } );
        if ( readonly || disableSwitch || GetKingdom().GetHeroes().size() < 2 ) {
            previous.disable();
            next.disable();
        }
        if ( readonly || disableDismiss || inCastle() || Modes( NOTDISMISS ) ) {
            dismiss.disable();
        }
        previous.draw();
        next.draw();
        dismiss.draw();
        exit.draw();
        display.render();
        auto & le = LocalEvent::Get();
        while ( le.HandleEvents() ) {
            previous.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( previous.area() ) );
            next.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( next.area() ) );
            dismiss.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( dismiss.area() ) );
            exit.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( exit.area() ) );
            if ( Game::HotKeyCloseWindow() || le.MouseClickLeft( exit.area() ) ) {
                return Dialog::CANCEL;
            }
            if ( previous.isEnabled() && le.MouseClickLeft( previous.area() ) ) {
                return Dialog::PREV;
            }
            if ( next.isEnabled() && le.MouseClickLeft( next.area() ) ) {
                return Dialog::NEXT;
            }
            if ( dismiss.isEnabled() && le.MouseClickLeft( dismiss.area() )
                 && fheroes2::showStandardTextMessage( GetName(), _( "Are you sure you want to dismiss this Hero?" ), Dialog::YES | Dialog::NO ) == Dialog::YES ) {
                return Dialog::DISMISS;
            }
            std::string message;
            bool changed = false;
            if ( le.isMouseCursorPosInArea( army.GetArea() ) && army.QueueEventProcessing( &message ) ) {
                artifacts.ResetSelected();
                army.Redraw( display );
                changed = true;
            }
            else if ( le.isMouseCursorPosInArea( artifacts.GetArea() ) && artifacts.QueueEventProcessing( &message ) ) {
                army.ResetSelected();
                artifacts.Redraw( display );
                changed = true;
            }
            if ( changed ) {
                drawStats();
                display.render();
            }
        }
        return Dialog::CANCEL;
    }
#endif

    // Set the cursor image.This dialog does not require a cursor restorer. It is called from other dialogs that have the same cursor
    // or from the Game Area that will set the appropriate cursor after this dialog is closed.
    Cursor::Get().SetThemes( Cursor::POINTER );

    fheroes2::Display & display = fheroes2::Display::instance();

    fheroes2::Rect dialogRoi;
    fheroes2::Rect dialogWithShadowRoi;
    std::unique_ptr<fheroes2::StandardWindow> background;
    std::unique_ptr<fheroes2::ImageRestorer> restorer;

    std::unique_ptr<Maps::Map_Format::HeroMetadata> initialHeroMetadata;
    if ( isEditor ) {
        initialHeroMetadata = std::make_unique<Maps::Map_Format::HeroMetadata>( getHeroMetadata() );
    }

    if ( renderBackgroundDialog ) {
        background = std::make_unique<fheroes2::StandardWindow>( fheroes2::Display::DEFAULT_WIDTH, fheroes2::Display::DEFAULT_HEIGHT, false );
        dialogRoi = background->activeArea();
        dialogWithShadowRoi = background->totalArea();
    }
    else {
        dialogRoi = { ( display.width() - fheroes2::Display::DEFAULT_WIDTH ) / 2, ( display.height() - fheroes2::Display::DEFAULT_HEIGHT ) / 2,
                      fheroes2::Display::DEFAULT_WIDTH, fheroes2::Display::DEFAULT_HEIGHT };
        dialogWithShadowRoi = { dialogRoi.x - 2 * fheroes2::borderWidthPx, dialogRoi.y - fheroes2::borderWidthPx, dialogRoi.width + 3 * fheroes2::borderWidthPx,
                                dialogRoi.height + 3 * fheroes2::borderWidthPx };
        restorer = std::make_unique<fheroes2::ImageRestorer>( display, dialogRoi.x, dialogRoi.y, dialogRoi.width, dialogRoi.height );
    }

    // Fade-out game screen only for 640x480 resolution and if 'renderBackgroundDialog' is false (we are replacing image in already opened dialog).
    const bool isDefaultScreenSize = display.isDefaultSize();
    if ( fade && ( isDefaultScreenSize || !renderBackgroundDialog ) ) {
        fheroes2::fadeOutDisplay( dialogRoi, !isDefaultScreenSize );
    }

    const fheroes2::Sprite & backgroundImage = Assets::getImage( ICN::HEROBKG, 0 );
    fheroes2::Blit( backgroundImage, display, dialogRoi.x, dialogRoi.y );
    fheroes2::Blit( Assets::getImage( Settings::Get().isEvilInterfaceEnabled() ? ICN::HEROEXTE : ICN::HEROEXTG, 0 ), display, dialogRoi.x, dialogRoi.y );

    // Hero portrait.
    const fheroes2::Rect portPos( dialogRoi.x + 49, dialogRoi.y + 31, 101, 93 );
    if ( isEditor && !isValidId( _portrait ) ) {
        fheroes2::renderHeroRacePortrait( _race, portPos, display );
    }
    else {
        PortraitRedraw( portPos.x, portPos.y, PORT_BIG, display );
    }

    // In Editor there could be set to use "default" experience for hero. This state is transferred by setting 'experience = UINT32_MAX'.
    bool useDefaultExperience = isEditor && ( _experience == UINT32_MAX );

    if ( useDefaultExperience ) {
        _experience = _getStartingXp();
    }

    // Dialog title.
    const fheroes2::Rect titleRoi( dialogRoi.x + 60, dialogRoi.y + 1, 519, 17 );

    auto drawTitleText
        = [&display, &titleRoi, &dialogRoi, &backgroundImage, language, this]( const std::string & heroName, const int heroRace, const bool restoreBackground ) {
              if ( restoreBackground ) {
                  fheroes2::Copy( backgroundImage, titleRoi.x - dialogRoi.x, titleRoi.y - dialogRoi.y, display, titleRoi );
              }

              std::string titleText;
              if ( !heroName.empty() ) {
                  titleText = _( "%{name} the %{race} (Level %{level})" );
                  StringReplace( titleText, "%{race}", Race::String( heroRace ) );
                  StringReplace( titleText, "%{level}", GetLevel() );

                  const fheroes2::SupportedLanguage gameLanguage = fheroes2::getLanguageFromAbbreviation( Settings::Get().getGameLanguage() );

                  auto title = fheroes2::getLocalizedText( fheroes2::getLocalizedStrings( std::move( titleText ), gameLanguage, "%{name}", heroName, language ),
                                                           fheroes2::FontType::normalWhite() );
                  title->drawInRoi( titleRoi.x + ( titleRoi.width - title->width() ) / 2, titleRoi.y + 2, display, titleRoi );
                  return;
              }

              if ( heroRace == Race::RAND ) {
                  // In Editor the empty name is a sign that the default random hero (with a random name) will be used on the game start.
                  titleText = _( "Random hero (Level %{level})" );
              }
              else {
                  titleText = _( "Random %{race} hero (Level %{level})" );
              }
              StringReplace( titleText, "%{race}", Race::String( heroRace ) );
              StringReplace( titleText, "%{level}", GetLevel() );

              const fheroes2::Text title( std::move( titleText ), fheroes2::FontType::normalWhite() );
              title.drawInRoi( titleRoi.x + ( titleRoi.width - title.width() ) / 2, titleRoi.y + 2, display, titleRoi );
          };

    drawTitleText( _name, _race, false );

    fheroes2::Point dst_pt( dialogRoi.x + 156, dialogRoi.y + 31 );

    PrimarySkillsBar primarySkillsBar( this, false, isEditor, isEditor );
    primarySkillsBar.setTableSize( { 4, 1 } );
    primarySkillsBar.setInBetweenItemsOffset( { 6, 0 } );
    primarySkillsBar.setRenderingOffset( dst_pt );

    if ( isEditor ) {
        // In Editor '-1' means that the primary skill value is reset to its default state.
        // Here we consider any negative value as a default skill value state.
        if ( attack < 0 && defense < 0 && power < 0 && knowledge < 0 ) {
            primarySkillsBar.useDefaultValues();
        }

        if ( attack < 0 ) {
            attack = Heroes::getHeroDefaultSkillValue( Skill::Primary::ATTACK, _race );
        }
        if ( defense < 0 ) {
            defense = Heroes::getHeroDefaultSkillValue( Skill::Primary::DEFENSE, _race );
        }
        if ( power < 0 ) {
            power = Heroes::getHeroDefaultSkillValue( Skill::Primary::POWER, _race );
        }
        if ( knowledge < 0 ) {
            knowledge = Heroes::getHeroDefaultSkillValue( Skill::Primary::KNOWLEDGE, _race );
        }
    }

    primarySkillsBar.Redraw( display );

    // Morale indicator.
    dst_pt.x = dialogRoi.x + 514;
    dst_pt.y = dialogRoi.y + ( isEditor ? 44 : 34 );

    MoraleIndicator moraleIndicator( this );
    moraleIndicator.SetPos( dst_pt );
    moraleIndicator.Redraw();

    // Luck indicator.
    dst_pt.x += 36;

    LuckIndicator luckIndicator( this );
    luckIndicator.SetPos( dst_pt );
    luckIndicator.Redraw();

    // Army format spread icon.
    dst_pt.x = dialogRoi.x + 516;
    dst_pt.y = dialogRoi.y + 63;
    const fheroes2::Sprite & sprite1 = Assets::getImage( ICN::HSICONS, 9 );
    const fheroes2::Rect rectSpreadArmyFormat( dst_pt.x, dst_pt.y, sprite1.width(), sprite1.height() );
    const fheroes2::Point army1_pt( dst_pt.x - 1, dst_pt.y - 1 );

    // Army format grouped icon.
    dst_pt.x = dialogRoi.x + 552;
    const fheroes2::Sprite & sprite2 = Assets::getImage( ICN::HSICONS, 10 );
    const fheroes2::Rect rectGroupedArmyFormat( dst_pt.x, dst_pt.y, sprite2.width(), sprite2.height() );
    const fheroes2::Point army2_pt( dst_pt.x - 1, dst_pt.y - 1 );

    // Army format cursor.
    fheroes2::MovableSprite cursorFormat( Assets::getImage( ICN::HSICONS, 11 ) );
    const fheroes2::Point cursorFormatPos = _army.isSpreadFormation() ? army1_pt : army2_pt;

    // Do not show Army format in Editor.
    if ( !isEditor ) {
        fheroes2::Copy( sprite1, 0, 0, display, rectSpreadArmyFormat );
        fheroes2::Copy( sprite2, 0, 0, display, rectGroupedArmyFormat );
        cursorFormat.setPosition( cursorFormatPos.x, cursorFormatPos.y );
    }

    // Experience indicator.
    dst_pt.x = dialogRoi.x + 512;
    dst_pt.y = dialogRoi.y + ( isEditor ? 76 : 86 );
    ExperienceIndicator experienceInfo( this );
    experienceInfo.SetPos( dst_pt );
    if ( isEditor ) {
        experienceInfo.setDefaultState( useDefaultExperience );
    }
    experienceInfo.Redraw();

    // Spell points indicator.
    bool useDefaultSpellPoints = isEditor && ( GetSpellPoints() == UINT32_MAX );
    if ( useDefaultSpellPoints ) {
        SetSpellPoints( GetMaxSpellPoints() );
    }

    dst_pt.x += 38;
    dst_pt.y += 2;
    SpellPointsIndicator spellPointsInfo( this );
    spellPointsInfo.SetPos( dst_pt );
    if ( isEditor ) {
        spellPointsInfo.setDefaultState( useDefaultSpellPoints );
    }
    spellPointsInfo.Redraw();

    // Color icon or race change icon for jailed hero details edit.
    const fheroes2::Rect crestRect( portPos.x, portPos.y + 99, portPos.width, portPos.height );
    fheroes2::Rect raceRect;

    auto redrawRace = [&raceRect, &crestRect, &display]( const int race ) {
        const fheroes2::Sprite & raceSprite = Assets::getImage( ICN::NGEXTRA, Race::getRaceIcnIndex( race, true ) );
        fheroes2::Copy( raceSprite, 0, 0, display, raceRect );

        // Update race text background.
        const int32_t offsetY = raceRect.y - crestRect.y + raceRect.height + 9;
        const int32_t posY = crestRect.y + offsetY;
        const int32_t sizeY = crestRect.height - offsetY;
        fheroes2::ApplyPalette( Assets::getImage( ICN::STRIP, 3 ), 0, offsetY, display, crestRect.x, posY, crestRect.width, sizeY,
                                PAL::GetPalette( PAL::PaletteType::DARKENING ) );
        const fheroes2::Text raceText( Race::String( race ), fheroes2::FontType::smallWhite() );
        raceText.drawInRoi( crestRect.x, posY, crestRect.width, display, { crestRect.x, posY, crestRect.width, sizeY } );
    };

    if ( isEditor && Modes( JAIL ) ) {
        assert( GetColor() == PlayerColor::NONE );
        fheroes2::ApplyPalette( Assets::getImage( ICN::STRIP, 3 ), 0, 0, display, crestRect.x, crestRect.y, crestRect.width, crestRect.height,
                                PAL::GetPalette( PAL::PaletteType::DARKENING ) );

        const fheroes2::Text raceText( _( "Hero class:" ), fheroes2::FontType::normalWhite() );
        raceText.drawInRoi( crestRect.x, crestRect.y + 6, crestRect.width, display, crestRect );

        const fheroes2::Sprite & raceSprite = Assets::getImage( ICN::NGEXTRA, Race::getRaceIcnIndex( _race, true ) );
        raceRect.width = raceSprite.width();
        raceRect.height = raceSprite.height();
        raceRect.x = crestRect.x + ( crestRect.width - raceRect.width ) / 2;
        raceRect.y = crestRect.y + ( crestRect.height - raceRect.height ) / 2;

        fheroes2::Blit( Assets::getImage( ICN::NGEXTRA, 61U ), display, raceRect.x - 5, raceRect.y + 3 );

        redrawRace( _race );
    }
    else {
        // Color "crest" icon.
        const PlayerColor color = GetColor();
        const uint32_t icnIndex = Color::GetIndex( color == PlayerColor::NONE ? Settings::Get().CurrentColor() : color );

        fheroes2::Copy( Assets::getImage( ICN::CREST, icnIndex ), 0, 0, display, crestRect );
    }

    // Hero's army.
    dst_pt.x = dialogRoi.x + 156;
    dst_pt.y = dialogRoi.y + 130;

    // In Editor mode we allow to edit army and remove all customized troops from the army.
    ArmyBar selectArmy( &_army, false, readonly, isEditor, !isEditor );
    selectArmy.setTableSize( { 5, 1 } );
    selectArmy.setRenderingOffset( dst_pt );
    selectArmy.setInBetweenItemsOffset( { 6, 0 } );
    selectArmy.Redraw( display );

    // Hero's secondary skills.
    SecondarySkillsBar secskill_bar( *this, false, isEditor, isEditor );
    secskill_bar.setTableSize( { 8, 1 } );
    secskill_bar.setInBetweenItemsOffset( { 5, 0 } );
    secskill_bar.SetContent( _secondarySkills.ToVector() );
    secskill_bar.setRenderingOffset( { dialogRoi.x + 3, dialogRoi.y + 233 } );
    secskill_bar.Redraw( display );

    // Status bar.
    dst_pt.x = dialogRoi.x + 22;
    dst_pt.y = dialogRoi.y + 460;
    const fheroes2::Sprite & bar = Assets::getImage( ICN::HSBTNS, 8 );
    fheroes2::Copy( bar, 0, 0, display, dst_pt.x, dst_pt.y, bar.width(), bar.height() );

    StatusBar statusBar;
    // Status bar must be smaller due to extra art on both sides.
    statusBar.setRoi( { dst_pt.x + 16, dst_pt.y + 1, bar.width() - 16 * 2, 0 } );

    // Artifacts bar.
    dst_pt.x = dialogRoi.x + 51;
    dst_pt.y = dialogRoi.y + 308;

    ArtifactsBar selectArtifacts( this, false, readonly, isEditor, true, &statusBar );
    selectArtifacts.setTableSize( { 7, 2 } );
    selectArtifacts.setInBetweenItemsOffset( { 15, 15 } );
    selectArtifacts.SetContent( GetBagArtifacts() );
    selectArtifacts.setRenderingOffset( dst_pt );
    selectArtifacts.Redraw( display );

    // Previous hero button.
    dst_pt.x = dialogRoi.x;
    dst_pt.y = dialogRoi.y + fheroes2::Display::DEFAULT_HEIGHT - 20;
    fheroes2::Button buttonPrevHero( dst_pt.x, dst_pt.y, ICN::HSBTNS, 4, 5 );
    fheroes2::TimedEventValidator timedButtonPrevHero( [&buttonPrevHero]() { return buttonPrevHero.isPressed(); } );
    buttonPrevHero.subscribe( &timedButtonPrevHero );

    // Next hero button.
    dst_pt.x += fheroes2::Display::DEFAULT_WIDTH - 22;
    fheroes2::Button buttonNextHero( dst_pt.x, dst_pt.y, ICN::HSBTNS, 6, 7 );
    fheroes2::TimedEventValidator timedButtonNextHero( [&buttonNextHero]() { return buttonNextHero.isPressed(); } );
    buttonNextHero.subscribe( &timedButtonNextHero );

    // Hero dismiss button.
    dst_pt.x = dialogRoi.x + 9;
    dst_pt.y = dialogRoi.y + 378;

    std::unique_ptr<fheroes2::ButtonSprite> buttonDismiss;

    if ( !isEditor && !readonly && !disableDismiss ) {
        const fheroes2::Sprite & dismissReleased = Assets::getImage( ICN::BUTTON_VERTICAL_DISMISS, 0 );
        buttonDismiss
            = std::make_unique<fheroes2::ButtonSprite>( dst_pt.x, dst_pt.y - dismissReleased.height() / 2, dismissReleased,
                                                        Assets::getImage( ICN::BUTTON_VERTICAL_DISMISS, 1 ), Assets::getImage( ICN::DISMISS_HERO_DISABLED_BUTTON, 0 ) );

        if ( inCastle() || readonly || disableDismiss || Modes( NOTDISMISS ) ) {
            buttonDismiss->disable();
        }

        fheroes2::addGradientShadow( dismissReleased, display, { dst_pt.x, dst_pt.y - dismissReleased.height() / 2 }, { -3, 5 } );

        buttonDismiss->draw();
    }

    // Hero Patrol mode button (used in Editor).
    std::unique_ptr<fheroes2::ButtonSprite> buttonPatrol;

    if ( isEditor ) {
        const fheroes2::Sprite & patrolReleased = Assets::getImage( ICN::BUTTON_VERTICAL_PATROL, 0 );
        buttonPatrol = std::make_unique<fheroes2::ButtonSprite>( dst_pt.x, dst_pt.y - patrolReleased.height() / 2, patrolReleased,
                                                                 Assets::getImage( ICN::BUTTON_VERTICAL_PATROL, 1 ) );

        fheroes2::addGradientShadow( patrolReleased, display, { dialogRoi.x + 9, dst_pt.y - patrolReleased.height() / 2 }, { -3, 5 } );

        if ( Modes( PATROL ) ) {
            buttonPatrol->press();
        }

        buttonPatrol->draw();
    }

    // Exit button.
    dst_pt.x = dialogRoi.x + 602;
    const fheroes2::Sprite & exitReleased = Assets::getImage( ICN::BUTTON_VERTICAL_EXIT, 0 );
    fheroes2::ButtonSprite buttonExit( dst_pt.x, dst_pt.y - exitReleased.height() / 2, exitReleased, Assets::getImage( ICN::BUTTON_VERTICAL_EXIT, 1 ) );

    LocalEvent & le = LocalEvent::Get();

    if ( readonly || disableSwitch || 2 > GetKingdom().GetHeroes().size() ) {
        buttonNextHero.disable();
        buttonPrevHero.disable();
    }

    buttonPrevHero.draw();
    buttonNextHero.draw();
    buttonExit.draw();

    // Fade-in hero dialog.
    if ( fade ) {
        if ( renderBackgroundDialog && !isDefaultScreenSize ) {
            // We need to expand the ROI for the next render to properly render window borders and shadow.
            display.updateNextRenderRoi( dialogWithShadowRoi );
        }

        // Use half fade if game resolution is not 640x480.
        fheroes2::fadeInDisplay( dialogRoi, !isDefaultScreenSize );
    }
    else {
        display.render( dialogWithShadowRoi );
    }

    bool needRedraw{ false };
    std::string message;

    // dialog menu loop
    while ( le.HandleEvents() ) {
        if ( le.MouseClickLeft( buttonExit.area() ) || Game::HotKeyCloseWindow() ) {
            // Exit the dialog handling loop to close it.
            break;
        }

        buttonExit.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonExit.area() ) );

        // Manage hero's army.
        if ( le.isMouseCursorPosInArea( selectArmy.GetArea() ) && selectArmy.QueueEventProcessing( &message ) ) {
            if ( selectArtifacts.isSelected() ) {
                selectArtifacts.ResetSelected();
            }
            selectArmy.Redraw( display );
            moraleIndicator.Redraw();
            luckIndicator.Redraw();

            needRedraw = true;
        }

        // Manage hero's artifacts.
        else if ( le.isMouseCursorPosInArea( selectArtifacts.GetArea() ) && selectArtifacts.QueueEventProcessing( &message ) ) {
            if ( selectArmy.isSelected() ) {
                selectArmy.ResetSelected();
            }
            selectArtifacts.Redraw( display );

            if ( isEditor ) {
                // Artifacts affect many hero stats.
                if ( useDefaultSpellPoints ) {
                    SetSpellPoints( GetMaxSpellPoints() );
                }

                spellPointsInfo.Redraw();
                moraleIndicator.Redraw();
                luckIndicator.Redraw();
                primarySkillsBar.Redraw( display );
            }

            needRedraw = true;
        }

        // Dismiss hero.
        else if ( buttonDismiss && buttonDismiss->isEnabled() ) {
            buttonDismiss->drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonDismiss->area() ) || HotKeyPressEvent( Game::HotKeyEvent::ARMY_DISMISS ) );

            if ( ( le.MouseClickLeft( buttonDismiss->area() ) || Game::HotKeyPressEvent( Game::HotKeyEvent::ARMY_DISMISS ) )
                 && Dialog::YES == fheroes2::showStandardTextMessage( GetName(), _( "Are you sure you want to dismiss this Hero?" ), Dialog::YES | Dialog::NO ) ) {
                // Fade-out hero dialog.
                fheroes2::fadeOutDisplay( dialogRoi, !isDefaultScreenSize );

                return Dialog::DISMISS;
            }
        }

        // Previous hero.
        if ( buttonPrevHero.isEnabled() ) {
            buttonPrevHero.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonPrevHero.area() ) );
            if ( le.MouseClickLeft( buttonPrevHero.area() ) || HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_LEFT ) || timedButtonPrevHero.isDelayPassed() ) {
                return Dialog::PREV;
            }
        }

        // Next hero.
        if ( buttonNextHero.isEnabled() ) {
            buttonNextHero.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonNextHero.area() ) );
            if ( le.MouseClickLeft( buttonNextHero.area() ) || HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_RIGHT ) || timedButtonNextHero.isDelayPassed() ) {
                return Dialog::NEXT;
            }
        }

        if ( le.isMouseCursorPosInArea( moraleIndicator.GetArea() ) ) {
            MoraleIndicator::QueueEventProcessing( moraleIndicator );
            message = fheroes2::MoraleString( _army.GetMorale() );
        }
        else if ( le.isMouseCursorPosInArea( luckIndicator.GetArea() ) ) {
            LuckIndicator::QueueEventProcessing( luckIndicator );
            message = fheroes2::LuckString( _army.GetLuck() );
        }
        else if ( le.isMouseCursorPosInArea( experienceInfo.GetArea() ) ) {
            if ( isEditor ) {
                message = useDefaultExperience ? _( "Set custom Experience value. Current value is default." )
                                               : _( "Change Experience value. Right-click to reset to default value." );

                if ( le.MouseClickLeft() ) {
                    const fheroes2::ExperienceDialogElement tempExperienceUI{ 0 };
                    int32_t value = static_cast<int32_t>( _experience );

                    if ( Dialog::SelectCount( _( "Set Experience value:" ), 0, static_cast<int32_t>( Heroes::getExperienceMaxValue() ), value, 1, &tempExperienceUI ) ) {
                        useDefaultExperience = false;
                        _experience = static_cast<uint32_t>( value );
                        experienceInfo.setDefaultState( useDefaultExperience );
                        experienceInfo.Redraw();
                        drawTitleText( _name, _race, true );
                        needRedraw = true;
                    }
                }
                else if ( le.MouseClickRight() ) {
                    useDefaultExperience = true;
                    _experience = _getStartingXp();
                    experienceInfo.setDefaultState( useDefaultExperience );
                    experienceInfo.Redraw();
                    drawTitleText( _name, _race, true );
                    needRedraw = true;
                }
            }
            else {
                message = _( "View Experience Info" );
                experienceInfo.QueueEventProcessing();
            }
        }
        else if ( le.isMouseCursorPosInArea( spellPointsInfo.GetArea() ) ) {
            if ( isEditor ) {
                message = useDefaultSpellPoints ? _( "Set custom Spell Points value. Current value is default." )
                                                : _( "Change Spell Points value. Right-click to reset to default value." );

                if ( le.MouseClickLeft() ) {
                    int32_t value = static_cast<int32_t>( GetSpellPoints() );
                    if ( Dialog::SelectCount( _( "Set Spell Points value:" ), 0, std::max( spellPointsMaxValue, value ), value ) ) {
                        useDefaultSpellPoints = false;
                        SetSpellPoints( static_cast<uint32_t>( value ) );
                        spellPointsInfo.setDefaultState( useDefaultSpellPoints );
                        spellPointsInfo.Redraw();
                        needRedraw = true;
                    }
                }
                else if ( le.MouseClickRight() ) {
                    // Reset spell points modification.
                    useDefaultSpellPoints = true;
                    SetSpellPoints( GetMaxSpellPoints() );
                    spellPointsInfo.setDefaultState( useDefaultSpellPoints );
                    spellPointsInfo.Redraw();
                    needRedraw = true;
                }
            }
            else {
                message = _( "View Spell Points Info" );
                spellPointsInfo.QueueEventProcessing();
            }
        }
        else if ( !readonly && !isEditor && le.MouseClickLeft( rectSpreadArmyFormat ) && !_army.isSpreadFormation() ) {
            cursorFormat.setPosition( army1_pt.x, army1_pt.y );
            needRedraw = true;
            _army.SetSpreadFormation( true );
        }
        else if ( !readonly && !isEditor && le.MouseClickLeft( rectGroupedArmyFormat ) && _army.isSpreadFormation() ) {
            cursorFormat.setPosition( army2_pt.x, army2_pt.y );
            needRedraw = true;
            _army.SetSpreadFormation( false );
        }
        else if ( le.isMouseCursorPosInArea( secskill_bar.GetArea() ) && secskill_bar.QueueEventProcessing( &message ) ) {
            if ( isEditor ) {
                // The change of secondary skills affects many hero stats.
                secskill_bar.Redraw( display );
                moraleIndicator.Redraw();
                luckIndicator.Redraw();
                needRedraw = true;
            }
        }
        else if ( le.isMouseCursorPosInArea( primarySkillsBar.GetArea() ) && primarySkillsBar.QueueEventProcessing( &message ) ) {
            if ( isEditor ) {
                primarySkillsBar.Redraw( display );

                // Hero's Knowledge may have changed.
                if ( useDefaultSpellPoints ) {
                    SetSpellPoints( GetMaxSpellPoints() );
                }

                spellPointsInfo.Redraw();
                needRedraw = true;
            }
        }
        else if ( isEditor && le.MouseClickLeft( portPos ) ) {
            const int newPortrait = Dialog::selectHeroes( _portrait );
            if ( newPortrait != Heroes::UNKNOWN ) {
                _portrait = newPortrait;
                PortraitRedraw( portPos.x, portPos.y, PORT_BIG, display );
                needRedraw = true;
            }
        }
        else if ( le.isMouseRightButtonPressedInArea( portPos ) ) {
            if ( isEditor ) {
                _portrait = 0;
                fheroes2::renderHeroRacePortrait( _race, portPos, display );
                needRedraw = true;
            }
            else {
                Dialog::QuickInfo( *this, true );
            }
        }
        else if ( buttonPatrol && le.isMouseCursorPosInArea( buttonPatrol->area() ) ) {
            if ( le.isMouseLeftButtonPressed() && buttonPatrol->isReleased() && !Modes( PATROL ) ) {
                buttonPatrol->drawOnPress();
                int32_t value = static_cast<int32_t>( _patrolDistance );
                if ( Dialog::SelectCount( _( "Set patrol radius in tiles:" ), 0, 255, value ) ) {
                    SetModes( PATROL );
                    _patrolDistance = static_cast<uint32_t>( value );
                }
                else {
                    buttonPatrol->drawOnRelease();
                    ResetModes( PATROL );
                }
            }
            else if ( le.MouseClickLeft() && buttonPatrol->isPressed() && Modes( PATROL ) ) {
                ResetModes( PATROL );
            }
            if ( !Modes( PATROL ) ) {
                buttonPatrol->drawOnRelease();
            }
        }
        else if ( isEditor ) {
            if ( le.MouseClickLeft( titleRoi ) ) {
                std::string res = _name;
                const fheroes2::Text body{ _( "Enter hero's name:" ), fheroes2::FontType::normalWhite() };

                if ( Dialog::inputString( fheroes2::Text{}, body, res, Maps::Map_Format::nameCharLimit, false, language ) && !res.empty() ) {
                    _name = std::move( res );
                    drawTitleText( _name, _race, true );
                    needRedraw = true;
                }
            }
            else if ( le.MouseClickRight( titleRoi ) ) {
                _name.clear();
                drawTitleText( _name, _race, true );
                needRedraw = true;
            }
            else if ( le.isMouseCursorPosInArea( raceRect ) ) {
                assert( !needRedraw );
                message = _( "Click to change class." );
                if ( le.MouseClickLeft() || le.isMouseWheelDown() ) {
                    _race = Race::getNextRace( _race );
                    needRedraw = true;
                }
                else if ( le.MouseClickRight() || le.isMouseWheelUp() ) {
                    _race = Race::getPreviousRace( _race );
                    needRedraw = true;
                }

                if ( needRedraw ) {
                    drawTitleText( _name, _race, true );
                    redrawRace( _race );

                    if ( _portrait == 0 ) {
                        fheroes2::renderHeroRacePortrait( _race, portPos, display );
                    }

                    if ( primarySkillsBar.isDefaultValues() ) {
                        attack = getHeroDefaultSkillValue( Skill::Primary::ATTACK, _race );
                        defense = getHeroDefaultSkillValue( Skill::Primary::DEFENSE, _race );
                        power = getHeroDefaultSkillValue( Skill::Primary::POWER, _race );
                        knowledge = getHeroDefaultSkillValue( Skill::Primary::KNOWLEDGE, _race );
                        primarySkillsBar.Redraw( display );

                        if ( useDefaultSpellPoints ) {
                            SetSpellPoints( GetMaxSpellPoints() );
                        }

                        spellPointsInfo.Redraw();
                    }
                }
            }
        }
        else {
            // If dialog is opened not in Editor.
            if ( le.isMouseRightButtonPressedInArea( rectSpreadArmyFormat ) ) {
                fheroes2::showStandardTextMessage(
                    _( "Spread Formation" ),
                    _( "'Spread' combat formation spreads the hero's units from the top to the bottom of the battlefield, with at least one empty space between each unit." ),
                    Dialog::ZERO );
            }
            else if ( le.isMouseRightButtonPressedInArea( rectGroupedArmyFormat ) ) {
                fheroes2::showStandardTextMessage( _( "Grouped Formation" ),
                                                   _( "'Grouped' combat formation bunches the hero's army together in the center of their side of the battlefield." ),
                                                   Dialog::ZERO );
            }
        }

        // Status messages.
        if ( le.isMouseCursorPosInArea( buttonExit.area() ) ) {
            message = _( "Exit Hero Screen" );
        }
        else if ( buttonDismiss && le.isMouseCursorPosInArea( buttonDismiss->area() ) ) {
            if ( inCastle() ) {
                message = _( "You cannot dismiss a hero in a castle" );
            }
            else if ( Modes( NOTDISMISS ) ) {
                message = _( "Dismissal of %{name} the %{race} is prohibited by scenario" );
                StringReplace( message, "%{name}", _name );
                StringReplace( message, "%{race}", Race::String( _race ) );
            }
            else if ( buttonDismiss->isEnabled() ) {
                message = _( "Dismiss %{name} the %{race}" );
                StringReplace( message, "%{name}", _name );
                StringReplace( message, "%{race}", Race::String( _race ) );
            }
        }
        else if ( buttonPrevHero.isEnabled() && le.isMouseCursorPosInArea( buttonPrevHero.area() ) ) {
            message = _( "Show previous hero" );
        }
        else if ( buttonNextHero.isEnabled() && le.isMouseCursorPosInArea( buttonNextHero.area() ) ) {
            message = _( "Show next hero" );
        }
        else if ( !isEditor ) {
            // These messages are not shown in the Editor mode.
            if ( le.isMouseCursorPosInArea( rectSpreadArmyFormat ) ) {
                message = _( "Set army combat formation to 'Spread'" );
            }
            else if ( le.isMouseCursorPosInArea( rectGroupedArmyFormat ) ) {
                message = _( "Set army combat formation to 'Grouped'" );
            }
        }
        else if ( message.empty() ) {
            // Editor related status messages.
            if ( le.isMouseCursorPosInArea( selectArtifacts.GetArea() ) ) {
                message = _( "Set hero's Artifacts. Right-click to reset Artifact." );
            }
            else if ( le.isMouseCursorPosInArea( secskill_bar.GetArea() ) ) {
                message = _( "Set hero's Secondary Skills. Right-click to reset skill." );
            }
            else if ( le.isMouseCursorPosInArea( selectArmy.GetArea() ) ) {
                message = _( "Set hero's Army. Right-click to reset unit." );
            }
            else if ( le.isMouseCursorPosInArea( portPos ) ) {
                message = _( "Set hero's portrait. Right-click to reset to default." );
            }
            else if ( le.isMouseCursorPosInArea( titleRoi ) ) {
                message = _( "Click to change hero's name. Right-click to reset to default." );
            }
            else if ( buttonPatrol && le.isMouseCursorPosInArea( buttonPatrol->area() ) ) {
                if ( buttonPatrol->isPressed() ) {
                    message = _( "Hero is in patrol mode in %{tiles} tiles radius. Click to disable it." );
                    StringReplace( message, "%{tiles}", _patrolDistance );
                }
                else {
                    message = _( "Click to enable hero's patrol mode." );
                }
            }
        }

        if ( message.empty() ) {
            statusBar.ShowMessage( _( "Hero Screen" ) );
        }
        else {
            statusBar.ShowMessage( std::move( message ) );
            message.clear();
        }

        if ( needRedraw ) {
            needRedraw = false;
            display.render( dialogRoi );
        }
    }

    // Disable fast scroll for resolutions where the exit button is directly above the border.
    Interface::AdventureMap::Get().getGameArea().setFastScrollStatus( false );

    buttonExit.drawOnPress();

    if ( isEditor ) {
        if ( useDefaultExperience ) {
            // Tell Editor that default experience value is set.
            _experience = UINT32_MAX;
        }

        if ( useDefaultSpellPoints ) {
            // Tell Editor that default Spell Points value is set.
            SetSpellPoints( UINT32_MAX );
        }

        // For Editor '-1' means that the primary skill is reset to its default state.
        if ( primarySkillsBar.isDefaultValues() ) {
            attack = -1;
            defense = -1;
            power = -1;
            knowledge = -1;
        }

        assert( initialHeroMetadata.get() != nullptr );

        const auto currentMetadata = getHeroMetadata();
        if ( currentMetadata != *initialHeroMetadata ) {
            if ( fheroes2::showStandardTextMessage( GetName(), _( "Do you want to save the changes made?" ), Dialog::YES | Dialog::NO ) == Dialog::NO ) {
                return Dialog::DISMISS;
            }
        }
        else {
            // This could happen when we automatically change the location of certain items.
            // For example, when pushing artifacts we put them to the first position.
            return Dialog::DISMISS;
        }
    }

    // Fade-out hero dialog.
    fheroes2::fadeOutDisplay( dialogRoi, !isDefaultScreenSize );

    return Dialog::CANCEL;
}
