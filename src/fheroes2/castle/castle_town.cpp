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
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "army.h"
#include "artifact.h"
#include "buildinginfo.h"
#include "captain.h"
#include "castle.h" // IWYU pragma: associated
#include "castle_building_info.h"
#include "cursor.h"
#include "dialog.h"
#include "game_assets.h"
#include "game_hotkeys.h"
#include "heroes.h"
#include "heroes_base.h"
#include "heroes_recruits.h"
#include "icn.h"
#include "image.h"
#include "kingdom.h"
#include "localevent.h"
#include "maps_fileinfo.h"
#include "math_base.h"
#include "pal.h"
#include "payment.h"
#include "race.h"
#include "resource.h"
#include "screen.h"
#include "settings.h"
#include "skill.h"
#include "statusbar.h"
#include "tools.h"
#include "translations.h"
#include "ui_3ds.h"
#include "ui_button.h"
#include "ui_castle.h"
#include "ui_constants.h"
#include "ui_dialog.h"
#include "ui_kingdom.h"
#include "ui_language.h"
#include "ui_text.h"
#include "ui_tool.h"
#include "world.h"

#if defined( TARGET_NINTENDO_3DS )
namespace
{
    constexpr int32_t kTopWidth3DS = 400;
    constexpr int32_t kTopHeight3DS = 240;
    constexpr int32_t kBottomX3DS = 0;
    constexpr int32_t kBottomY3DS = 240;
    constexpr int32_t kBottomWidth3DS = 320;
    constexpr int32_t kBottomHeight3DS = 240;

    constexpr int32_t kCardWidth3DS = 92;
    constexpr int32_t kCardHeight3DS = 56;
    constexpr int32_t kCardArtW3DS = 90;
    constexpr int32_t kCardArtH3DS = 43;
    constexpr int32_t kCardPlateH3DS = 11;
    constexpr int32_t kGridCols3DS = 4;
    constexpr int32_t kGridMarginX3DS = 7;
    constexpr int32_t kGridGapX3DS = 6;
    constexpr int32_t kGridGapY3DS = 5;
    constexpr int32_t kHeaderHeight3DS = 14;
    constexpr int32_t kFadeBandHeight3DS = 22;

    struct BuildCard3DS final
    {
        BuildingType baseType{ BUILD_NOTHING };
        BuildingInfo info;
        bool isBuilt{ false };
        bool isUpgradeable{ false };
        fheroes2::Rect rect{};
        int32_t gridRow{ 0 };
        int32_t gridCol{ 0 };

        BuildCard3DS( const Castle & castle, const BuildingType type )
            : baseType( type )
            , info( castle, type )
        {
            const bool isBaseBuilt = ( ( type & BUILD_MAGEGUILD ) != 0 ) ? ( castle.GetLevelMageGuild() > 0 ) : castle.isBuild( type );
            isBuilt = ( info.getStatus() == BuildingStatus::ALREADY_BUILT );
            isUpgradeable = !isBuilt && isBaseBuilt;
        }
    };

    struct HeaderEntry3DS final
    {
        int32_t y{ 0 };
        std::string title;
    };

    void tileStoneBackground3DS( fheroes2::Image & dst, const fheroes2::Rect & roi, const bool isEvil )
    {
        const fheroes2::Sprite & tile = Assets::getImage( isEvil ? ICN::STONEBAK_EVIL : ICN::STONEBAK, 0 );
        if ( tile.empty() ) {
            fheroes2::Fill( dst, roi.x, roi.y, roi.width, roi.height, fheroes2::GetColorId( 0x38, 0x24, 0x14 ) );
            return;
        }

        const int32_t maxX = roi.x + roi.width;
        const int32_t maxY = roi.y + roi.height;
        for ( int32_t y = roi.y; y < maxY; y += tile.height() ) {
            const int32_t h = std::min<int32_t>( tile.height(), maxY - y );
            for ( int32_t x = roi.x; x < maxX; x += tile.width() ) {
                const int32_t w = std::min<int32_t>( tile.width(), maxX - x );
                fheroes2::Copy( tile, 0, 0, dst, x, y, w, h );
            }
        }
    }

    std::array<fheroes2::Image, 7> createMiniResourceIcons3DS()
    {
        std::array<fheroes2::Image, 7> icons;
        for ( int i = 0; i < 7; ++i ) {
            const fheroes2::Sprite & src = Assets::getImage( ICN::RESOURCE, i );
            icons[i] = fheroes2::Image( 9, 9 );
            icons[i].reset();
            if ( !src.empty() ) {
                fheroes2::Resize( src, icons[i] );
            }
        }
        return icons;
    }

    void drawGroupHeader3DS( fheroes2::Image & dst, const int32_t y, const std::string & title )
    {
        const uint8_t darkShadow = fheroes2::GetColorId( 0x16, 0x0C, 0x06 );
        const uint8_t bannerFill = fheroes2::GetColorId( 0x30, 0x1C, 0x0E );
        const uint8_t goldBright = fheroes2::GetColorId( 0xE8, 0xC8, 0x48 );
        const uint8_t goldMid = fheroes2::GetColorId( 0x9C, 0x74, 0x24 );

        const fheroes2::Text label( title, fheroes2::FontType::smallYellow() );
        const int32_t titleW = label.width();
        const int32_t centerX = dst.width() / 2;
        const int32_t titleX = centerX - titleW / 2;
        const int32_t padX = 10;
        const int32_t boxX = std::max<int32_t>( 40, titleX - padX );
        const int32_t boxW = std::min<int32_t>( dst.width() - 80, titleW + padX * 2 );

        // Ornamental flanking wings
        const int32_t midY = y + kHeaderHeight3DS / 2;
        const int32_t leftEnd = boxX - 4;
        const int32_t rightStart = boxX + boxW + 4;
        if ( leftEnd > 10 ) {
            fheroes2::DrawLine( dst, { 10, midY }, { leftEnd, midY }, goldMid );
            fheroes2::DrawLine( dst, { 10, midY + 1 }, { leftEnd, midY + 1 }, darkShadow );
            fheroes2::Fill( dst, leftEnd - 2, midY - 1, 3, 3, goldBright );
            fheroes2::Fill( dst, 10, midY - 1, 3, 3, goldBright );
        }
        if ( rightStart < dst.width() - 10 ) {
            fheroes2::DrawLine( dst, { rightStart, midY }, { dst.width() - 11, midY }, goldMid );
            fheroes2::DrawLine( dst, { rightStart, midY + 1 }, { dst.width() - 11, midY + 1 }, darkShadow );
            fheroes2::Fill( dst, rightStart, midY - 1, 3, 3, goldBright );
            fheroes2::Fill( dst, dst.width() - 13, midY - 1, 3, 3, goldBright );
        }

        // Framed plaque behind group title
        fheroes2::Fill( dst, boxX, y, boxW, kHeaderHeight3DS, bannerFill );
        fheroes2::DrawRect( dst, { boxX, y, boxW, kHeaderHeight3DS }, goldMid );
        fheroes2::DrawRect( dst, { boxX + 1, y + 1, boxW - 2, kHeaderHeight3DS - 2 }, darkShadow );
        fheroes2::Fill( dst, boxX, y, 2, 2, goldBright );
        fheroes2::Fill( dst, boxX + boxW - 2, y, 2, 2, goldBright );
        fheroes2::Fill( dst, boxX, y + kHeaderHeight3DS - 2, 2, 2, goldBright );
        fheroes2::Fill( dst, boxX + boxW - 2, y + kHeaderHeight3DS - 2, 2, 2, goldBright );

        label.draw( titleX, y + ( kHeaderHeight3DS - label.height() ) / 2, dst );
    }

    void drawUpgradeBadge3DS( fheroes2::Image & dst, const int32_t x, const int32_t y, const BuildingStatus status )
    {
        const int32_t size = 12;
        const uint8_t bg = fheroes2::GetColorId( 0x18, 0x10, 0x08 );
        const uint8_t border = fheroes2::GetColorId( 0xE0, 0xC0, 0x48 );

        uint8_t arrow = fheroes2::GetColorId( 0xE4, 0x4C, 0x40 );
        if ( status == BuildingStatus::ALLOW_BUILD ) {
            arrow = fheroes2::GetColorId( 0x54, 0xDC, 0x54 );
        }
        else if ( status == BuildingStatus::LACK_RESOURCES ) {
            arrow = fheroes2::GetColorId( 0xF4, 0xD0, 0x38 );
        }

        fheroes2::Fill( dst, x, y, size, size, bg );
        fheroes2::DrawRect( dst, { x, y, size, size }, border );

        // Double upward chevron inside the badge
        for ( int row = 0; row < 3; ++row ) {
            const int32_t w = 1 + row * 2;
            const int32_t cx = x + size / 2 - row;
            fheroes2::Fill( dst, cx, y + 2 + row, w, 1, arrow );
            fheroes2::Fill( dst, cx, y + 6 + row, w, 1, arrow );
        }
    }

