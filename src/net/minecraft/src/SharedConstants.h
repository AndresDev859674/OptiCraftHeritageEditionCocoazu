#pragma once

#include "java/String.h"

namespace SharedConstants
{

static const jstring VERSION_STRING = "1.2.5";
extern const int NETWORK_PROTOCOL_VERSION;
extern const int maxChatLength;
// Read from /font.txt on first use. Java ran this in the class initializer;
// as a C++ global it would open a resource before main(), i.e. on the
// consoles before the asset devices are even initialised.
const jstring &acceptableLetters();

}
