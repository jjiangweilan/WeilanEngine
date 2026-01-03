#pragma once
#include "Engine/Library/DynamicArray.hpp"
#include "Engine/Library/Math.hpp"
#include <SDL.h>

enum class [[LuaEnum]] GamepadButtons
{
    X = 0,
    Y = 1,
    A = 2,
    B = 3,
};

struct [[LuaClass]] Gamepad
{
    Gamepad(int idx) : gamepadID(idx) {}
    /**
     * reserved for multi gamepad support,
     * matching the ID of SDL_JoystickInstanceID
     */
    int gamepadID = 0;

    /**
     * 0: xbox controller, 1: dualsense
     */
    int gamepadType;

    /**
     * @brief: check if a button is pressed for the current frame, button index is basing on the return index of xbox
     * controller from SDL
     *
     * @param idx: 0 'X', 1 'Y', 2 'A', 3 'B'.
     */
    [[LuaFn]] bool IsButtonPressed(uint8_t idx);

    /**
     * @brief: check if a bumper is pressed for the current frame
     *
     * @param idx 0 'left bumper', 1 'right bumper'
     */
    [[LuaFn]] bool IsBumperPressed(uint8_t idx);

    /**
     * @brief: get trigger state for the current frame
     *
     * @return: 0.0f - 1.0f
     */
    [[LuaFn]] float GetTrigger(uint8_t idx);

    /**
     * @brief; get axis state
     *
     * @param idx 0 'left axis', 1 'right axis'
     */
    [[LuaFn]] float2 GetAxis(uint8_t idx);

private:
    /**
     * @brief: remap the button index based on controller type(xbox, dualsense, switch, etc...)
     */
    // TODO: implemented this as an int to int array remap
    uint8_t ButtonRemap(uint8_t idx) { return idx; }
};

enum class InputKeycode
{
};

enum class [[LuaEnum]] InputScancode
{
    Key_UNKNOWN = SDL_SCANCODE_UNKNOWN,

    /**
     *  \name Usage page 0x07
     *
     *  These values are from usage page 0x07 (USB keyboard page).
     */
    /* @{ */

    Key_A = SDL_SCANCODE_A,
    Key_B = SDL_SCANCODE_B,
    Key_C = SDL_SCANCODE_C,
    Key_D = SDL_SCANCODE_D,
    Key_E = SDL_SCANCODE_E,
    Key_F = SDL_SCANCODE_F,
    Key_G = SDL_SCANCODE_G,
    Key_H = SDL_SCANCODE_H,
    Key_I = SDL_SCANCODE_I,
    Key_J = SDL_SCANCODE_J,
    Key_K = SDL_SCANCODE_K,
    Key_L = SDL_SCANCODE_L,
    Key_M = SDL_SCANCODE_M,
    Key_N = SDL_SCANCODE_N,
    Key_O = SDL_SCANCODE_O,
    Key_P = SDL_SCANCODE_P,
    Key_Q = SDL_SCANCODE_Q,
    Key_R = SDL_SCANCODE_R,
    Key_S = SDL_SCANCODE_S,
    Key_T = SDL_SCANCODE_T,
    Key_U = SDL_SCANCODE_U,
    Key_V = SDL_SCANCODE_V,
    Key_W = SDL_SCANCODE_W,
    Key_X = SDL_SCANCODE_X,
    Key_Y = SDL_SCANCODE_Y,
    Key_Z = SDL_SCANCODE_Z,

    Key_1 = SDL_SCANCODE_1,
    Key_2 = SDL_SCANCODE_2,
    Key_3 = SDL_SCANCODE_3,
    Key_4 = SDL_SCANCODE_4,
    Key_5 = SDL_SCANCODE_5,
    Key_6 = SDL_SCANCODE_6,
    Key_7 = SDL_SCANCODE_7,
    Key_8 = SDL_SCANCODE_8,
    Key_9 = SDL_SCANCODE_9,
    Key_0 = SDL_SCANCODE_0,