    void drawCardCostOverlay3DS( fheroes2::Image & cardImg, const Funds & cost, const std::array<fheroes2::Image, 7> & miniIcons, const int32_t artBottomY )
    {
        struct CostPill final
        {
            int iconIdx{ 0 };
            std::string amount;
            int32_t textW{ 0 };
            int32_t totalW{ 0 };
        };

        const std::array<std::pair<int, int32_t>, 7> entries = { {
            { 0, cost.wood },
            { 2, cost.ore },
            { 1, cost.mercury },
            { 3, cost.sulfur },
            { 4, cost.crystal },
            { 5, cost.gems },
            { 6, cost.gold },
        } };

        std::vector<CostPill> pills;
        pills.reserve( 7 );
        for ( const auto & [iconIdx, value] : entries ) {
            if ( value <= 0 ) {
                continue;
            }
            std::string amtStr = ( value >= 10000 ) ? ( std::to_string( value / 1000 ) + "K" ) : std::to_string( value );
            const fheroes2::Text txt( amtStr, fheroes2::FontType::smallWhite() );
            const int32_t tw = txt.width();
            pills.push_back( { iconIdx, std::move( amtStr ), tw, 9 + 1 + tw } );
        }

        if ( pills.empty() ) {
            return;
        }

        const int32_t maxRowW = kCardArtW3DS - 2;
        std::vector<std::vector<CostPill>> rows;
        rows.emplace_back();
        int32_t curRowW = 0;

        for ( const auto & pill : pills ) {
            const int32_t nextW = ( rows.back().empty() ? 0 : curRowW + 3 ) + pill.totalW;
            if ( !rows.back().empty() && nextW > maxRowW ) {
                rows.emplace_back();
                curRowW = 0;
            }
            if ( !rows.back().empty() ) {
                curRowW += 3;
            }
            curRowW += pill.totalW;
            rows.back().push_back( pill );
        }

        const int32_t rowH = 10;
        const int32_t overlayH = static_cast<int32_t>( rows.size() ) * rowH + 2;
        const int32_t overlayY = std::max<int32_t>( 1, artBottomY - overlayH );

        // Subtle dark translucent band over building art so costs are always legible.
        fheroes2::ApplyAlpha( cardImg, 1, overlayY, cardImg, 1, overlayY, kCardArtW3DS, artBottomY - overlayY, 105 );

        int32_t py = overlayY + 1;
        for ( const auto & row : rows ) {
            int32_t rowW = 0;
            for ( size_t i = 0; i < row.size(); ++i ) {
                rowW += row[i].totalW + ( i + 1 < row.size() ? 3 : 0 );
            }
            int32_t px = 1 + std::max<int32_t>( 0, ( kCardArtW3DS - rowW ) / 2 );
            for ( size_t i = 0; i < row.size(); ++i ) {
                const auto & p = row[i];
                fheroes2::Blit( miniIcons[p.iconIdx], cardImg, px, py );
                const fheroes2::Text txt( p.amount, fheroes2::FontType::smallWhite() );
                txt.draw( px + 10, py + 1, cardImg );
                px += p.totalW + ( i + 1 < row.size() ? 3 : 0 );
            }
            py += rowH;
        }
    }

    fheroes2::Image renderBuildCard3DS( const BuildCard3DS & card, const Castle & castle, const std::array<fheroes2::Image, 7> & miniIcons, const bool isSelected )
    {
        fheroes2::Image cardImg( kCardWidth3DS, kCardHeight3DS );
        cardImg.reset();

        const uint8_t darkBg = fheroes2::GetColorId( 0x20, 0x12, 0x08 );
        fheroes2::Fill( cardImg, 0, 0, kCardWidth3DS, kCardHeight3DS, darkBg );

        const bool isEvil = Settings::Get().isEvilInterfaceEnabled();

        // Building artwork
        if ( card.baseType == BUILD_CAPTAIN ) {
            tileStoneBackground3DS( cardImg, { 1, 1, kCardArtW3DS, kCardArtH3DS }, isEvil );
            const fheroes2::Sprite & capSpr = Assets::getImage( ICN::getCaptainIcnId( castle.GetRace() ), card.isBuilt ? 1 : 0 );
            if ( !capSpr.empty() && capSpr.height() > 0 ) {
                const int32_t dstW = std::clamp<int32_t>( capSpr.width() * kCardArtH3DS / capSpr.height(), 1, kCardArtW3DS );
                fheroes2::Image scaled( dstW, kCardArtH3DS );
                scaled.reset();
                fheroes2::Resize( capSpr, scaled );
                fheroes2::Blit( scaled, cardImg, 1 + ( kCardArtW3DS - dstW ) / 2, 1 );
            }
        }
        else {
            const fheroes2::Sprite & bldSpr
                = Assets::getImage( ICN::getBuildingIcnId( castle.GetRace() ), fheroes2::getIndexBuildingSprite( static_cast<BuildingType>( card.info.getBuilding() ) ) );
            if ( !bldSpr.empty() ) {
                fheroes2::Image scaled( kCardArtW3DS, kCardArtH3DS );
                scaled.reset();
                fheroes2::Resize( bldSpr, scaled );
                fheroes2::Copy( scaled, 0, 0, cardImg, 1, 1, kCardArtW3DS, kCardArtH3DS );
            }
            else {
                tileStoneBackground3DS( cardImg, { 1, 1, kCardArtW3DS, kCardArtH3DS }, isEvil );
            }
        }

        // Cost overlay on artwork (omitted when fully built)
        if ( !card.isBuilt ) {
            drawCardCostOverlay3DS( cardImg, PaymentConditions::BuyBuilding( castle.GetRace(), card.info.getBuilding() ), miniIcons, 1 + kCardArtH3DS );
        }

        // Top-right status icon: Checkmark (built), Upgrade chevron (upgradeable), or X (not built)
        const int32_t rightEdge = kCardWidth3DS - 3;
        if ( card.isBuilt ) {
            const fheroes2::Sprite & check = Assets::getImage( ICN::TOWNWIND, 11 );
            if ( !check.empty() ) {
                const int32_t bx = rightEdge - check.width();
                const int32_t by = 3;
                fheroes2::ApplyAlpha( cardImg, std::max<int32_t>( 1, bx - 1 ), by - 1, cardImg, std::max<int32_t>( 1, bx - 1 ), by - 1, check.width() + 2,
                                      check.height() + 2, 120 );
                fheroes2::Blit( check, cardImg, bx, by );
            }
        }
        else if ( card.isUpgradeable ) {
            drawUpgradeBadge3DS( cardImg, rightEdge - 12, 3, card.info.getStatus() );
        }
        else {
            const fheroes2::Sprite & cross = Assets::getImage( ICN::TOWNWIND, 12 );
            if ( !cross.empty() ) {
                const int32_t bx = rightEdge - cross.width();
                const int32_t by = 3;
                fheroes2::ApplyAlpha( cardImg, std::max<int32_t>( 1, bx - 1 ), by - 1, cardImg, std::max<int32_t>( 1, bx - 1 ), by - 1, cross.width() + 2,
                                      cross.height() + 2, 120 );
                fheroes2::Blit( cross, cardImg, bx, by );
            }
        }

        // Bottom nameplate: Green if buildable now, Warm bronze if already built, Red if unavailable
        const int32_t plateY = 1 + kCardArtH3DS;
        if ( card.info.getStatus() == BuildingStatus::ALLOW_BUILD ) {
            const fheroes2::Sprite & greenPlate = Assets::getImage( ICN::CASLXTRA, 1 );
            if ( !greenPlate.empty() ) {
                fheroes2::Image plate( kCardArtW3DS, kCardPlateH3DS );
                plate.reset();
                fheroes2::Resize( greenPlate, plate );
                fheroes2::Copy( plate, 0, 0, cardImg, 1, plateY, kCardArtW3DS, kCardPlateH3DS );
            }
            else {
                fheroes2::Fill( cardImg, 1, plateY, kCardArtW3DS, kCardPlateH3DS, fheroes2::GetColorId( 0x20, 0x70, 0x20 ) );
            }
        }
        else if ( card.isBuilt ) {
            const fheroes2::Sprite & bldXtra = Assets::getImage( ICN::BLDGXTRA, 0 );
            if ( bldXtra.width() >= 131 && bldXtra.height() >= 68 ) {
                const int32_t cropH = std::min<int32_t>( 11, bldXtra.height() - 58 );
                const fheroes2::Sprite tanCrop = fheroes2::Crop( bldXtra, 6, 58, 123, cropH );
                fheroes2::Image plate( kCardArtW3DS, kCardPlateH3DS );
                plate.reset();
                fheroes2::Resize( tanCrop, plate );
                fheroes2::Copy( plate, 0, 0, cardImg, 1, plateY, kCardArtW3DS, kCardPlateH3DS );
            }
            else {
                fheroes2::Fill( cardImg, 1, plateY, kCardArtW3DS, kCardPlateH3DS, fheroes2::GetColorId( 0x8C, 0x6C, 0x3C ) );
            }
        }
        else {
            const fheroes2::Sprite & redPlate = Assets::getImage( ICN::CASLXTRA, 2 );
            if ( !redPlate.empty() ) {
                fheroes2::Image plate( kCardArtW3DS, kCardPlateH3DS );
                plate.reset();
                fheroes2::Resize( redPlate, plate );
                fheroes2::Copy( plate, 0, 0, cardImg, 1, plateY, kCardArtW3DS, kCardPlateH3DS );
            }
            else {
                fheroes2::Fill( cardImg, 1, plateY, kCardArtW3DS, kCardPlateH3DS, fheroes2::GetColorId( 0x78, 0x18, 0x18 ) );
            }
        }

        // Building title centered on nameplate
        std::string label = card.info.GetName();
        StringReplace( label, ", Level ", " Lv." );
        fheroes2::Text nameText( label, fheroes2::FontType::smallWhite() );
        if ( nameText.width() > kCardArtW3DS - 4 ) {
            nameText.fitToOneRow( kCardArtW3DS - 4 );
        }
        const int32_t nameX = 1 + ( kCardArtW3DS - nameText.width() ) / 2;
        const int32_t nameY = plateY + ( kCardPlateH3DS - nameText.height() ) / 2 + 1;
        nameText.draw( nameX, nameY, cardImg );

        // Card outer border (highlighted gold frame when selected)
        if ( isSelected ) {
            const uint8_t brightGold = fheroes2::GetColorId( 0xFF, 0xEA, 0x40 );
            const uint8_t warmGold = fheroes2::GetColorId( 0xC8, 0x90, 0x18 );
            const uint8_t glint = fheroes2::GetColorId( 0xFF, 0xFC, 0xB8 );
            fheroes2::DrawRect( cardImg, { 0, 0, kCardWidth3DS, kCardHeight3DS }, brightGold );
            fheroes2::DrawRect( cardImg, { 1, 1, kCardWidth3DS - 2, kCardHeight3DS - 2 }, warmGold );
            fheroes2::Fill( cardImg, 0, 0, 3, 3, glint );
            fheroes2::Fill( cardImg, kCardWidth3DS - 3, 0, 3, 3, glint );
            fheroes2::Fill( cardImg, 0, kCardHeight3DS - 3, 3, 3, glint );
            fheroes2::Fill( cardImg, kCardWidth3DS - 3, kCardHeight3DS - 3, 3, 3, glint );
        }
        else {
            const uint8_t darkFrame = fheroes2::GetColorId( 0x1A, 0x10, 0x08 );
            const uint8_t subtleTrim = fheroes2::GetColorId( 0x80, 0x60, 0x34 );
            fheroes2::DrawRect( cardImg, { 0, 0, kCardWidth3DS, kCardHeight3DS }, darkFrame );
            fheroes2::DrawLine( cardImg, { 1, 1 }, { kCardWidth3DS - 2, 1 }, subtleTrim );
            fheroes2::DrawLine( cardImg, { 1, 1 }, { 1, kCardHeight3DS - 2 }, subtleTrim );
        }

        return cardImg;
    }

