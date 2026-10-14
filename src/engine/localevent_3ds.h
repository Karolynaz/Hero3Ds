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
// Included inside localevent.cpp: native libctru implementation with the same
// event contract as the SDL backend. This file intentionally uses LocalEvent's
// private state through the EventEngine friendship.
#include <limits>
#include <vector>

namespace EventProcessing
{
    class EventEngine
    {
    public:
        static void initEvents() {}
        static void initTouchpad()
        {
            fheroes2::cursor().forceSoftwareEmulation();
        }
        void initController()
        {
            initTouchpad();
        }
        void closeController() {}
        bool isControllerValid() const
        {
            return false;
        }
        static int32_t getCurrentKeyModifiers()
        {
            return 0;
        }
        static void sleep( const uint32_t milliseconds )
        {
            svcSleepThread( static_cast<int64_t>( milliseconds ) * 1000000 );
        }
        static const char * getKeyName( const fheroes2::Key key )
        {
            switch ( key ) {
            case fheroes2::Key::KEY_ESCAPE:
                return "B";
            case fheroes2::Key::KEY_ENTER:
                return "Start";
            case fheroes2::Key::KEY_LEFT:
                return "D-Pad Left";
            case fheroes2::Key::KEY_RIGHT:
                return "D-Pad Right";
            case fheroes2::Key::KEY_UP:
                return "D-Pad Up";
            case fheroes2::Key::KEY_DOWN:
                return "D-Pad Down";
            default:
                return "";
            }
        }

        static const void * nextDialogButton( const std::vector<std::pair<const void *, fheroes2::Rect>> & buttons, const void * focused, const int horizontal,
                                              const int vertical )
        {
            const auto current = std::find_if( buttons.begin(), buttons.end(), [focused]( const auto & button ) { return button.first == focused; } );
            if ( current == buttons.end() ) {
                return buttons.empty() ? nullptr : buttons.front().first;
            }
            const auto center = []( const fheroes2::Rect & area ) { return fheroes2::Point( area.x + area.width / 2, area.y + area.height / 2 ); };
            const auto origin = center( current->second );
            const void * result = focused;
            double bestScore = std::numeric_limits<double>::max();
            for ( const auto & button : buttons ) {
                const auto point = center( button.second );
                const double x = point.x - origin.x;
                const double y = point.y - origin.y;
                const double forward = x * horizontal + y * vertical;
                if ( button.first == focused || forward <= 0.0 ) {
                    continue;
                }
                const double lateral = x * vertical - y * horizontal;
                const double score = x * x + y * y + 4.0 * lateral * lateral;
                if ( score < bestScore ) {
                    bestScore = score;
                    result = button.first;
                }
            }
            return result;
        }