    Key_RETURN = SDL_SCANCODE_RETURN,
    Key_ESCAPE = SDL_SCANCODE_ESCAPE,
    Key_BACKSPACE = SDL_SCANCODE_BACKSPACE,
    Key_TAB = SDL_SCANCODE_TAB,
    Key_SPACE = SDL_SCANCODE_SPACE,

    Key_MINUS = SDL_SCANCODE_MINUS,
    Key_EQUALS = SDL_SCANCODE_EQUALS,
    Key_LEFTBRACKET = SDL_SCANCODE_LEFTBRACKET,
    Key_RIGHTBRACKET = SDL_SCANCODE_RIGHTBRACKET,
    Key_BACKSLASH = SDL_SCANCODE_BACKSLASH, /**< Located at the lower left of the return
                                  *   key on ISO keyboards and at the right end
                                  *   of the QWERTY row on ANSI keyboards.
                                  *   Produces REVERSE SOLIDUS (backslash) and
                                  *   VERTICAL LINE in a US layout, REVERSE
                                  *   SOLIDUS and VERTICAL LINE in a UK Mac
                                  *   layout, NUMBER SIGN and TILDE in a UK
                                  *   Windows layout, DOLLAR SIGN and POUND SIGN
                                  *   in a Swiss German layout, NUMBER SIGN and
                                  *   APOSTROPHE in a German layout, GRAVE
                                  *   ACCENT and POUND SIGN in a French Mac
                                  *   layout, and ASTERISK and MICRO SIGN in a
                                  *   French Windows layout.
                                  */
    Key_NONUSHASH = SDL_SCANCODE_NONUSHASH, /**< ISO USB keyboards actually use this code
                                  *   instead of 49 for the same key, but all
                                  *   OSes I've seen treat the two codes
                                  *   identically. So, as an implementor, unless
                                  *   your keyboard generates both of those
                                  *   codes and your OS treats them differently,
                                  *   you should generate Key_BACKSLASH = SDL_SCANCODE_BACKSLASH
                                  *   instead of this code. As a user, you
                                  *   should not rely on this code because SDL
                                  *   will never generate it with most (all?)
                                  *   keyboards.
                                  */
    Key_SEMICOLON = SDL_SCANCODE_SEMICOLON,
    Key_APOSTROPHE = SDL_SCANCODE_APOSTROPHE,
    Key_GRAVE = SDL_SCANCODE_GRAVE, /**< Located in the top left corner (on both ANSI
                              *   and ISO keyboards). Produces GRAVE ACCENT and
                              *   TILDE in a US Windows layout and in US and UK
                              *   Mac layouts on ANSI keyboards, GRAVE ACCENT
                              *   and NOT SIGN in a UK Windows layout, SECTION
                              *   SIGN and PLUS-MINUS SIGN in US and UK Mac
                              *   layouts on ISO keyboards, SECTION SIGN and
                              *   DEGREE SIGN in a Swiss German layout (Mac:
                              *   only on ISO keyboards), CIRCUMFLEX ACCENT and
                              *   DEGREE SIGN in a German layout (Mac: only on
                              *   ISO keyboards), SUPERSCRIPT TWO and TILDE in a
                              *   French Windows layout, COMMERCIAL AT and
                              *   NUMBER SIGN in a French Mac layout on ISO
                              *   keyboards, and LESS-THAN SIGN and GREATER-THAN
                              *   SIGN in a Swiss German, German, or French Mac
                              *   layout on ANSI keyboards.
                              */
    Key_COMMA = SDL_SCANCODE_COMMA,
    Key_PERIOD = SDL_SCANCODE_PERIOD,
    Key_SLASH = SDL_SCANCODE_SLASH,

    Key_CAPSLOCK = SDL_SCANCODE_CAPSLOCK,