    void drawScrollFade3DS( fheroes2::Image & topBuf, const bool fadeTop, const bool fadeBottom )
    {
        // Progressive dark gradient bands at top/bottom edges so scrolled-out rows peek visibly.
        static constexpr std::array<uint8_t, 6> alphas = { 65, 98, 132, 166, 198, 228 };
        const int32_t stripH = 4;

        if ( fadeTop ) {
            for ( size_t i = 0; i < alphas.size(); ++i ) {
                const int32_t y = static_cast<int32_t>( i ) * stripH;
                fheroes2::ApplyAlpha( topBuf, 0, y, topBuf, 0, y, kTopWidth3DS, stripH, alphas[i] );
            }
        }

        if ( fadeBottom ) {
            for ( size_t i = 0; i < alphas.size(); ++i ) {
                const int32_t y = kTopHeight3DS - stripH * static_cast<int32_t>( i + 1 );
                fheroes2::ApplyAlpha( topBuf, 0, y, topBuf, 0, y, kTopWidth3DS, stripH, alphas[i] );
            }
        }
    }

    void drawBottomStatusBanner3DS( fheroes2::Image & dst, const fheroes2::Rect & area, const std::string & message )
    {
        const uint8_t bg = fheroes2::GetColorId( 0x24, 0x14, 0x0A );
        const uint8_t gold = fheroes2::GetColorId( 0xC8, 0x9C, 0x38 );
        const uint8_t goldHi = fheroes2::GetColorId( 0xF0, 0xD4, 0x60 );
        const uint8_t dark = fheroes2::GetColorId( 0x10, 0x08, 0x04 );

        fheroes2::Fill( dst, area.x, area.y, area.width, area.height, bg );
        fheroes2::DrawRect( dst, area, gold );
        fheroes2::DrawRect( dst, { area.x + 1, area.y + 1, area.width - 2, area.height - 2 }, dark );
        fheroes2::Fill( dst, area.x, area.y, 2, 2, goldHi );
        fheroes2::Fill( dst, area.x + area.width - 2, area.y, 2, 2, goldHi );
        fheroes2::Fill( dst, area.x, area.y + area.height - 2, 2, 2, goldHi );
        fheroes2::Fill( dst, area.x + area.width - 2, area.y + area.height - 2, 2, 2, goldHi );

        if ( message.empty() ) {
            return;
        }

        const int32_t maxW = area.width - 14;
        fheroes2::Text statusText( message, fheroes2::FontType::normalYellow() );
        if ( statusText.rows( maxW ) > 2 ) {
            statusText.set( message, fheroes2::FontType::smallWhite() );
        }
        const int32_t textH = statusText.height( maxW );
        const int32_t textY = area.y + std::max<int32_t>( 2, ( area.height - textH ) / 2 );
        statusText.draw( area.x + 7, textY, maxW, dst );
    }
}
#endif

int Castle::DialogBuyHero( const Heroes * hero ) const
{
    if ( !hero ) {
        return Dialog::CANCEL;
    }

    fheroes2::Display & display = fheroes2::Display::instance();

    // setup cursor
    const CursorRestorer cursorRestorer( true, Cursor::POINTER );

    const int32_t spacer = 10;
    const fheroes2::Sprite & portrait_frame = Assets::getImage( ICN::SURRENDR, 4 );

    const fheroes2::Text recruitHeroText( _( "Recruit Hero" ), fheroes2::FontType::normalYellow() );

    uint32_t count = hero->GetCountArtifacts();
    if ( hero->hasArtifact( Artifact::MAGIC_BOOK ) ) {
        --count;
    }

    std::string str = _( "%{name} is a level %{value} %{race} " );

    if ( count ) {
        str += count > 1 ? _( "with %{count} artifacts." ) : _( "with 1 artifact." );
    }
    else {
        str += _( "without artifacts." );
    }

    StringReplace( str, "%{name}", hero->GetName() );
    StringReplace( str, "%{value}", hero->GetLevel() );
    StringReplace( str, "%{race}", Race::String( hero->GetRace() ) );
    StringReplace( str, "%{count}", count );

    const fheroes2::Text heroDescriptionText( std::move( str ), fheroes2::FontType::normalWhite() );

    Resource::BoxSprite rbs( PaymentConditions::RecruitHero(), fheroes2::boxAreaWidthPx );

    const int32_t dialogHeight = recruitHeroText.height( fheroes2::boxAreaWidthPx ) + spacer + portrait_frame.height() + spacer
                                 + heroDescriptionText.height( fheroes2::boxAreaWidthPx ) + spacer + rbs.GetArea().height;

    const Dialog::FrameBox box( dialogHeight, true );
    const fheroes2::Rect & dialogRoi = box.GetArea();

    recruitHeroText.draw( dialogRoi.x, dialogRoi.y + 2, fheroes2::boxAreaWidthPx, display );

    // portrait and frame
    fheroes2::Point pos{ dialogRoi.x + ( dialogRoi.width - portrait_frame.width() ) / 2, dialogRoi.y + recruitHeroText.height( fheroes2::boxAreaWidthPx ) + spacer };
    fheroes2::Blit( portrait_frame, display, pos.x, pos.y );

    const fheroes2::Rect heroPortraitArea( pos.x, pos.y, portrait_frame.width(), portrait_frame.height() );
    pos.x = pos.x + 5;
    pos.y = pos.y + 6;
    hero->PortraitRedraw( pos.x, pos.y, PORT_BIG, display );

    pos.y += portrait_frame.height() + spacer;
    heroDescriptionText.draw( dialogRoi.x, pos.y + 2, fheroes2::boxAreaWidthPx, display );

    rbs.SetPos( dialogRoi.x, pos.y + heroDescriptionText.height( fheroes2::boxAreaWidthPx ) + spacer );
    rbs.Redraw();

    fheroes2::ButtonGroup buttonGroup( dialogRoi, Dialog::OK | Dialog::CANCEL );
    fheroes2::ButtonBase & buttonOkay = buttonGroup.button( 0 );
    const fheroes2::ButtonBase & buttonCancel = buttonGroup.button( 1 );

    if ( !AllowBuyHero() ) {
        buttonOkay.disable();
    }

    buttonGroup.draw();
    display.render();

    LocalEvent & le = LocalEvent::Get();
    while ( le.HandleEvents() ) {
        const int result = buttonGroup.processEvents();
        if ( result != Dialog::ZERO ) {
            return result;
        }

        if ( le.isMouseRightButtonPressedInArea( heroPortraitArea ) ) {
            Dialog::QuickInfo( *hero );
        }
        else if ( le.isMouseRightButtonPressedInArea( buttonOkay.area() ) ) {
            std::string recruitHero = _( "Recruit %{name} the %{race}" );
            StringReplace( recruitHero, "%{name}", hero->GetName() );
            StringReplace( recruitHero, "%{race}", Race::String( hero->GetRace() ) );
            recruitHero += '.';
            fheroes2::showStandardTextMessage( _( "Okay" ), std::move( recruitHero ), Dialog::ZERO );
        }
        else if ( le.isMouseRightButtonPressedInArea( buttonCancel.area() ) ) {
            fheroes2::showStandardTextMessage( _( "Cancel" ), _( "Exit this menu without doing anything." ), Dialog::ZERO );
        }
    }

    return Dialog::CANCEL;
}

int Castle::DialogBuyCastle( const bool hasButtons /* = true */ ) const
{
    const BuildingInfo info( *this, BUILD_CASTLE );
    return info.DialogBuyBuilding( hasButtons ) ? Dialog::OK : Dialog::CANCEL;
}