        bool handleEvents( LocalEvent & events, const bool, bool & updateDisplay )
        {
            updateDisplay = false;
            if ( !aptMainLoop() ) {
                return false;
            }
            hidScanInput();
            const uint32_t down = hidKeysDown();
            const uint32_t up = hidKeysUp();
            const uint32_t held = hidKeysHeld();
            const double elapsed = _timer.getS();
            _timer.reset();
            circlePosition circle{};
            hidCircleRead( &circle );
            const fheroes2::Display & display = fheroes2::Display::instance();
            const bool nativeDialog = fheroes2::is3DSNativeDialog();
            const auto dialogScopeId = fheroes2::get3DSDialogScopeId();
            const bool dialogChanged = dialogScopeId != _dialogScopeId;
            _dialogScopeId = dialogScopeId;
            const bool battleView = !nativeDialog && fheroes2::is3DSBattleView();
            const bool battleViewChanged = battleView != _wasBattleView;
            const bool dialogFreePointer = nativeDialog && ( held & KEY_L );
            if ( dialogFreePointer ) {
                auto viewport = fheroes2::get3DSDialogViewport();
                if ( !_wasDialogFreePointer || dialogChanged ) {
                    _dialogFreePointerPos
                        = { std::clamp( events._emulatedPointerPos.x - viewport.x, 0.0, 319.0 ), std::clamp( events._emulatedPointerPos.y - viewport.y, 0.0, 239.0 ) };
                    _dialogPanRemainder = {};
                }
                const int horizontal = static_cast<int>( ( held & KEY_DRIGHT ) != 0 ) - static_cast<int>( ( held & KEY_DLEFT ) != 0 );
                const int vertical = static_cast<int>( ( held & KEY_DDOWN ) != 0 ) - static_cast<int>( ( held & KEY_DUP ) != 0 );
                const double panDistance = 300.0 * std::clamp( elapsed, 0.0, 0.05 );
                _dialogPanRemainder.x = horizontal ? _dialogPanRemainder.x + horizontal * panDistance : 0.0;
                _dialogPanRemainder.y = vertical ? _dialogPanRemainder.y + vertical * panDistance : 0.0;
                const fheroes2::Point pan( static_cast<int32_t>( _dialogPanRemainder.x ), static_cast<int32_t>( _dialogPanRemainder.y ) );
                _dialogPanRemainder.x -= pan.x;
                _dialogPanRemainder.y -= pan.y;
                fheroes2::pan3DSDialogViewport( pan );
                const auto pannedViewport = fheroes2::get3DSDialogViewport();
                if ( viewport != pannedViewport ) {
                    updateDisplay = true;
                }
                viewport = pannedViewport;
                _dialogFreePointerPos.x = fheroes2::input3DS::cursorAxis( _dialogFreePointerPos.x, circle.dx, elapsed, viewport.width );
                _dialogFreePointerPos.y = fheroes2::input3DS::cursorAxis( _dialogFreePointerPos.y, -circle.dy, elapsed, viewport.height );
                if ( !_touchGate.active() ) {
                    const fheroes2::Point point( viewport.x + static_cast<int32_t>( _dialogFreePointerPos.x ),
                                                 viewport.y + static_cast<int32_t>( _dialogFreePointerPos.y ) );
                    if ( point != events.getMouseCursorPos() ) {
                        events.onMouseMotionEvent( point );
                    }
                    events._emulatedPointerPos = { viewport.x + _dialogFreePointerPos.x, viewport.y + _dialogFreePointerPos.y };
                }
                _dialogDirection = {};
                _dialogRepeat = 0.0;
            }
            else if ( nativeDialog ) {
                const auto buttons = fheroes2::get3DSDialogButtons();
                auto focused = fheroes2::get3DSDialogFocus();
                if ( std::none_of( buttons.begin(), buttons.end(), [focused]( const auto & button ) { return button.first == focused; } ) ) {
                    focused = buttons.empty() ? nullptr : buttons.front().first;
                }
                const int horizontal = static_cast<int>( ( held & KEY_DRIGHT ) != 0 ) - static_cast<int>( ( held & KEY_DLEFT ) != 0 );
                const int vertical = static_cast<int>( ( held & KEY_DDOWN ) != 0 ) - static_cast<int>( ( held & KEY_DUP ) != 0 );
                const int directionX = horizontal ? horizontal : ( circle.dx > 60 ? 1 : ( circle.dx < -60 ? -1 : 0 ) );
                const int directionY = vertical ? vertical : ( circle.dy > 60 ? -1 : ( circle.dy < -60 ? 1 : 0 ) );
                const fheroes2::Point direction( directionX, directionY );
                _dialogRepeat += std::clamp( elapsed, 0.0, 0.05 );
                if ( direction != fheroes2::Point() && ( dialogChanged || direction != _dialogDirection || _dialogRepeat >= 0.18 ) ) {
                    focused = nextDialogButton( buttons, focused, directionX, directionY );
                    _dialogRepeat = 0.0;
                }
                else if ( direction == fheroes2::Point() ) {
                    _dialogRepeat = 0.0;
                }
                _dialogDirection = direction;
                if ( focused != fheroes2::get3DSDialogFocus() ) {
                    fheroes2::set3DSDialogFocus( focused );
                    updateDisplay = true;
                }
                const auto selected = std::find_if( buttons.begin(), buttons.end(), [focused]( const auto & button ) { return button.first == focused; } );
                if ( selected != buttons.end() && !_touchGate.active() ) {
                    const auto oldViewport = fheroes2::get3DSDialogViewport();
                    fheroes2::focus3DSDialogButton( selected->second );
                    if ( oldViewport != fheroes2::get3DSDialogViewport() ) {
                        updateDisplay = true;
                    }
                    const fheroes2::Point point( selected->second.x + selected->second.width / 2, selected->second.y + selected->second.height / 2 );
                    if ( point != events.getMouseCursorPos() ) {
                        events.onMouseMotionEvent( point );
                    }
                }
                else if ( buttons.empty() ) {
                    // Legacy custom controls without ButtonBase registration
                    // still retain a pointer inside the bottom viewport.
                    const auto viewport = fheroes2::get3DSDialogViewport();
                    const double x = fheroes2::input3DS::cursorAxis( events._emulatedPointerPos.x - viewport.x, circle.dx, elapsed, viewport.width );
                    const double y = fheroes2::input3DS::cursorAxis( events._emulatedPointerPos.y - viewport.y, -circle.dy, elapsed, viewport.height );
                    const fheroes2::Point point( viewport.x + static_cast<int32_t>( x ), viewport.y + static_cast<int32_t>( y ) );
                    if ( point != events.getMouseCursorPos() ) {
                        events.onMouseMotionEvent( point );
                    }
                    events._emulatedPointerPos = { viewport.x + x, viewport.y + y };
                }
            }
            else if ( battleView ) {
                if ( !_wasBattleView ) {
                    const auto logical = events.getMouseCursorPos();
                    _battlePointerPos = { std::clamp( static_cast<double>( logical.x ) * 380.0 / 640.0, 0.0, 379.0 ),
                                          std::clamp( static_cast<double>( logical.y ) * 380.0 / 640.0 - 20.0, 0.0, 239.0 ) };
                }
                _battlePointerPos.x = fheroes2::input3DS::cursorAxis( _battlePointerPos.x, circle.dx, elapsed, 380 );
                _battlePointerPos.y = fheroes2::input3DS::cursorAxis( _battlePointerPos.y, -circle.dy, elapsed, 240 );
                const auto point = fheroes2::map3DSBattlePointer( { static_cast<int32_t>( _battlePointerPos.x ), static_cast<int32_t>( _battlePointerPos.y ) } );
                if ( !_touchGate.active() && point != events.getMouseCursorPos() )
                    events.onMouseMotionEvent( point );
                if ( !_touchGate.active() )
                    events._emulatedPointerPos = { static_cast<double>( point.x ), static_cast<double>( point.y ) };
            }
            else {
                const int width = fheroes2::is3DSAdventureLayout() ? 400 : display.width();
                const int height = fheroes2::is3DSAdventureLayout() ? 240 : display.height();
                if ( std::abs( circle.dx ) > 20 || std::abs( circle.dy ) > 20 ) {
                    const double x = fheroes2::input3DS::cursorAxis( events._emulatedPointerPos.x, circle.dx, elapsed, width );
                    const double y = fheroes2::input3DS::cursorAxis( events._emulatedPointerPos.y, -circle.dy, elapsed, height );
                    const fheroes2::Point point( static_cast<int32_t>( x ), static_cast<int32_t>( y ) );
                    if ( point != events.getMouseCursorPos() ) {
                        events.onMouseMotionEvent( point );
                    }
                    events._emulatedPointerPos = { x, y };
                }
            }
            _wasBattleView = battleView;
            _wasDialogFreePointer = dialogFreePointer;
            fheroes2::Point projectedTouch( -1, -1 );
            if ( held & KEY_TOUCH ) {
                touchPosition touch{};
                hidTouchRead( &touch );
                if ( nativeDialog ) {
                    projectedTouch = fheroes2::map3DSDialogTouch( { touch.px, touch.py } );
                }
                else if ( battleView ) {
                    projectedTouch = fheroes2::map3DSBattleTouch( { touch.px, touch.py } );
                }
                else if ( fheroes2::is3DSAdventureLayout() ) {
                    projectedTouch = { touch.px, touch.py + 240 };
                }
            }
            const bool touchEnabled = nativeDialog || fheroes2::is3DSAdventureLayout() || ( battleView && ( !( held & KEY_TOUCH ) || projectedTouch.x >= 0 ) );
            const auto touchTransition = _touchGate.update( touchEnabled && !battleViewChanged && !dialogChanged, held & KEY_TOUCH, down & KEY_TOUCH, up & KEY_TOUCH );
            if ( _touchGate.active() && ( held & KEY_TOUCH ) ) {
                _touch = projectedTouch;
                events.onMouseMotionEvent( _touch );
                if ( nativeDialog ) {
                    const auto buttons = fheroes2::get3DSDialogButtons();
                    const auto touched = std::find_if( buttons.begin(), buttons.end(), [this]( const auto & button ) { return button.second & _touch; } );
                    if ( touched != buttons.end() && touched->first != fheroes2::get3DSDialogFocus() ) {
                        fheroes2::set3DSDialogFocus( touched->first );
                        updateDisplay = true;
                    }
                }
            }
            using TouchTransition = fheroes2::input3DS::TouchGate::Transition;
            if ( touchTransition == TouchTransition::Press ) {
                events.onMouseButtonEvent( true, LocalEvent::MouseButtonType::MOUSE_BUTTON_LEFT, _touch );
            }
            else if ( touchTransition == TouchTransition::Release ) {
                events.onMouseButtonEvent( false, LocalEvent::MouseButtonType::MOUSE_BUTTON_LEFT, _touch );
            }
            else if ( touchTransition == TouchTransition::Cancel ) {
                // Do not synthesize a click on modal controls when the lower
                // screen becomes a frozen dashboard (or a blank screen).
                events.resetStates( LocalEvent::MOUSE_PRESSED | LocalEvent::MOUSE_RELEASED | LocalEvent::DRAG_ONGOING );
                if ( held & KEY_A ) {
                    events.onMouseButtonEvent( true, LocalEvent::MouseButtonType::MOUSE_BUTTON_LEFT, events.getMouseCursorPos() );
                }
            }
            if ( down & KEY_A )
                events.onMouseButtonEvent( true, LocalEvent::MouseButtonType::MOUSE_BUTTON_LEFT, events.getMouseCursorPos() );
            if ( up & KEY_A )
                events.onMouseButtonEvent( false, LocalEvent::MouseButtonType::MOUSE_BUTTON_LEFT, events.getMouseCursorPos() );

            fheroes2::Key key = fheroes2::Key::NONE;
            if ( ( down & ( KEY_B | KEY_SELECT ) ) && ( !nativeDialog || fheroes2::is3DSDialogClosable() ) )
                key = fheroes2::Key::KEY_ESCAPE;
            else if ( down & KEY_START )
                key = fheroes2::Key::KEY_ENTER;
            else if ( down & KEY_X )
                key = fheroes2::Key::KEY_E;
            else if ( down & KEY_Y )
                key = fheroes2::Key::KEY_H;
            else if ( !nativeDialog && !battleView && ( held & KEY_DLEFT ) )
                key = fheroes2::Key::KEY_LEFT;
            else if ( !nativeDialog && !battleView && ( held & KEY_DRIGHT ) )
                key = fheroes2::Key::KEY_RIGHT;
            else if ( !nativeDialog && !battleView && ( held & KEY_DUP ) )
                key = fheroes2::Key::KEY_UP;
            else if ( !nativeDialog && !battleView && ( held & KEY_DDOWN ) )
                key = fheroes2::Key::KEY_DOWN;
            if ( _previousKey != fheroes2::Key::NONE && _previousKey != key ) {
                events.onKeyboardEvent( _previousKey, 0, LocalEvent::KeyboardEventState::KEY_UP );
            }
            if ( key != fheroes2::Key::NONE ) {
                events.onKeyboardEvent( key, 0, LocalEvent::KeyboardEventState::KEY_DOWN );
            }
            _previousKey = key;
            return true;
        }

    private:
        fheroes2::Time _timer;
        fheroes2::Point _touch;
        fheroes2::PointBase2D<double> _battlePointerPos;
        bool _wasBattleView{ false };
        bool _wasDialogFreePointer{ false };
        fheroes2::PointBase2D<double> _dialogFreePointerPos;
        fheroes2::PointBase2D<double> _dialogPanRemainder;
        size_t _dialogScopeId{ 0 };
        fheroes2::Point _dialogDirection;
        double _dialogRepeat{ 0.0 };
        fheroes2::input3DS::TouchGate _touchGate;
        fheroes2::Key _previousKey{ fheroes2::Key::NONE };
    };
}