    Key_F1 = SDL_SCANCODE_F1,
    Key_F2 = SDL_SCANCODE_F2,
    Key_F3 = SDL_SCANCODE_F3,
    Key_F4 = SDL_SCANCODE_F4,
    Key_F5 = SDL_SCANCODE_F5,
    Key_F6 = SDL_SCANCODE_F6,
    Key_F7 = SDL_SCANCODE_F7,
    Key_F8 = SDL_SCANCODE_F8,
    Key_F9 = SDL_SCANCODE_F9,
    Key_F10 = SDL_SCANCODE_F10,
    Key_F11 = SDL_SCANCODE_F11,
    Key_F12 = SDL_SCANCODE_F12,

    Key_PRINTSCREEN = SDL_SCANCODE_PRINTSCREEN,
    Key_SCROLLLOCK = SDL_SCANCODE_SCROLLLOCK,
    Key_PAUSE = SDL_SCANCODE_PAUSE,
    Key_INSERT = SDL_SCANCODE_INSERT, /**< insert on PC, help on some Mac keyboards (but
                                   does send code 73, not 117) */
    Key_HOME = SDL_SCANCODE_HOME,
    Key_PAGEUP = SDL_SCANCODE_PAGEUP,
    Key_DELETE = SDL_SCANCODE_DELETE,
    Key_END = SDL_SCANCODE_END,
    Key_PAGEDOWN = SDL_SCANCODE_PAGEDOWN,
    Key_RIGHT = SDL_SCANCODE_RIGHT,
    Key_LEFT = SDL_SCANCODE_LEFT,
    Key_DOWN = SDL_SCANCODE_DOWN,
    Key_UP = SDL_SCANCODE_UP,

    Key_NUMLOCKCLEAR = SDL_SCANCODE_NUMLOCKCLEAR, /**< num lock on PC, clear on Mac keyboards
                                     */
    Key_KP_DIVIDE = SDL_SCANCODE_KP_DIVIDE,
    Key_KP_MULTIPLY = SDL_SCANCODE_KP_MULTIPLY,
    Key_KP_MINUS = SDL_SCANCODE_KP_MINUS,
    Key_KP_PLUS = SDL_SCANCODE_KP_PLUS,
    Key_KP_ENTER = SDL_SCANCODE_KP_ENTER,
    Key_KP_1 = SDL_SCANCODE_KP_1,
    Key_KP_2 = SDL_SCANCODE_KP_2,
    Key_KP_3 = SDL_SCANCODE_KP_3,
    Key_KP_4 = SDL_SCANCODE_KP_4,
    Key_KP_5 = SDL_SCANCODE_KP_5,
    Key_KP_6 = SDL_SCANCODE_KP_6,
    Key_KP_7 = SDL_SCANCODE_KP_7,
    Key_KP_8 = SDL_SCANCODE_KP_8,
    Key_KP_9 = SDL_SCANCODE_KP_9,
    Key_KP_0 = SDL_SCANCODE_KP_0,
    Key_KP_PERIOD = SDL_SCANCODE_KP_PERIOD,