Castle::ConstructionDialogResult Castle::_openConstructionDialog( uint32_t & dwellingTobuild )
{
    if ( !isBuild( BUILD_CASTLE ) ) {
        // It is not possible to open this dialog without a built castle!
        assert( 0 );
        return ConstructionDialogResult::DoNothing;
    }

    dwellingTobuild = BUILD_NOTHING;

#if defined( TARGET_NINTENDO_3DS )
    {
        const Settings & conf = Settings::Get();
        const bool isEvil = conf.isEvilInterfaceEnabled();
        const bool isSkipTavernInteraction = ( Race::NECR == _race ) && ( conf.getCurrentMapInfo().version == GameVersion::SUCCESSION_WARS );

        fheroes2::Display & display = fheroes2::Display::instance();
        const fheroes2::ImageRestorer screenRestorer( display, 0, 0, display.width(), display.height() );
        const fheroes2::Scope3DSCastleBuildView buildViewScope;

        BuildingType mageLevel = BUILD_NOTHING;
        switch ( GetLevelMageGuild() ) {
        case 0:
            mageLevel = BUILD_MAGEGUILD1;
            break;
        case 1:
            mageLevel = BUILD_MAGEGUILD2;
            break;
        case 2:
            mageLevel = BUILD_MAGEGUILD3;
            break;
        case 3:
            mageLevel = BUILD_MAGEGUILD4;
            break;
        default:
            mageLevel = BUILD_MAGEGUILD5;
            break;
        }

        struct GroupDef final
        {
            std::string title;
            std::vector<BuildingType> types;
        };

        std::vector<GroupDef> groups;
        {
            GroupDef common;
            common.title = _( "Common Buildings" );
            common.types.push_back( mageLevel );
            if ( !isSkipTavernInteraction ) {
                common.types.push_back( BUILD_TAVERN );
            }
            common.types.push_back( BUILD_THIEVESGUILD );
            common.types.push_back( BUILD_SHIPYARD );
            common.types.push_back( BUILD_STATUE );
            common.types.push_back( BUILD_MARKETPLACE );
            common.types.push_back( BUILD_WELL );
            common.types.push_back( BUILD_LEFTTURRET );
            common.types.push_back( BUILD_RIGHTTURRET );
            common.types.push_back( BUILD_MOAT );
            groups.push_back( std::move( common ) );

            GroupDef unique;
            unique.title = _( "Unique Buildings" );
            unique.types.push_back( BUILD_CAPTAIN );
            unique.types.push_back( BUILD_WEL2 );
            unique.types.push_back( BUILD_SPEC );
            groups.push_back( std::move( unique ) );

            GroupDef dwellings;
            dwellings.title = _( "Creature Dwellings" );
            dwellings.types = { DWELLING_MONSTER1, DWELLING_MONSTER2, DWELLING_MONSTER3, DWELLING_MONSTER4, DWELLING_MONSTER5, DWELLING_MONSTER6 };
            groups.push_back( std::move( dwellings ) );
        }

        std::vector<BuildCard3DS> cards;
        std::vector<HeaderEntry3DS> headers;
        std::vector<std::vector<size_t>> rowCardIndices;
        cards.reserve( 20 );

        int32_t layoutY = 6;
        int32_t currentGridRow = 0;

        for ( size_t g = 0; g < groups.size(); ++g ) {
            if ( g > 0 ) {
                layoutY += 6;
            }
            headers.push_back( { layoutY, groups[g].title } );
            layoutY += kHeaderHeight3DS + 2;

            const auto & types = groups[g].types;
            for ( size_t i = 0; i < types.size(); i += kGridCols3DS ) {
                std::vector<size_t> rowIndices;
                const size_t rowEnd = std::min<size_t>( types.size(), i + kGridCols3DS );
                for ( size_t j = i; j < rowEnd; ++j ) {
                    const int32_t col = static_cast<int32_t>( j - i );
                    const int32_t cx = kGridMarginX3DS + col * ( kCardWidth3DS + kGridGapX3DS );
                    cards.emplace_back( *this, types[j] );
                    BuildCard3DS & card = cards.back();
                    card.rect = { cx, layoutY, kCardWidth3DS, kCardHeight3DS };
                    card.gridRow = currentGridRow;
                    card.gridCol = col;
                    rowIndices.push_back( cards.size() - 1 );
                }
                rowCardIndices.push_back( std::move( rowIndices ) );
                ++currentGridRow;
                layoutY += kCardHeight3DS + kGridGapY3DS;
            }
            // Trim trailing gap after last row of a group
            layoutY -= kGridGapY3DS;
        }

        const int32_t contentHeight = std::max<int32_t>( kTopHeight3DS, layoutY + 8 );
        const int32_t maxScrollY = std::max<int32_t>( 0, contentHeight - kTopHeight3DS );
        const std::array<fheroes2::Image, 7> miniIcons = createMiniResourceIcons3DS();

        fheroes2::Image contentBuffer( kTopWidth3DS, contentHeight );
        contentBuffer.reset();

        size_t selectedIndex = 0;
        int32_t curScrollY = 0;
        int32_t targetScrollY = 0;

        auto updateTargetScroll = [&]() {
            if ( cards.empty() ) {
                return;
            }
            const BuildCard3DS & sel = cards[selectedIndex];
            if ( sel.gridRow == 0 ) {
                targetScrollY = 0;
                return;
            }
            if ( sel.gridRow + 1 >= static_cast<int32_t>( rowCardIndices.size() ) ) {
                targetScrollY = maxScrollY;
                return;
            }
            if ( sel.rect.y < targetScrollY + 24 ) {
                targetScrollY = std::max<int32_t>( 0, sel.rect.y - 24 );
            }
            else if ( sel.rect.y + sel.rect.height > targetScrollY + ( kTopHeight3DS - 24 ) ) {
                targetScrollY = std::min<int32_t>( maxScrollY, sel.rect.y + sel.rect.height - ( kTopHeight3DS - 24 ) );
            }
        };

        auto rebuildContentBuffer = [&]() {
            tileStoneBackground3DS( contentBuffer, { 0, 0, kTopWidth3DS, contentHeight }, isEvil );
            for ( const auto & hdr : headers ) {
                drawGroupHeader3DS( contentBuffer, hdr.y, hdr.title );
            }
            for ( size_t i = 0; i < cards.size(); ++i ) {
                const fheroes2::Image cardImg = renderBuildCard3DS( cards[i], *this, miniIcons, i == selectedIndex );
                fheroes2::Copy( cardImg, 0, 0, contentBuffer, cards[i].rect.x, cards[i].rect.y, cardImg.width(), cardImg.height() );
            }
        };

        auto renderTopView = [&]() {
            fheroes2::Copy( contentBuffer, 0, curScrollY, display, 0, 0, kTopWidth3DS, kTopHeight3DS );
            fheroes2::Image topView = fheroes2::Crop( display, 0, 0, kTopWidth3DS, kTopHeight3DS );
            drawScrollFade3DS( topView, curScrollY > 0, curScrollY < maxScrollY );
            fheroes2::Copy( topView, 0, 0, display, 0, 0, kTopWidth3DS, kTopHeight3DS );
        };

        Kingdom & kingdom = GetKingdom();
        Heroes * hero1 = kingdom.GetRecruits().GetHero1();
        Heroes * hero2 = kingdom.GetRecruits().GetHero2();

        std::string notAllow1Msg;
        std::string notAllow2Msg;
        const bool allowBuyHero1 = hero1 ? AllowBuyHero( &notAllow1Msg ) : false;
        const bool allowBuyHero2 = hero2 ? AllowBuyHero( &notAllow2Msg ) : false;

        // Pre-render the miniature Castle visual onto offscreen 160x148 region then scale to 116x104.
        fheroes2::Image castleThumb( 116, 104 );
        castleThumb.reset();
        {
            tileStoneBackground3DS( display, { 450, 10, 160, 148 }, isEvil );
            DrawImageCastle( { 450, 10 } );
            const fheroes2::Sprite rawCastle = fheroes2::Crop( display, 450, 10, 160, 148 );
            fheroes2::Resize( rawCastle, castleThumb );
        }

        const fheroes2::Rect rectHero1( 158, 268, 50, 47 );
        const fheroes2::Rect rectHero2( 230, 268, 50, 47 );
        const fheroes2::Rect rectBanner( 14, 378, 292, 46 );

        fheroes2::Rect footerExitLocalRect;
        fheroes2::Rect exitDisplayRect;
        fheroes2::Rect resDisplayRect;
        bool exitPressed = false;

        auto drawHeroSlot = [&]( Heroes * hero, const fheroes2::Rect & area, const bool allowBuy ) {
            const uint8_t goldFrame = fheroes2::GetColorId( 0xC0, 0x94, 0x34 );
            fheroes2::DrawRect( display, { area.x - 1, area.y - 1, area.width + 2, area.height + 2 }, goldFrame );
            if ( hero ) {
                hero->PortraitRedraw( area.x, area.y, PORT_MEDIUM, display );
                fheroes2::Text hName( hero->GetName(), fheroes2::FontType::smallWhite() );
                if ( hName.width() > 64 ) {
                    hName.fitToOneRow( 64 );
                }
                hName.draw( area.x + ( area.width - hName.width() ) / 2, area.y + area.height + 2, display );
            }
            else {
                const fheroes2::Sprite & emptySpr = Assets::getImage( ICN::STRIP, 3 );
                if ( !emptySpr.empty() ) {
                    fheroes2::Image scaled( area.width, area.height );
                    scaled.reset();
                    fheroes2::Resize( emptySpr, scaled );
                    fheroes2::Copy( scaled, 0, 0, display, area.x, area.y, area.width, area.height );
                }
            }
            if ( !allowBuy ) {
                const fheroes2::Sprite & denySpr = Assets::getImage( ICN::TOWNWIND, 12 );
                if ( !denySpr.empty() ) {
                    fheroes2::Blit( denySpr, display, area.x + area.width - denySpr.width() - 2, area.y + area.height - denySpr.height() - 2 );
                }
            }
        };

        auto renderBottomFooter = [&]() {
            const fheroes2::Image footerImg = fheroes2::render3DSCastleResourceFooter( kingdom.GetFunds(), exitPressed, footerExitLocalRect );
            fheroes2::Copy( footerImg, 0, 0, display, 0, 432, footerImg.width(), footerImg.height() );
            exitDisplayRect = { footerExitLocalRect.x, 432 + footerExitLocalRect.y, footerExitLocalRect.width, footerExitLocalRect.height };
            resDisplayRect = { 0, 432, footerExitLocalRect.x, footerImg.height() };
        };

        auto renderBottomView = [&]() {
            fheroes2::draw3DSPanel( { kBottomX3DS, kBottomY3DS, kBottomWidth3DS, kBottomHeight3DS } );

            // Left column: Castle name + miniature town preview
            fheroes2::Text castleTitle( GetName(), fheroes2::FontType::normalYellow() );
            if ( castleTitle.width() > 118 ) {
                castleTitle.fitToOneRow( 118 );
            }
            castleTitle.draw( 16 + ( 116 - castleTitle.width() ) / 2, 250, display );

            fheroes2::DrawRect( display, { 15, 267, 118, 106 }, fheroes2::GetColorId( 0xC0, 0x94, 0x34 ) );
            fheroes2::Copy( castleThumb, 0, 0, display, 16, 268, castleThumb.width(), castleThumb.height() );

            // Right column: Hero recruitment + selected building description
            const fheroes2::Text recruitHdr( _( "Recruit Heroes" ), fheroes2::FontType::smallYellow() );
            recruitHdr.draw( 142 + ( 162 - recruitHdr.width() ) / 2, 252, display );

            drawHeroSlot( hero1, rectHero1, allowBuyHero1 );
            drawHeroSlot( hero2, rectHero2, allowBuyHero2 );

            if ( !cards.empty() ) {
                const BuildCard3DS & sel = cards[selectedIndex];
                std::string selTitle = sel.info.GetName();
                fheroes2::Text selTitleTxt( selTitle, fheroes2::FontType::smallYellow() );
                if ( selTitleTxt.width() > 160 ) {
                    selTitleTxt.fitToOneRow( 160 );
                }
                selTitleTxt.draw( 142 + ( 162 - selTitleTxt.width() ) / 2, 329, display );

                std::string descStr;
                if ( sel.info.getStatus() == BuildingStatus::REQUIRES_BUILD ) {
                    descStr = fheroes2::getBuildingRequirementString( _race, static_cast<BuildingType>( sel.info.getBuilding() ) );
                }
                if ( descStr.empty() ) {
                    descStr = sel.info.getDescription();
                }
                const fheroes2::Text descTxt( descStr, fheroes2::FontType::smallWhite() );
                descTxt.drawInRoi( 142, 341, 162, display, { 142, 341, 162, 34 } );

                std::string statusMsg = sel.info.getConditionText();
                if ( sel.info.getStatus() == BuildingStatus::REQUIRES_BUILD ) {
                    const std::string req = fheroes2::getBuildingRequirementString( _race, static_cast<BuildingType>( sel.info.getBuilding() ) );
                    if ( !req.empty() ) {
                        statusMsg += " (" + req + ')';
                    }
                }
                drawBottomStatusBanner3DS( display, rectBanner, statusMsg );
            }

            renderBottomFooter();
        };

        auto openModal = [&]( const std::function<bool()> & modalFn ) -> bool {
            tileStoneBackground3DS( display, { 0, 0, display.width(), display.height() }, isEvil );
            const fheroes2::Scope3DSCastleView modalView3DS( true );
            const CursorRestorer modalCursor( true, Cursor::POINTER );
            const bool result = modalFn();
            LocalEvent::Get().reset();
            return result;
        };

        rebuildContentBuffer();
        renderTopView();
        renderBottomView();
        display.render();

        LocalEvent & le = LocalEvent::Get();
        const bool canSwitchCastles = ( kingdom.GetCastles().size() > 1 );

        while ( le.HandleEvents() ) {
            bool selectionChanged = false;
            bool needFooterRedraw = false;

            if ( Game::HotKeyCloseWindow() || le.isKeyPressed( fheroes2::Key::KEY_ESCAPE ) ) {
                return ConstructionDialogResult::DoNothing;
            }

            if ( canSwitchCastles ) {
                if ( le.isKeyPressed( fheroes2::Key::KEY_PAGE_UP ) ) {
                    return ConstructionDialogResult::PrevConstructionWindow;
                }
                if ( le.isKeyPressed( fheroes2::Key::KEY_PAGE_DOWN ) ) {
                    return ConstructionDialogResult::NextConstructionWindow;
                }
            }

            if ( !cards.empty() ) {
                if ( le.isKeyPressed( fheroes2::Key::KEY_RIGHT ) || Game::HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_RIGHT ) ) {
                    selectedIndex = ( selectedIndex + 1 ) % cards.size();
                    selectionChanged = true;
                }
                else if ( le.isKeyPressed( fheroes2::Key::KEY_LEFT ) || Game::HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_LEFT ) ) {
                    selectedIndex = ( selectedIndex + cards.size() - 1 ) % cards.size();
                    selectionChanged = true;
                }
                else if ( le.isKeyPressed( fheroes2::Key::KEY_DOWN ) || Game::HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_DOWN ) ) {
                    const int32_t curRow = cards[selectedIndex].gridRow;
                    const int32_t curCol = cards[selectedIndex].gridCol;
                    const size_t nextRow = static_cast<size_t>( ( curRow + 1 ) % static_cast<int32_t>( rowCardIndices.size() ) );
                    const auto & nextRowCards = rowCardIndices[nextRow];
                    const size_t targetIdx = std::min<size_t>( static_cast<size_t>( curCol ), nextRowCards.size() - 1 );
                    selectedIndex = nextRowCards[targetIdx];
                    selectionChanged = true;
                }
                else if ( le.isKeyPressed( fheroes2::Key::KEY_UP ) || Game::HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_UP ) ) {
                    const int32_t curRow = cards[selectedIndex].gridRow;
                    const int32_t curCol = cards[selectedIndex].gridCol;
                    const size_t numRows = rowCardIndices.size();
                    const size_t prevRow = ( static_cast<size_t>( curRow ) + numRows - 1 ) % numRows;
                    const auto & prevRowCards = rowCardIndices[prevRow];
                    const size_t targetIdx = std::min<size_t>( static_cast<size_t>( curCol ), prevRowCards.size() - 1 );
                    selectedIndex = prevRowCards[targetIdx];
                    selectionChanged = true;
                }
                else if ( le.isKeyPressed( fheroes2::Key::KEY_ENTER ) || Game::HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_OKAY ) ) {
                    BuildCard3DS & sel = cards[selectedIndex];
                    if ( openModal( [&sel]() { return sel.info.DialogBuyBuilding( true ); } ) ) {
                        dwellingTobuild = sel.info.getBuilding();
                        return ConstructionDialogResult::Build;
                    }
                    rebuildContentBuffer();
                    renderTopView();
                    renderBottomView();
                    display.render();
                    continue;
                }
            }

            if ( hero1 && le.MouseClickLeft( rectHero1 ) ) {
                if ( openModal( [this, hero1]() { return Dialog::OK == DialogBuyHero( hero1 ); } ) ) {
                    RecruitHero( hero1 );
                    return ConstructionDialogResult::RecruitHero;
                }
                rebuildContentBuffer();
                renderTopView();
                renderBottomView();
                display.render();
                continue;
            }
            if ( hero2 && le.MouseClickLeft( rectHero2 ) ) {
                if ( openModal( [this, hero2]() { return Dialog::OK == DialogBuyHero( hero2 ); } ) ) {
                    RecruitHero( hero2 );
                    return ConstructionDialogResult::RecruitHero;
                }
                rebuildContentBuffer();
                renderTopView();
                renderBottomView();
                display.render();
                continue;
            }

            if ( le.MouseClickLeft( resDisplayRect ) ) {
                openModal( [&kingdom]() {
                    fheroes2::showKingdomIncome( kingdom, Dialog::OK );
                    return false;
                } );
                rebuildContentBuffer();
                renderTopView();
                renderBottomView();
                display.render();
                continue;
            }

            const bool exitHeld = le.isMouseLeftButtonPressedAndHeldInArea( exitDisplayRect );
            if ( exitHeld != exitPressed ) {
                exitPressed = exitHeld;
                needFooterRedraw = true;
            }
            if ( le.MouseClickLeft( exitDisplayRect ) ) {
                return ConstructionDialogResult::DoNothing;
            }

            if ( selectionChanged ) {
                updateTargetScroll();
                rebuildContentBuffer();
                renderBottomView();
            }

            if ( curScrollY != targetScrollY || selectionChanged ) {
                if ( curScrollY != targetScrollY ) {
                    const int32_t diff = targetScrollY - curScrollY;
                    curScrollY += ( std::abs( diff ) <= 4 ) ? diff : ( diff / 3 + ( diff > 0 ? 1 : -1 ) );
                }
                renderTopView();
                display.render();
            }
            else if ( needFooterRedraw ) {
                renderBottomFooter();
                display.render( { 0, 432, kBottomWidth3DS, 48 } );
            }
        }

        return ConstructionDialogResult::DoNothing;
    }
