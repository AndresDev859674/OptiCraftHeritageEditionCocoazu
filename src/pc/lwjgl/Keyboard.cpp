#include "lwjgl/Keyboard.h"

#include <queue>
#include <locale>
#include <codecvt>

#include "external/SDLException.h"

#include "SDL_events.h"

namespace lwjgl
{
    namespace Keyboard
    {

        // -------- Tablas de conversión --------
        //
        // Indice  = codigo LWJGL
        // Valor   = SDLK_*
        //
        // Tamano 256 es suficiente porque los codigos LWJGL usados
        // en este proyecto llegan hasta 223.
        static const int_t LWJGL_TO_SDL[256] = {
            //   0 -   9
            SDLK_UNKNOWN, SDLK_ESCAPE, SDLK_1, SDLK_2, SDLK_3,
            SDLK_4, SDLK_5, SDLK_6, SDLK_7, SDLK_8,
            //  10 -  19
            SDLK_9, SDLK_0, SDLK_MINUS, SDLK_EQUALS, SDLK_BACKSPACE,
            SDLK_TAB, SDLK_q, SDLK_w, SDLK_e, SDLK_r,
            //  20 -  29
            SDLK_t, SDLK_y, SDLK_u, SDLK_i, SDLK_o,
            SDLK_p, SDLK_LEFTBRACKET, SDLK_RIGHTBRACKET, SDLK_RETURN, SDLK_LCTRL,
            //  30 -  39
            SDLK_a, SDLK_s, SDLK_d, SDLK_f, SDLK_g,
            SDLK_h, SDLK_j, SDLK_k, SDLK_l, SDLK_SEMICOLON,
            //  40 -  49
            SDLK_QUOTE, SDLK_BACKQUOTE, SDLK_LSHIFT, SDLK_BACKSLASH, SDLK_z,
            SDLK_x, SDLK_c, SDLK_v, SDLK_b, SDLK_n,
            //  50 -  59
            SDLK_m, SDLK_COMMA, SDLK_PERIOD, SDLK_SLASH, SDLK_RSHIFT,
            SDLK_KP_MULTIPLY, SDLK_UNKNOWN /*56 LMENU*/, SDLK_SPACE, SDLK_UNKNOWN /*58 CAPITAL*/, SDLK_F1,
            //  60 -  69
            SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5, SDLK_F6,
            SDLK_F7, SDLK_F8, SDLK_F9, SDLK_F10, SDLK_NUMLOCKCLEAR,
            //  70 -  79
            SDLK_UNKNOWN /*70 SCROLL*/, SDLK_KP_7, SDLK_KP_8, SDLK_KP_9, SDLK_KP_MINUS,
            SDLK_KP_4, SDLK_KP_5, SDLK_KP_6, SDLK_KP_PLUS, SDLK_KP_1,
            //  80 -  89
            SDLK_KP_2, SDLK_KP_3, SDLK_KP_0, SDLK_KP_DECIMAL, SDLK_UNKNOWN,
            SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_F11, SDLK_F12, SDLK_UNKNOWN,
            //  90 -  99
            SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
            SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
            // 100 - 109
            SDLK_F13, SDLK_F14, SDLK_F15, SDLK_F16, SDLK_F17,
            SDLK_F18, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
            // 110 - 119
            SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN /*112 KANA*/, SDLK_F19, SDLK_UNKNOWN,
            SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
            // 120 - 129
            SDLK_UNKNOWN, SDLK_UNKNOWN /*121 CONVERT*/, SDLK_UNKNOWN, SDLK_UNKNOWN /*123 NOCONVERT*/, SDLK_UNKNOWN,
            SDLK_UNKNOWN /*125 YEN*/, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
            // 130 - 139
            SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
            SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
            // 140 - 149
            SDLK_UNKNOWN, SDLK_KP_EQUALS, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN /*144 CIRCUMFLEX*/,
            SDLK_AT, SDLK_COLON, SDLK_UNKNOWN /*147 UNDERLINE*/, SDLK_UNKNOWN /*148 KANJI*/, SDLK_STOP,
            // 150 - 159
            SDLK_UNKNOWN /*150 AX*/, SDLK_UNKNOWN /*151 UNLABELED*/, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
            SDLK_UNKNOWN, SDLK_KP_ENTER, SDLK_RCTRL, SDLK_UNKNOWN, SDLK_UNKNOWN,
            // 160 - 169
            SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
            SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN /*167 SECTION*/, SDLK_UNKNOWN, SDLK_UNKNOWN,
            // 170 - 179
            SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
            SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_KP_COMMA,
            // 180 - 189
            SDLK_UNKNOWN, SDLK_KP_DIVIDE, SDLK_UNKNOWN, SDLK_SYSREQ, SDLK_UNKNOWN /*184 RMENU*/,
            SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
            // 190 - 199
            SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
            SDLK_UNKNOWN /*196 FUNCTION*/, SDLK_PAUSE, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_HOME,
            // 200 - 209
            SDLK_UP, SDLK_PRIOR, SDLK_UNKNOWN, SDLK_LEFT, SDLK_UNKNOWN,
            SDLK_RIGHT, SDLK_UNKNOWN, SDLK_END, SDLK_DOWN, SDLK_UNKNOWN /*209 AC_FORWARD*/,
            // 210 - 219
            SDLK_INSERT, SDLK_DELETE, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN,
            SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_UNKNOWN, SDLK_CLEAR, SDLK_LGUI,
            // 220 - 223
            SDLK_RGUI, SDLK_UNKNOWN /*221 APPS*/, SDLK_POWER, SDLK_SLEEP
        };