    Key_NONUSBACKSLASH = SDL_SCANCODE_NONUSBACKSLASH, /**< This is the additional key that ISO
                                        *   keyboards have over ANSI ones,
                                        *   located between left shift and Y.
                                        *   Produces GRAVE ACCENT and TILDE in a
                                        *   US or UK Mac layout, REVERSE SOLIDUS
                                        *   (backslash) and VERTICAL LINE in a
                                        *   US or UK Windows layout, and
                                        *   LESS-THAN SIGN and GREATER-THAN SIGN
                                        *   in a Swiss German, German, or French
                                        *   layout. */
    Key_APPLICATION = SDL_SCANCODE_APPLICATION,    /**< windows contextual menu, compose */
    Key_POWER = SDL_SCANCODE_POWER,          /**< The USB document says this is a status flag,
                                        *   not a physical key - but some Mac keyboards
                                        *   do have a power key. */
    Key_KP_EQUALS = SDL_SCANCODE_KP_EQUALS,
    Key_F13 = SDL_SCANCODE_F13,
    Key_F14 = SDL_SCANCODE_F14,
    Key_F15 = SDL_SCANCODE_F15,
    Key_F16 = SDL_SCANCODE_F16,
    Key_F17 = SDL_SCANCODE_F17,
    Key_F18 = SDL_SCANCODE_F18,
    Key_F19 = SDL_SCANCODE_F19,
    Key_F20 = SDL_SCANCODE_F20,
    Key_F21 = SDL_SCANCODE_F21,
    Key_F22 = SDL_SCANCODE_F22,
    Key_F23 = SDL_SCANCODE_F23,
    Key_F24 = SDL_SCANCODE_F24,
    Key_EXECUTE = SDL_SCANCODE_EXECUTE,
    Key_HELP = SDL_SCANCODE_HELP, /**< AL Integrated Help Center */
    Key_MENU = SDL_SCANCODE_MENU, /**< Menu (show menu) */
    Key_SELECT = SDL_SCANCODE_SELECT,
    Key_STOP = SDL_SCANCODE_STOP,  /**< AC Stop */
    Key_AGAIN = SDL_SCANCODE_AGAIN, /**< AC Redo/Repeat */
    Key_UNDO = SDL_SCANCODE_UNDO,  /**< AC Undo */
    Key_CUT = SDL_SCANCODE_CUT,   /**< AC Cut */
    Key_COPY = SDL_SCANCODE_COPY,  /**< AC Copy */
    Key_PASTE = SDL_SCANCODE_PASTE, /**< AC Paste */
    Key_FIND = SDL_SCANCODE_FIND,  /**< AC Find */
    Key_MUTE = SDL_SCANCODE_MUTE,
    Key_VOLUMEUP = SDL_SCANCODE_VOLUMEUP,
    Key_VOLUMEDOWN = SDL_SCANCODE_VOLUMEDOWN,
    /* not sure whether there's a reason to enable these */
    /*     Key_LOCKINGCAPSLOCK = SDL_SCANCODE_LOCKINGCAPSLOCK,  */
    /*     Key_LOCKINGNUMLOCK = SDL_SCANCODE_LOCKINGNUMLOCK, */
    /*     Key_LOCKINGSCROLLLOCK = SDL_SCANCODE_LOCKINGSCROLLLOCK, */
    Key_KP_COMMA = SDL_SCANCODE_KP_COMMA,
    Key_KP_EQUALSAS400 = SDL_SCANCODE_KP_EQUALSAS400,

    Key_INTERNATIONAL1 = SDL_SCANCODE_INTERNATIONAL1, /**< used on Asian keyboards, see
                                            footnotes in USB doc */
    Key_INTERNATIONAL2 = SDL_SCANCODE_INTERNATIONAL2,
    Key_INTERNATIONAL3 = SDL_SCANCODE_INTERNATIONAL3, /**< Yen */
    Key_INTERNATIONAL4 = SDL_SCANCODE_INTERNATIONAL4,
    Key_INTERNATIONAL5 = SDL_SCANCODE_INTERNATIONAL5,
    Key_INTERNATIONAL6 = SDL_SCANCODE_INTERNATIONAL6,
    Key_INTERNATIONAL7 = SDL_SCANCODE_INTERNATIONAL7,
    Key_INTERNATIONAL8 = SDL_SCANCODE_INTERNATIONAL8,
    Key_INTERNATIONAL9 = SDL_SCANCODE_INTERNATIONAL9,
    Key_LANG1 = SDL_SCANCODE_LANG1, /**< Hangul/English toggle */
    Key_LANG2 = SDL_SCANCODE_LANG2, /**< Hanja conversion */
    Key_LANG3 = SDL_SCANCODE_LANG3, /**< Katakana */
    Key_LANG4 = SDL_SCANCODE_LANG4, /**< Hiragana */
    Key_LANG5 = SDL_SCANCODE_LANG5, /**< Zenkaku/Hankaku */
    Key_LANG6 = SDL_SCANCODE_LANG6, /**< reserved */
    Key_LANG7 = SDL_SCANCODE_LANG7, /**< reserved */
    Key_LANG8 = SDL_SCANCODE_LANG8, /**< reserved */
    Key_LANG9 = SDL_SCANCODE_LANG9, /**< reserved */