#endif

    // setup cursor
    const CursorRestorer cursorRestorer( true, Cursor::POINTER );

    fheroes2::Display & display = fheroes2::Display::instance();
    const fheroes2::ImageRestorer restorer( display, ( display.width() - fheroes2::Display::DEFAULT_WIDTH ) / 2,
                                            ( display.height() - fheroes2::Display::DEFAULT_HEIGHT ) / 2, fheroes2::Display::DEFAULT_WIDTH,
                                            fheroes2::Display::DEFAULT_HEIGHT );

    const fheroes2::Point cur_pt( restorer.x(), restorer.y() );
    fheroes2::Point dst_pt( cur_pt );

    const Settings & conf = Settings::Get();
    const bool isEvilInterface = conf.isEvilInterfaceEnabled();

    fheroes2::Blit( Assets::getImage( isEvilInterface ? ICN::CASLWIND_EVIL : ICN::CASLWIND, 0 ), display, dst_pt.x, dst_pt.y );

    // hide captain options
    if ( !( _constructedBuildings & BUILD_CAPTAIN ) ) {
        dst_pt.x = 530;
        dst_pt.y = 163;
        const fheroes2::Rect rect( dst_pt.x, dst_pt.y, 110, 84 );
        dst_pt.x += cur_pt.x;
        dst_pt.y += cur_pt.y;

        const fheroes2::Sprite & backgroundImage = Assets::getImage( isEvilInterface ? ICN::STONEBAK_EVIL : ICN::STONEBAK, 0 );
        fheroes2::Copy( backgroundImage, rect.x, rect.y, display, dst_pt.x, dst_pt.y, rect.width, rect.height );
    }

    // draw castle sprite
    dst_pt.x = cur_pt.x + 459;
    dst_pt.y = cur_pt.y + 6;
    DrawImageCastle( dst_pt );

    // castle name
    const fheroes2::Text castleName( GetName(), fheroes2::FontType::smallWhite() );
    castleName.draw( cur_pt.x + 538 - castleName.width() / 2, cur_pt.y + 2, display );

    dst_pt.y = cur_pt.y + 2;

    BuildingInfo dwelling1( *this, DWELLING_MONSTER1 );
    dwelling1.SetPos( cur_pt.x + 5, dst_pt.y );
    dwelling1.Redraw();

    BuildingInfo dwelling2( *this, DWELLING_MONSTER2 );
    dwelling2.SetPos( cur_pt.x + 149, dst_pt.y );
    dwelling2.Redraw();

    BuildingInfo dwelling3( *this, DWELLING_MONSTER3 );
    dwelling3.SetPos( cur_pt.x + 293, dst_pt.y );
    dwelling3.Redraw();

    dst_pt.y = cur_pt.y + 77;

    BuildingInfo dwelling4( *this, DWELLING_MONSTER4 );
    dwelling4.SetPos( cur_pt.x + 5, dst_pt.y );
    dwelling4.Redraw();

    BuildingInfo dwelling5( *this, DWELLING_MONSTER5 );
    dwelling5.SetPos( cur_pt.x + 149, dst_pt.y );
    dwelling5.Redraw();

    BuildingInfo dwelling6( *this, DWELLING_MONSTER6 );
    dwelling6.SetPos( cur_pt.x + 293, dst_pt.y );
    dwelling6.Redraw();

    // mage guild
    BuildingType level = BUILD_NOTHING;
    switch ( GetLevelMageGuild() ) {
    case 0:
        level = BUILD_MAGEGUILD1;
        break;
    case 1:
        level = BUILD_MAGEGUILD2;
        break;
    case 2:
        level = BUILD_MAGEGUILD3;
        break;
    case 3:
        level = BUILD_MAGEGUILD4;
        break;
    default:
        level = BUILD_MAGEGUILD5;
        break;
    }

    dst_pt.y = cur_pt.y + 157;

    BuildingInfo buildingMageGuild( *this, level );
    buildingMageGuild.SetPos( cur_pt.x + 5, dst_pt.y );
    buildingMageGuild.Redraw();

    // tavern
    const bool isSkipTavernInteraction = ( Race::NECR == _race ) && ( conf.getCurrentMapInfo().version == GameVersion::SUCCESSION_WARS );
    BuildingInfo buildingTavern( *this, BUILD_TAVERN );
    buildingTavern.SetPos( cur_pt.x + 149, dst_pt.y );
    buildingTavern.Redraw();

    // thieves guild
    BuildingInfo buildingThievesGuild( *this, BUILD_THIEVESGUILD );
    buildingThievesGuild.SetPos( cur_pt.x + 293, dst_pt.y );
    buildingThievesGuild.Redraw();

    dst_pt.y = cur_pt.y + 232;

    // shipyard
    BuildingInfo buildingShipyard( *this, BUILD_SHIPYARD );
    buildingShipyard.SetPos( cur_pt.x + 5, dst_pt.y );
    buildingShipyard.Redraw();

    // statue
    BuildingInfo buildingStatue( *this, BUILD_STATUE );
    buildingStatue.SetPos( cur_pt.x + 149, dst_pt.y );
    buildingStatue.Redraw();

    // marketplace
    BuildingInfo buildingMarketplace( *this, BUILD_MARKETPLACE );
    buildingMarketplace.SetPos( cur_pt.x + 293, dst_pt.y );
    buildingMarketplace.Redraw();

    dst_pt.y = cur_pt.y + 307;

    // well
    BuildingInfo buildingWell( *this, BUILD_WELL );
    buildingWell.SetPos( cur_pt.x + 5, dst_pt.y );
    buildingWell.Redraw();

    // wel2
    BuildingInfo buildingWel2( *this, BUILD_WEL2 );
    buildingWel2.SetPos( cur_pt.x + 149, dst_pt.y );
    buildingWel2.Redraw();

    // spec
    BuildingInfo buildingSpec( *this, BUILD_SPEC );
    buildingSpec.SetPos( cur_pt.x + 293, dst_pt.y );
    buildingSpec.Redraw();

    dst_pt.y = cur_pt.y + 387;

    // left turret
    BuildingInfo buildingLTurret( *this, BUILD_LEFTTURRET );
    buildingLTurret.SetPos( cur_pt.x + 5, dst_pt.y );
    buildingLTurret.Redraw();

    // right turret
    BuildingInfo buildingRTurret( *this, BUILD_RIGHTTURRET );
    buildingRTurret.SetPos( cur_pt.x + 149, dst_pt.y );
    buildingRTurret.Redraw();

    // moat
    BuildingInfo buildingMoat( *this, BUILD_MOAT );
    buildingMoat.SetPos( cur_pt.x + 293, dst_pt.y );
    buildingMoat.Redraw();

    // captain
    BuildingInfo buildingCaptain( *this, BUILD_CAPTAIN );
    buildingCaptain.SetPos( cur_pt.x + 444, cur_pt.y + 165 );
    buildingCaptain.Redraw();

    // combat format
    const fheroes2::Sprite & spriteSpreadArmyFormat = Assets::getImage( ICN::HSICONS, 9 );
    const fheroes2::Sprite & spriteGroupedArmyFormat = Assets::getImage( ICN::HSICONS, 10 );
    const fheroes2::Rect rectSpreadArmyFormat( cur_pt.x + 550, cur_pt.y + 220, spriteSpreadArmyFormat.width(), spriteSpreadArmyFormat.height() );
    const fheroes2::Rect rectGroupedArmyFormat( cur_pt.x + 585, cur_pt.y + 220, spriteGroupedArmyFormat.width(), spriteGroupedArmyFormat.height() );
    const std::string descriptionSpreadArmyFormat(
        _( "'Spread' combat formation spreads the castle's units from the top to the bottom of the battlefield, with at least one empty space between each unit." ) );
    const std::string descriptionGroupedArmyFormat(
        _( "'Grouped' combat formation bunches the castle's units together in the center of the castle's side of the battlefield." ) );
    const fheroes2::Point pointSpreadArmyFormat( rectSpreadArmyFormat.x - 1, rectSpreadArmyFormat.y - 1 );
    const fheroes2::Point pointGroupedArmyFormat( rectGroupedArmyFormat.x - 1, rectGroupedArmyFormat.y - 1 );
    const fheroes2::Rect armyFormatRenderRect( pointSpreadArmyFormat, { rectGroupedArmyFormat.x + rectGroupedArmyFormat.width - pointSpreadArmyFormat.x + 1,
                                                                        rectGroupedArmyFormat.y + rectGroupedArmyFormat.height - pointSpreadArmyFormat.y + 1 } );

    fheroes2::MovableSprite cursorFormat( Assets::getImage( ICN::HSICONS, 11 ) );

    if ( isBuild( BUILD_CAPTAIN ) ) {
        const int32_t skillValueOffsetX = 90;
        const int32_t skillOffsetY = 12;

        fheroes2::Text text( Skill::Primary::String( Skill::Primary::ATTACK ), fheroes2::FontType::smallWhite() );
        dst_pt.x = cur_pt.x + 535;
        dst_pt.y = cur_pt.y + 170;
        text.draw( dst_pt.x, dst_pt.y, display );

        text.set( std::to_string( _captain.GetAttack() ), fheroes2::FontType::smallWhite() );
        text.draw( dst_pt.x + skillValueOffsetX, dst_pt.y, display );

        dst_pt.y += skillOffsetY;
        text.set( Skill::Primary::String( Skill::Primary::DEFENSE ), fheroes2::FontType::smallWhite() );
        text.draw( dst_pt.x, dst_pt.y, display );

        text.set( std::to_string( _captain.GetDefense() ), fheroes2::FontType::smallWhite() );
        text.draw( dst_pt.x + skillValueOffsetX, dst_pt.y, display );

        dst_pt.y += skillOffsetY;
        text.set( Skill::Primary::String( Skill::Primary::POWER ), fheroes2::FontType::smallWhite() );
        text.draw( dst_pt.x, dst_pt.y, display );

        text.set( std::to_string( _captain.GetPower() ), fheroes2::FontType::smallWhite() );
        text.draw( dst_pt.x + skillValueOffsetX, dst_pt.y, display );

        dst_pt.y += skillOffsetY;
        text.set( Skill::Primary::String( Skill::Primary::KNOWLEDGE ), fheroes2::FontType::smallWhite() );
        text.draw( dst_pt.x, dst_pt.y, display );

        text.set( std::to_string( _captain.GetKnowledge() ), fheroes2::FontType::smallWhite() );
        text.draw( dst_pt.x + skillValueOffsetX, dst_pt.y, display );

        fheroes2::Copy( spriteSpreadArmyFormat, 0, 0, display, rectSpreadArmyFormat.x, rectSpreadArmyFormat.y, rectSpreadArmyFormat.width, rectSpreadArmyFormat.height );
        fheroes2::Copy( spriteGroupedArmyFormat, 0, 0, display, rectGroupedArmyFormat.x, rectGroupedArmyFormat.y, rectGroupedArmyFormat.width,
                        rectGroupedArmyFormat.height );

        if ( _army.isSpreadFormation() ) {
            cursorFormat.setPosition( pointSpreadArmyFormat.x, pointSpreadArmyFormat.y );
        }
        else {
            cursorFormat.setPosition( pointGroupedArmyFormat.x, pointGroupedArmyFormat.y );
        }
    }

    Kingdom & kingdom = GetKingdom();

    Heroes * hero1 = kingdom.GetRecruits().GetHero1();
    Heroes * hero2 = kingdom.GetRecruits().GetHero2();

    std::string not_allow1_msg;
    std::string not_allow2_msg;
    const bool allow_buy_hero1 = hero1 ? AllowBuyHero( &not_allow1_msg ) : false;
    const bool allow_buy_hero2 = hero2 ? AllowBuyHero( &not_allow2_msg ) : false;

    // first hero
    dst_pt.x = cur_pt.x + 443;
    dst_pt.y = cur_pt.y + 260;
    const fheroes2::Rect rectHero1( dst_pt.x, dst_pt.y, 102, 93 );

    if ( hero1 ) {
        hero1->PortraitRedraw( dst_pt.x, dst_pt.y, PORT_BIG, display );
    }
    else {
        const fheroes2::Sprite & noHeroBackgound = Assets::getImage( ICN::STRIP, 3 );
        fheroes2::Copy( noHeroBackgound, 0, 0, display, rectHero1.x, rectHero1.y, rectHero1.width, rectHero1.height );
    }

    // indicator
    if ( !allow_buy_hero1 ) {
        const fheroes2::Sprite & spriteDeny = Assets::getImage( ICN::TOWNWIND, 12 );
        fheroes2::Blit( spriteDeny, display, dst_pt.x + 102 - 4 + 1 - spriteDeny.width(), dst_pt.y + 93 - 2 - spriteDeny.height() );
    }

    // second hero
    dst_pt.x = cur_pt.x + 443;
    dst_pt.y = cur_pt.y + 363;
    const fheroes2::Rect rectHero2( dst_pt.x, dst_pt.y, 102, 94 );
    if ( hero2 ) {
        hero2->PortraitRedraw( dst_pt.x, dst_pt.y, PORT_BIG, display );
    }
    else {
        const fheroes2::Sprite & noHeroBackgound = Assets::getImage( ICN::STRIP, 3 );
        fheroes2::Copy( noHeroBackgound, 0, 0, display, rectHero2.x, rectHero2.y, rectHero2.width, rectHero2.height );
    }

    // indicator
    if ( !allow_buy_hero2 ) {
        const fheroes2::Sprite & spriteDeny = Assets::getImage( ICN::TOWNWIND, 12 );
        fheroes2::Blit( spriteDeny, display, dst_pt.x + 102 - 4 + 1 - spriteDeny.width(), dst_pt.y + 93 - 2 - spriteDeny.height() );
    }

    dst_pt.y = cur_pt.y + 480 - 19;
    fheroes2::Button buttonPrevCastle( cur_pt.x, dst_pt.y, ICN::SMALLBAR, 1, 2 );
    fheroes2::TimedEventValidator timedButtonPrevCastle( [&buttonPrevCastle]() { return buttonPrevCastle.isPressed(); } );
    buttonPrevCastle.subscribe( &timedButtonPrevCastle );

    // bottom small bar
    const fheroes2::Sprite & bar = Assets::getImage( ICN::SMALLBAR, 0 );
    const int32_t statusBarWidth = bar.width();
    dst_pt.x = cur_pt.x + buttonPrevCastle.area().width;
    fheroes2::Copy( bar, 0, 0, display, dst_pt.x, dst_pt.y, statusBarWidth, bar.height() );

    StatusBar statusBar;
    // Status bar must be smaller due to extra art on both sides.
    statusBar.setRoi( { dst_pt.x + 16, dst_pt.y, statusBarWidth - 16 * 2, 0 } );

    // button next castle
    fheroes2::Button buttonNextCastle( dst_pt.x + statusBarWidth, dst_pt.y, ICN::SMALLBAR, 3, 4 );
    fheroes2::TimedEventValidator timedButtonNextCastle( [&buttonNextCastle]() { return buttonNextCastle.isPressed(); } );
    buttonNextCastle.subscribe( &timedButtonNextCastle );

    // button exit
    dst_pt.x = cur_pt.x + 553;
    dst_pt.y = cur_pt.y + 428;
    fheroes2::Button buttonExit( dst_pt.x, dst_pt.y, ICN::BUTTON_EXIT_TOWN, 0, 1 );

    if ( GetKingdom().GetCastles().size() < 2 ) {
        buttonPrevCastle.disable();
        buttonNextCastle.disable();
    }

    buttonPrevCastle.draw();
    buttonNextCastle.draw();
    buttonExit.draw();

    // redraw resource panel
    const fheroes2::Rect & rectResource = fheroes2::drawResourcePanel( GetKingdom().GetFunds(), display, cur_pt );
    const fheroes2::Rect resActiveArea( rectResource.x, rectResource.y, rectResource.width, buttonExit.area().y - rectResource.y - 3 );

    auto recruitHeroDialog = [this, &buttonExit]( Heroes * hero ) {
        const fheroes2::ButtonRestorer exitRestorer( buttonExit );
        if ( Dialog::OK == DialogBuyHero( hero ) ) {
            RecruitHero( hero );
            return true;
        }
        return false;
    };

    std::string defaultTitle( _( "Castle Options. Month: %{month}, Week: %{week}, Day: %{day}" ) );
    StringReplace( defaultTitle, "%{month}", world.GetMonth() );
    StringReplace( defaultTitle, "%{week}", world.GetWeek() );
    StringReplace( defaultTitle, "%{day}", world.GetDay() );

    LocalEvent & le = LocalEvent::Get();

    auto updateStatusBar = [&]() {
        const bool isCaptainBuilt = isBuild( BUILD_CAPTAIN );

        if ( le.isMouseCursorPosInArea( dwelling1.GetArea() ) ) {
            dwelling1.SetStatusMessage( statusBar );
        }
        else if ( le.isMouseCursorPosInArea( dwelling2.GetArea() ) ) {
            dwelling2.SetStatusMessage( statusBar );
        }
        else if ( le.isMouseCursorPosInArea( dwelling3.GetArea() ) ) {
            dwelling3.SetStatusMessage( statusBar );
        }
        else if ( le.isMouseCursorPosInArea( dwelling4.GetArea() ) ) {
            dwelling4.SetStatusMessage( statusBar );
        }
        else if ( le.isMouseCursorPosInArea( dwelling5.GetArea() ) ) {
            dwelling5.SetStatusMessage( statusBar );
        }
        else if ( le.isMouseCursorPosInArea( dwelling6.GetArea() ) ) {
            dwelling6.SetStatusMessage( statusBar );
        }
        else if ( le.isMouseCursorPosInArea( buildingMageGuild.GetArea() ) ) {
            buildingMageGuild.SetStatusMessage( statusBar );
        }
        else if ( !isSkipTavernInteraction && le.isMouseCursorPosInArea( buildingTavern.GetArea() ) ) {
            buildingTavern.SetStatusMessage( statusBar );
        }
        else if ( le.isMouseCursorPosInArea( buildingThievesGuild.GetArea() ) ) {
            buildingThievesGuild.SetStatusMessage( statusBar );
        }
        else if ( le.isMouseCursorPosInArea( buildingShipyard.GetArea() ) ) {
            buildingShipyard.SetStatusMessage( statusBar );
        }
        else if ( le.isMouseCursorPosInArea( buildingStatue.GetArea() ) ) {
            buildingStatue.SetStatusMessage( statusBar );
        }
        else if ( le.isMouseCursorPosInArea( buildingMarketplace.GetArea() ) ) {
            buildingMarketplace.SetStatusMessage( statusBar );
        }
        else if ( le.isMouseCursorPosInArea( buildingWell.GetArea() ) ) {
            buildingWell.SetStatusMessage( statusBar );
        }
        else if ( le.isMouseCursorPosInArea( buildingWel2.GetArea() ) ) {
            buildingWel2.SetStatusMessage( statusBar );
        }
        else if ( le.isMouseCursorPosInArea( buildingSpec.GetArea() ) ) {
            buildingSpec.SetStatusMessage( statusBar );
        }
        else if ( le.isMouseCursorPosInArea( buildingLTurret.GetArea() ) ) {
            buildingLTurret.SetStatusMessage( statusBar );
        }
        else if ( le.isMouseCursorPosInArea( buildingRTurret.GetArea() ) ) {
            buildingRTurret.SetStatusMessage( statusBar );
        }
        else if ( le.isMouseCursorPosInArea( buildingMoat.GetArea() ) ) {
            buildingMoat.SetStatusMessage( statusBar );
        }
        else if ( le.isMouseCursorPosInArea( buildingCaptain.GetArea() ) ) {
            buildingCaptain.SetStatusMessage( statusBar );
        }
        else if ( hero1 && le.isMouseCursorPosInArea( rectHero1 ) ) {
            if ( !allow_buy_hero1 ) {
                statusBar.ShowMessage( not_allow1_msg );
            }
            else {
                std::string str = _( "Recruit %{name} the %{race}" );
                StringReplace( str, "%{name}", hero1->GetName() );
                StringReplace( str, "%{race}", Race::String( hero1->GetRace() ) );
                statusBar.ShowMessage( str );
            }
        }
        else if ( hero2 && le.isMouseCursorPosInArea( rectHero2 ) ) {
            if ( !allow_buy_hero2 ) {
                statusBar.ShowMessage( not_allow2_msg );
            }
            else {
                std::string str = _( "Recruit %{name} the %{race}" );
                StringReplace( str, "%{name}", hero2->GetName() );
                StringReplace( str, "%{race}", Race::String( hero2->GetRace() ) );
                statusBar.ShowMessage( str );
            }
        }
        else if ( isCaptainBuilt && le.isMouseCursorPosInArea( rectSpreadArmyFormat ) ) {
            statusBar.ShowMessage( _( "Set garrison combat formation to 'Spread'" ) );
        }
        else if ( isCaptainBuilt && le.isMouseCursorPosInArea( rectGroupedArmyFormat ) ) {
            statusBar.ShowMessage( _( "Set garrison combat formation to 'Grouped'" ) );
        }
        else if ( le.isMouseCursorPosInArea( buttonExit.area() ) ) {
            statusBar.ShowMessage( _( "Exit Castle Options" ) );
        }
        else if ( le.isMouseCursorPosInArea( resActiveArea ) ) {
            statusBar.ShowMessage( _( "Show Income" ) );
        }
        else if ( buttonPrevCastle.isEnabled() && le.isMouseCursorPosInArea( buttonPrevCastle.area() ) ) {
            statusBar.ShowMessage( _( "Show previous town" ) );
        }
        else if ( buttonNextCastle.isEnabled() && le.isMouseCursorPosInArea( buttonNextCastle.area() ) ) {
            statusBar.ShowMessage( _( "Show next town" ) );
        }
        else {
            statusBar.ShowMessage( defaultTitle );
        }
    };

    updateStatusBar();

    display.render( restorer.rect() );

    while ( le.HandleEvents() ) {
        buttonExit.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonExit.area() ) );

        if ( le.MouseClickLeft( buttonExit.area() ) || Game::HotKeyCloseWindow() ) {
            break;
        }

        if ( buttonPrevCastle.isEnabled() ) {
            buttonPrevCastle.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonPrevCastle.area() ) );

            if ( le.MouseClickLeft( buttonPrevCastle.area() ) || HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_LEFT ) || timedButtonPrevCastle.isDelayPassed() ) {
                return ConstructionDialogResult::PrevConstructionWindow;
            }
        }
        if ( buttonNextCastle.isEnabled() ) {
            buttonNextCastle.drawOnState( le.isMouseLeftButtonPressedAndHeldInArea( buttonNextCastle.area() ) );

            if ( le.MouseClickLeft( buttonNextCastle.area() ) || HotKeyPressEvent( Game::HotKeyEvent::DEFAULT_RIGHT ) || timedButtonNextCastle.isDelayPassed() ) {
                return ConstructionDialogResult::NextConstructionWindow;
            }
        }

        if ( le.MouseClickLeft( resActiveArea ) ) {
            const fheroes2::ButtonRestorer exitRestorer( buttonExit );
            fheroes2::showKingdomIncome( world.GetKingdom( GetColor() ), Dialog::OK );
        }
        else if ( le.isMouseRightButtonPressedInArea( resActiveArea ) ) {
            fheroes2::showKingdomIncome( world.GetKingdom( GetColor() ), 0 );
        }
        else if ( le.isMouseRightButtonPressedInArea( buttonExit.area() ) ) {
            fheroes2::showStandardTextMessage( _( "Exit" ), _( "Exit this menu." ), Dialog::ZERO );
        }
        else if ( le.isMouseCursorPosInArea( dwelling1.GetArea() ) && dwelling1.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = dwelling1.getBuilding();
            return ConstructionDialogResult::Build;
        }
        if ( le.isMouseCursorPosInArea( dwelling2.GetArea() ) && dwelling2.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = dwelling2.getBuilding();
            return ConstructionDialogResult::Build;
        }
        if ( le.isMouseCursorPosInArea( dwelling3.GetArea() ) && dwelling3.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = dwelling3.getBuilding();
            return ConstructionDialogResult::Build;
        }
        if ( le.isMouseCursorPosInArea( dwelling4.GetArea() ) && dwelling4.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = dwelling4.getBuilding();
            return ConstructionDialogResult::Build;
        }
        if ( le.isMouseCursorPosInArea( dwelling5.GetArea() ) && dwelling5.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = dwelling5.getBuilding();
            return ConstructionDialogResult::Build;
        }
        if ( le.isMouseCursorPosInArea( dwelling6.GetArea() ) && dwelling6.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = dwelling6.getBuilding();
            return ConstructionDialogResult::Build;
        }
        if ( le.isMouseCursorPosInArea( buildingMageGuild.GetArea() ) && buildingMageGuild.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = buildingMageGuild.getBuilding();
            return ConstructionDialogResult::Build;
        }
        if ( !isSkipTavernInteraction && le.isMouseCursorPosInArea( buildingTavern.GetArea() ) && buildingTavern.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = ( Race::NECR == _race ? BUILD_SHRINE : BUILD_TAVERN );
            return ConstructionDialogResult::Build;
        }
        if ( le.isMouseCursorPosInArea( buildingThievesGuild.GetArea() ) && buildingThievesGuild.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = BUILD_THIEVESGUILD;
            return ConstructionDialogResult::Build;
        }
        if ( le.isMouseCursorPosInArea( buildingShipyard.GetArea() ) && buildingShipyard.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = BUILD_SHIPYARD;
            return ConstructionDialogResult::Build;
        }
        if ( le.isMouseCursorPosInArea( buildingStatue.GetArea() ) && buildingStatue.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = BUILD_STATUE;
            return ConstructionDialogResult::Build;
        }
        if ( le.isMouseCursorPosInArea( buildingMarketplace.GetArea() ) && buildingMarketplace.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = BUILD_MARKETPLACE;
            return ConstructionDialogResult::Build;
        }
        if ( le.isMouseCursorPosInArea( buildingWell.GetArea() ) && buildingWell.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = BUILD_WELL;
            return ConstructionDialogResult::Build;
        }
        if ( le.isMouseCursorPosInArea( buildingWel2.GetArea() ) && buildingWel2.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = BUILD_WEL2;
            return ConstructionDialogResult::Build;
        }
        if ( le.isMouseCursorPosInArea( buildingSpec.GetArea() ) && buildingSpec.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = BUILD_SPEC;
            return ConstructionDialogResult::Build;
        }
        if ( le.isMouseCursorPosInArea( buildingLTurret.GetArea() ) && buildingLTurret.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = BUILD_LEFTTURRET;
            return ConstructionDialogResult::Build;
        }
        if ( le.isMouseCursorPosInArea( buildingRTurret.GetArea() ) && buildingRTurret.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = BUILD_RIGHTTURRET;
            return ConstructionDialogResult::Build;
        }
        if ( le.isMouseCursorPosInArea( buildingMoat.GetArea() ) && buildingMoat.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = BUILD_MOAT;
            return ConstructionDialogResult::Build;
        }
        if ( le.isMouseCursorPosInArea( buildingCaptain.GetArea() ) && buildingCaptain.QueueEventProcessing( buttonExit ) ) {
            dwellingTobuild = BUILD_CAPTAIN;
            return ConstructionDialogResult::Build;
        }
        if ( hero1 && le.MouseClickLeft( rectHero1 ) && recruitHeroDialog( hero1 ) ) {
            return ConstructionDialogResult::RecruitHero;
        }
        if ( hero2 && le.MouseClickLeft( rectHero2 ) && recruitHeroDialog( hero2 ) ) {
            return ConstructionDialogResult::RecruitHero;
        }

        const bool isCaptainBuilt = isBuild( BUILD_CAPTAIN );

        if ( isCaptainBuilt ) {
            if ( le.MouseClickLeft( rectSpreadArmyFormat ) && !_army.isSpreadFormation() ) {
                cursorFormat.setPosition( pointSpreadArmyFormat.x, pointSpreadArmyFormat.y );
                display.render( armyFormatRenderRect );
                _army.SetSpreadFormation( true );
            }
            else if ( le.MouseClickLeft( rectGroupedArmyFormat ) && _army.isSpreadFormation() ) {
                cursorFormat.setPosition( pointGroupedArmyFormat.x, pointGroupedArmyFormat.y );
                display.render( armyFormatRenderRect );
                _army.SetSpreadFormation( false );
            }
            else if ( le.isMouseRightButtonPressedInArea( rectSpreadArmyFormat ) ) {
                fheroes2::showStandardTextMessage( _( "Spread Formation" ), descriptionSpreadArmyFormat, Dialog::ZERO );
            }
            else if ( le.isMouseRightButtonPressedInArea( rectGroupedArmyFormat ) ) {
                fheroes2::showStandardTextMessage( _( "Grouped Formation" ), descriptionGroupedArmyFormat, Dialog::ZERO );
            }
        }

        // Right click
        if ( hero1 && le.isMouseRightButtonPressedInArea( rectHero1 ) ) {
            LocalEvent::Get().reset();
            hero1->OpenDialog( true, true, false, false, false, false, fheroes2::getLanguageFromAbbreviation( conf.getGameLanguage() ) );

            // Use half fade if game resolution is not 640x480.
            fheroes2::fadeInDisplay( restorer.rect(), !display.isDefaultSize() );
        }
        else if ( hero2 && le.isMouseRightButtonPressedInArea( rectHero2 ) ) {
            LocalEvent::Get().reset();
            hero2->OpenDialog( true, true, false, false, false, false, fheroes2::getLanguageFromAbbreviation( conf.getGameLanguage() ) );

            // Use half fade if game resolution is not 640x480.
            fheroes2::fadeInDisplay( restorer.rect(), !display.isDefaultSize() );
        }
        else if ( le.isMouseRightButtonPressedInArea( buttonNextCastle.area() ) ) {
            fheroes2::showStandardTextMessage( _( "Show next town" ), _( "Click to show the next town." ), Dialog::ZERO );
        }
        else if ( le.isMouseRightButtonPressedInArea( buttonPrevCastle.area() ) ) {
            fheroes2::showStandardTextMessage( _( "Show previous town" ), _( "Click to show the previous town." ), Dialog::ZERO );
        }

        updateStatusBar();
    }

    return ConstructionDialogResult::DoNothing;
}
