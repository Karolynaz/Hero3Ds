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
        static void initTouchpad() { fheroes2::cursor().forceSoftwareEmulation(); }
        void initController() { initTouchpad(); }
        void closeController() {}
        bool isControllerValid() const { return false; }
        static int32_t getCurrentKeyModifiers() { return 0; }
        static void sleep( const uint32_t milliseconds ) { svcSleepThread( static_cast<int64_t>( milliseconds ) * 1000000 ); }
        static const char * getKeyName( const fheroes2::Key key )
        {
            switch ( key ) {
            case fheroes2::Key::KEY_ESCAPE: return "B";
            case fheroes2::Key::KEY_ENTER: return "Start";
            case fheroes2::Key::KEY_LEFT: return "D-Pad Left";
            case fheroes2::Key::KEY_RIGHT: return "D-Pad Right";
            case fheroes2::Key::KEY_UP: return "D-Pad Up";
            case fheroes2::Key::KEY_DOWN: return "D-Pad Down";
            default: return "";
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
            if ( held & KEY_TOUCH || up & KEY_TOUCH ) {
                if ( held & KEY_TOUCH ) {
                    touchPosition touch{};
                    hidTouchRead( &touch );
                    _touch = fheroes2::is3DSAdventureLayout() ? fheroes2::Point( touch.px, touch.py + 240 )
                                                           : fheroes2::Point( touch.px * display.width() / 320, touch.py * display.height() / 240 );
                    events.onMouseMotionEvent( _touch );
                }
                if ( down & KEY_TOUCH ) events.onMouseButtonEvent( true, LocalEvent::MouseButtonType::MOUSE_BUTTON_LEFT, _touch );
                if ( up & KEY_TOUCH ) events.onMouseButtonEvent( false, LocalEvent::MouseButtonType::MOUSE_BUTTON_LEFT, _touch );
            }
            if ( down & KEY_A ) events.onMouseButtonEvent( true, LocalEvent::MouseButtonType::MOUSE_BUTTON_LEFT, events.getMouseCursorPos() );
            if ( up & KEY_A ) events.onMouseButtonEvent( false, LocalEvent::MouseButtonType::MOUSE_BUTTON_LEFT, events.getMouseCursorPos() );

            fheroes2::Key key = fheroes2::Key::NONE;
            if ( down & ( KEY_B | KEY_SELECT ) ) key = fheroes2::Key::KEY_ESCAPE;
            else if ( down & KEY_START ) key = fheroes2::Key::KEY_ENTER;
            else if ( down & KEY_X ) key = fheroes2::Key::KEY_E;
            else if ( down & KEY_Y ) key = fheroes2::Key::KEY_H;
            else if ( held & KEY_DLEFT ) key = fheroes2::Key::KEY_LEFT;
            else if ( held & KEY_DRIGHT ) key = fheroes2::Key::KEY_RIGHT;
            else if ( held & KEY_DUP ) key = fheroes2::Key::KEY_UP;
            else if ( held & KEY_DDOWN ) key = fheroes2::Key::KEY_DOWN;
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
        fheroes2::Key _previousKey{ fheroes2::Key::NONE };
    };
}