    Key_ALTERASE = SDL_SCANCODE_ALTERASE, /**< Erase-Eaze */
    Key_SYSREQ = SDL_SCANCODE_SYSREQ,
    Key_CANCEL = SDL_SCANCODE_CANCEL, /**< AC Cancel */
    Key_CLEAR = SDL_SCANCODE_CLEAR,
    Key_PRIOR = SDL_SCANCODE_PRIOR,
    Key_RETURN2 = SDL_SCANCODE_RETURN2,
    Key_SEPARATOR = SDL_SCANCODE_SEPARATOR,
    Key_OUT = SDL_SCANCODE_OUT,
    Key_OPER = SDL_SCANCODE_OPER,
    Key_CLEARAGAIN = SDL_SCANCODE_CLEARAGAIN,
    Key_CRSEL = SDL_SCANCODE_CRSEL,
    Key_EXSEL = SDL_SCANCODE_EXSEL,

    Key_KP_00 = SDL_SCANCODE_KP_00,
    Key_KP_000 = SDL_SCANCODE_KP_000,
    Key_THOUSANDSSEPARATOR = SDL_SCANCODE_THOUSANDSSEPARATOR,
    Key_DECIMALSEPARATOR = SDL_SCANCODE_DECIMALSEPARATOR,
    Key_CURRENCYUNIT = SDL_SCANCODE_CURRENCYUNIT,
    Key_CURRENCYSUBUNIT = SDL_SCANCODE_CURRENCYSUBUNIT,
    Key_KP_LEFTPAREN = SDL_SCANCODE_KP_LEFTPAREN,
    Key_KP_RIGHTPAREN = SDL_SCANCODE_KP_RIGHTPAREN,
    Key_KP_LEFTBRACE = SDL_SCANCODE_KP_LEFTBRACE,
    Key_KP_RIGHTBRACE = SDL_SCANCODE_KP_RIGHTBRACE,
    Key_KP_TAB = SDL_SCANCODE_KP_TAB,
    Key_KP_BACKSPACE = SDL_SCANCODE_KP_BACKSPACE,
    Key_KP_A = SDL_SCANCODE_KP_A,
    Key_KP_B = SDL_SCANCODE_KP_B,
    Key_KP_C = SDL_SCANCODE_KP_C,
    Key_KP_D = SDL_SCANCODE_KP_D,
    Key_KP_E = SDL_SCANCODE_KP_E,
    Key_KP_F = SDL_SCANCODE_KP_F,
    Key_KP_XOR = SDL_SCANCODE_KP_XOR,
    Key_KP_POWER = SDL_SCANCODE_KP_POWER,
    Key_KP_PERCENT = SDL_SCANCODE_KP_PERCENT,
    Key_KP_LESS = SDL_SCANCODE_KP_LESS,
    Key_KP_GREATER = SDL_SCANCODE_KP_GREATER,
    Key_KP_AMPERSAND = SDL_SCANCODE_KP_AMPERSAND,
    Key_KP_DBLAMPERSAND = SDL_SCANCODE_KP_DBLAMPERSAND,
    Key_KP_VERTICALBAR = SDL_SCANCODE_KP_VERTICALBAR,
    Key_KP_DBLVERTICALBAR = SDL_SCANCODE_KP_DBLVERTICALBAR,
    Key_KP_COLON = SDL_SCANCODE_KP_COLON,
    Key_KP_HASH = SDL_SCANCODE_KP_HASH,
    Key_KP_SPACE = SDL_SCANCODE_KP_SPACE,
    Key_KP_AT = SDL_SCANCODE_KP_AT,
    Key_KP_EXCLAM = SDL_SCANCODE_KP_EXCLAM,
    Key_KP_MEMSTORE = SDL_SCANCODE_KP_MEMSTORE,
    Key_KP_MEMRECALL = SDL_SCANCODE_KP_MEMRECALL,
    Key_KP_MEMCLEAR = SDL_SCANCODE_KP_MEMCLEAR,
    Key_KP_MEMADD = SDL_SCANCODE_KP_MEMADD,
    Key_KP_MEMSUBTRACT = SDL_SCANCODE_KP_MEMSUBTRACT,
    Key_KP_MEMMULTIPLY = SDL_SCANCODE_KP_MEMMULTIPLY,
    Key_KP_MEMDIVIDE = SDL_SCANCODE_KP_MEMDIVIDE,
    Key_KP_PLUSMINUS = SDL_SCANCODE_KP_PLUSMINUS,
    Key_KP_CLEAR = SDL_SCANCODE_KP_CLEAR,
    Key_KP_CLEARENTRY = SDL_SCANCODE_KP_CLEARENTRY,
    Key_KP_BINARY = SDL_SCANCODE_KP_BINARY,
    Key_KP_OCTAL = SDL_SCANCODE_KP_OCTAL,
    Key_KP_DECIMAL = SDL_SCANCODE_KP_DECIMAL,
    Key_KP_HEXADECIMAL = SDL_SCANCODE_KP_HEXADECIMAL,

