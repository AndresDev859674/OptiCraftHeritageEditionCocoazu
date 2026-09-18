#include "platform/Input.h"

PlatformTextInputSnapshot platformTextInputSnapshot(int port)
{
    (void)port;
    return {};
}

PlatformGamepadSnapshot platformGamepadSnapshot(int port)
{
    (void)port;
    return {};
}

PlatformGamepadSnapshot platformRawGamepadSnapshot(int port)
{
    (void)port;
    return {};
}

int platformMenuPad()
{
    return 0;
}

bool platformMenuPointerActive()
{
    return true;
}

bool platformMenuCursorVisible()
{
    return false;
}

void platformSetMenuCursor(int x, int y)
{
    (void)x;
    (void)y;
}

const PlatformKeyboardHints& platformKeyboardHints()
{
    static const PlatformKeyboardHints hints;
    return hints;
}

const char* platformInputDebugLine()
{
    return "";
}