        // Tamano del arreglo inverso. SDLK_* mas alto usado aqui es
        // SDLK_SLEEP (~ 0x4000 en SDL2), pero SDL los define como
        // scancodes | (1<<30). Para no depender del valor exacto,
        // calculamos el maximo en tiempo de compilacion seria complejo,
        // asi que usamos un mapa disperso via std::unordered_map, o
        // simplemente dimensionamos por el mayor SDLK_* + 1.
        //
        // Alternativa limpia: usar una std::unordered_map para el inverso.
        // Pero si prefieres arreglo, podemos usar el mayor + 1:
        static const int SDL_TO_LWJGL_MAX = SDLK_SLEEP + 1;

        // Se inicializa perezosamente (una sola vez)
        static int_t *buildSDLToLWJGL()
        {
            int_t *map = new int_t[SDL_TO_LWJGL_MAX];
            for (int i = 0; i < SDL_TO_LWJGL_MAX; ++i)
                map[i] = KEY_NONE;

            for (int lwjgl = 0; lwjgl < 256; ++lwjgl)
            {
                int sdl = LWJGL_TO_SDL[lwjgl];
                if (sdl != SDLK_UNKNOWN && sdl < SDL_TO_LWJGL_MAX)
                    map[sdl] = lwjgl;
            }
            return map;
        }

        static const int_t *SDL_TO_LWJGL = buildSDLToLWJGL();

        // -------- Funciones de conversion --------

        static int_t keyLWJGLToSDL(int_t key)
        {
            if (key < 0 || key >= 256)
                return SDLK_UNKNOWN;
            return LWJGL_TO_SDL[key];
        }

        static int_t keySDLToLWJGL(int_t key)
        {
            if (key < 0 || key >= SDL_TO_LWJGL_MAX)
                return KEY_NONE;
            return SDL_TO_LWJGL[key];
        }

        // -------- Event handling (sin cambios) --------

        namespace detail
        {
            struct Event
            {
                int_t key;
                int_t character;
                bool repeat;
                bool down;
            };

            static Event event_current = {};
            static std::queue<Event> event_queue;

            static bool has_retained_event = false;
            static Event event_retained = {};

            static void flushRetained()
            {
                if (!has_retained_event)
                    return;

                has_retained_event = false;
                event_queue.push(event_retained);
            }

            static void handleKey(int_t key, bool repeat, bool down)
            {
                flushRetained();
                has_retained_event = true;

                event_retained.key = key;
                event_retained.character = 0;
                event_retained.repeat = repeat;
                event_retained.down = down;
            }

            static void handleCharacter(int_t character)
            {
                if (has_retained_event && event_retained.character != 0)
                    flushRetained();
                if (!has_retained_event)
                    event_queue.push({KEY_NONE, character, false, true});
                else
                    event_retained.character = character;
            }

            void pushEvent(const SDL_Event &e)
            {
                if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)
                {
                    handleKey(keySDLToLWJGL(e.key.keysym.sym),
                              e.key.repeat,
                              e.key.state == SDL_PRESSED);
                }
                else
                {
                    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
                    std::u32string utf32 = conv.from_bytes(e.text.text);
                    for (char32_t c : utf32)
                        handleCharacter(c);
                }
            }
        }

        // -------- API publica (sin cambios) --------

        jstring getKeyName(int_t key)
        {
            return SDL_GetKeyName(keyLWJGLToSDL(key));
        }

        static bool allow_repeat_events = false;

        bool next()
        {
            detail::flushRetained();

            if (detail::event_queue.empty())
                return false;

            if (!allow_repeat_events)
            {
                while (1)
                {
                    if (detail::event_queue.empty())
                        return false;
                    if (detail::event_queue.front().repeat)
                        detail::event_queue.pop();
                    else
                        break;
                }
            }

            if (detail::event_queue.empty())
                return false;

            detail::event_current = detail::event_queue.front();
            detail::event_queue.pop();
            return true;
        }

        void enableRepeatEvents(bool repeat) { allow_repeat_events = repeat; }
        bool areRepeatEventsEnabled()        { return allow_repeat_events; }
        char_t getEventCharacter()           { return detail::event_current.character; }
        int_t  getEventKey()                 { return detail::event_current.key; }
        bool   getEventKeyState()            { return detail::event_current.down; }

        static const Uint8 *keyboard_state = nullptr;
        static int keyboard_state_size = 0;

        bool isKeyDown(int_t key)
        {
            if (keyboard_state == nullptr)
            {
                keyboard_state = SDL_GetKeyboardState(&keyboard_state_size);
                if (keyboard_state == nullptr)
                    throw SDLException();
            }

            int_t sdl_key = keyLWJGLToSDL(key);
            if (sdl_key == SDLK_UNKNOWN)
                return false;
            SDL_Scancode sc = SDL_GetScancodeFromKey(sdl_key);
            if (sc == SDL_SCANCODE_UNKNOWN)
                return false;
            if (sc < 0 || sc >= keyboard_state_size)
                return false;
            return keyboard_state[sc] != 0;
        }

    }
}