    Key_LCTRL = SDL_SCANCODE_LCTRL,
    Key_LSHIFT = SDL_SCANCODE_LSHIFT,
    Key_LALT = SDL_SCANCODE_LALT, /**< alt, option */
    Key_LGUI = SDL_SCANCODE_LGUI, /**< windows, command (apple), meta */
    Key_RCTRL = SDL_SCANCODE_RCTRL,
    Key_RSHIFT = SDL_SCANCODE_RSHIFT,
    Key_RALT = SDL_SCANCODE_RALT, /**< alt gr, option */
    Key_RGUI = SDL_SCANCODE_RGUI, /**< windows, command (apple), meta */

    Key_MODE = SDL_SCANCODE_MODE, /**< I'm not sure if this is really not covered
                              *   by any of the above, but since there's a
                              *   special KMOD_MODE for it I'm adding it here
                              */

    /* @} */ /* Usage page 0x07 */

    /**
     *  \name Usage page 0x0C
     *
     *  These values are mapped from usage page 0x0C (USB consumer page).
     *  See https://usb.org/sites/default/files/hut1_2.pdf
     *
     *  There are way more keys in the spec than we can represent in the
     *  current scancode range, so pick the ones that commonly come up in
     *  real world usage.
     */
    /* @{ */

    Key_AUDIONEXT = SDL_SCANCODE_AUDIONEXT,
    Key_AUDIOPREV = SDL_SCANCODE_AUDIOPREV,
    Key_AUDIOSTOP = SDL_SCANCODE_AUDIOSTOP,
    Key_AUDIOPLAY = SDL_SCANCODE_AUDIOPLAY,
    Key_AUDIOMUTE = SDL_SCANCODE_AUDIOMUTE,
    Key_MEDIASELECT = SDL_SCANCODE_MEDIASELECT,
    Key_WWW = SDL_SCANCODE_WWW, /**< AL Internet Browser */
    Key_MAIL = SDL_SCANCODE_MAIL,
    Key_CALCULATOR = SDL_SCANCODE_CALCULATOR, /**< AL Calculator */
    Key_COMPUTER = SDL_SCANCODE_COMPUTER,
    Key_AC_SEARCH = SDL_SCANCODE_AC_SEARCH,    /**< AC Search */
    Key_AC_HOME = SDL_SCANCODE_AC_HOME,      /**< AC Home */
    Key_AC_BACK = SDL_SCANCODE_AC_BACK,      /**< AC Back */
    Key_AC_FORWARD = SDL_SCANCODE_AC_FORWARD,   /**< AC Forward */
    Key_AC_STOP = SDL_SCANCODE_AC_STOP,      /**< AC Stop */
    Key_AC_REFRESH = SDL_SCANCODE_AC_REFRESH,   /**< AC Refresh */
    Key_AC_BOOKMARKS = SDL_SCANCODE_AC_BOOKMARKS, /**< AC Bookmarks */

