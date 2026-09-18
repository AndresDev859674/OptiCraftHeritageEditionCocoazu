#ifdef PS2_PLATFORM

#include <cstdlib>

// libstdc++ defaults std::terminate to __verbose_terminate_handler(), whose
// diagnostic path pulls the C++ demangler into the static PS2 executable. The
// game installs Ps2EarlyCrash during bootstrap, so this is only the lightweight
// fallback used before that handler is installed.
namespace __gnu_cxx
{
void __verbose_terminate_handler()
{
	std::abort();
}
}

#endif // PS2_PLATFORM
