#include "../stdafx.hpp"

#include "../modules.hpp"

static bool serverside = false;

extern "C" void __cdecl PM_PlayerMove(qboolean server)
{
	serverside = static_cast<bool>(server);
	if (serverside)
		return ServerDLL::HOOKED_PM_PlayerMove(server);
	else
		return ClientDLL::HOOKED_PM_PlayerMove(server);
}

extern "C" void __cdecl PM_PreventMegaBunnyJumping()
{
	if (serverside)
		return ServerDLL::HOOKED_PM_PreventMegaBunnyJumping();
	else
		return ClientDLL::HOOKED_PM_PreventMegaBunnyJumping();
}

extern "C" void __cdecl PM_Jump()
{
	if (serverside)
		return ServerDLL::HOOKED_PM_Jump();
	else
		return ClientDLL::HOOKED_PM_Jump();
}

extern "C" float __cdecl _Z22UTIL_SharedRandomFloatjff(unsigned int seed, float low, float high)
{
	return ServerDLL::HOOKED_UTIL_SharedRandomFloat(seed, low, high);
}