    /* @} */ /* Usage page 0x0C */

    /**
     *  \name Walther keys
     *
     *  These are values that Christian Walther added (for mac keyboard?).
     */
    /* @{ */

    Key_BRIGHTNESSDOWN = SDL_SCANCODE_BRIGHTNESSDOWN,
    Key_BRIGHTNESSUP = SDL_SCANCODE_BRIGHTNESSUP,
    Key_DISPLAYSWITCH = SDL_SCANCODE_DISPLAYSWITCH, /**< display mirroring/dual display
                                           switch, video mode switch */
    Key_KBDILLUMTOGGLE = SDL_SCANCODE_KBDILLUMTOGGLE,
    Key_KBDILLUMDOWN = SDL_SCANCODE_KBDILLUMDOWN,
    Key_KBDILLUMUP = SDL_SCANCODE_KBDILLUMUP,
    Key_EJECT = SDL_SCANCODE_EJECT,
    Key_SLEEP = SDL_SCANCODE_SLEEP, /**< SC System Sleep */

    Key_APP1 = SDL_SCANCODE_APP1,
    Key_APP2 = SDL_SCANCODE_APP2,

    /* @} */ /* Walther keys */

    /**
     *  \name Usage page 0x0C (additional media keys)
     *
     *  These values are mapped from usage page 0x0C (USB consumer page).
     */
    /* @{ */

    Key_AUDIOREWIND = SDL_SCANCODE_AUDIOREWIND,
    Key_AUDIOFASTFORWARD = SDL_SCANCODE_AUDIOFASTFORWARD,

    /* @} */ /* Usage page 0x0C (additional media keys) */

    /**
     *  \name Mobile keys
     *
     *  These are values that are often used on mobile phones.
     */
    /* @{ */

    Key_SOFTLEFT = SDL_SCANCODE_SOFTLEFT,  /**< Usually situated below the display on phones and
                                       used as a multi-function feature key for selecting
                                       a software defined function shown on the bottom left
                                       of the display. */
    Key_SOFTRIGHT = SDL_SCANCODE_SOFTRIGHT, /**< Usually situated below the display on phones and
                                       used as a multi-function feature key for selecting
                                       a software defined function shown on the bottom right
                                       of the display. */
    Key_CALL = SDL_SCANCODE_CALL,      /**< Used for accepting phone calls. */
    Key_ENDCALL = SDL_SCANCODE_ENDCALL,   /**< Used for rejecting phone calls. */

    /* @} */ /* Mobile keys */

    /* Add any other keys here. */

    SDL_NUM_SCANCODES /**< not a key, just marks the number of scancodes
                                 for array bounds */
};

/**
 * @class Input
 * @brief We need an improvement on Input handling, like jump command should append to a process queue instead of
 * directly query the button state. An input command queue is very command specific, so just specialize it
 *
 */

class [[LuaClass]] Input
{
public:
    [[LuaFn]] static Gamepad GetGamepad(int padIdx = 0);
    static int2 GetMousePosition();
    [[LuaFn]] static float GetMovementX();
    [[LuaFn]] static float GetMovementY();
    [[LuaFn]] static bool IsInteractPressed();
    static void GetMovement(float& x, float& y);
    static void GetLookAround(float& x, float& y);
    [[LuaFn]] static float GetLookAroundX();
    [[LuaFn]] static float GetLookAroundY();
    [[LuaFn]] static bool Jump();
    static void PushEvent(SDL_Event& event);
    static void SetGameplayInput(bool enabled);
    static void Reset();
    static void UpdateState();
    static bool IsScancodeDown(InputScancode keycode);
};
