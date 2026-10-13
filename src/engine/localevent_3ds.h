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
            const bool battleView = fheroes2::is3DSBattleView();
            const bool battleViewChanged = battleView != _wasBattleView;
            if ( battleView ) {
                auto camera = fheroes2::get3DSBattleCamera();
                if ( !_wasBattleView ) {
                    _battlePointerPos = { std::clamp( static_cast<double>( events.getMouseCursorPos().x - camera.x ), 0.0, 399.0 ),
                                          std::clamp( static_cast<double>( events.getMouseCursorPos().y - camera.y ), 0.0, 239.0 ) };
                    _cameraPanRemainder = {};
                }
                const int horizontal = static_cast<int>( ( held & KEY_DRIGHT ) != 0 ) - static_cast<int>( ( held & KEY_DLEFT ) != 0 );
                const int vertical = static_cast<int>( ( held & KEY_DDOWN ) != 0 ) - static_cast<int>( ( held & KEY_DUP ) != 0 );
                const double panDistance = 300.0 * std::clamp( elapsed, 0.0, 0.05 );
                _cameraPanRemainder.x = horizontal ? _cameraPanRemainder.x + horizontal * panDistance : 0.0;
                _cameraPanRemainder.y = vertical ? _cameraPanRemainder.y + vertical * panDistance : 0.0;
                const fheroes2::Point pan( static_cast<int32_t>( _cameraPanRemainder.x ), static_cast<int32_t>( _cameraPanRemainder.y ) );
                _cameraPanRemainder.x -= pan.x;
                _cameraPanRemainder.y -= pan.y;
                if ( pan != fheroes2::Point() ) {
                    fheroes2::pan3DSBattleCamera( pan );
                    updateDisplay = true;
                }
                camera = fheroes2::get3DSBattleCamera();
                _battlePointerPos.x = fheroes2::input3DS::cursorAxis( _battlePointerPos.x, circle.dx, elapsed, 400 );
                _battlePointerPos.y = fheroes2::input3DS::cursorAxis( _battlePointerPos.y, -circle.dy, elapsed, 240 );
                // Camera changes (including automatic enemy following) retain
                // the physical cursor location within the visible top screen.
                const fheroes2::Point point( camera.x + static_cast<int32_t>( _battlePointerPos.x ), camera.y + static_cast<int32_t>( _battlePointerPos.y ) );
                if ( !_touchGate.active() && point != events.getMouseCursorPos() ) {
                    events.onMouseMotionEvent( point );
                }
                if ( !_touchGate.active() ) {
                    events._emulatedPointerPos = { camera.x + _battlePointerPos.x, camera.y + _battlePointerPos.y };
                }
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
            fheroes2::Point projectedTouch( -1, -1 );
            if ( held & KEY_TOUCH ) {
                touchPosition touch{};
                hidTouchRead( &touch );
                if ( battleView ) {
                    projectedTouch = fheroes2::map3DSBattleTouch( { touch.px, touch.py } );
                }
                else if ( fheroes2::is3DSAdventureLayout() ) {
                    projectedTouch = { touch.px, touch.py + 240 };
                }
            }
            const bool touchEnabled = fheroes2::is3DSAdventureLayout() || ( battleView && ( !( held & KEY_TOUCH ) || projectedTouch.x >= 0 ) );
            const auto touchTransition = _touchGate.update( touchEnabled && !battleViewChanged, held & KEY_TOUCH, down & KEY_TOUCH, up & KEY_TOUCH );
            if ( _touchGate.active() && ( held & KEY_TOUCH ) ) {
                _touch = projectedTouch;
                events.onMouseMotionEvent( _touch );
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
            if ( down & ( KEY_B | KEY_SELECT ) )
                key = fheroes2::Key::KEY_ESCAPE;
            else if ( down & KEY_START )
                key = fheroes2::Key::KEY_ENTER;
            else if ( down & KEY_X )
                key = fheroes2::Key::KEY_E;
            else if ( down & KEY_Y )
                key = fheroes2::Key::KEY_H;
            else if ( !battleView && ( held & KEY_DLEFT ) )
                key = fheroes2::Key::KEY_LEFT;
            else if ( !battleView && ( held & KEY_DRIGHT ) )
                key = fheroes2::Key::KEY_RIGHT;
            else if ( !battleView && ( held & KEY_DUP ) )
                key = fheroes2::Key::KEY_UP;
            else if ( !battleView && ( held & KEY_DDOWN ) )
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
        fheroes2::PointBase2D<double> _cameraPanRemainder;
        bool _wasBattleView{ false };
        fheroes2::input3DS::TouchGate _touchGate;
        fheroes2::Key _previousKey{ fheroes2::Key::NONE };
    };
}
