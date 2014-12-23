#include "../stdafx.hpp"

#include "../sptlib-wrapper.hpp"
#include <SPTLib/MemUtils.hpp>
#include <SPTLib/Hooks.hpp>
#include "../modules.hpp"
#include "../patterns.hpp"
#include "../cvars.hpp"

void __cdecl ServerDLL::HOOKED_PM_Jump()
{
	return serverDLL.HOOKED_PM_Jump_Func();
}

void __cdecl ServerDLL::HOOKED_PM_PreventMegaBunnyJumping()
{
	return serverDLL.HOOKED_PM_PreventMegaBunnyJumping_Func();
}

void __cdecl ServerDLL::HOOKED_PM_PlayerMove(qboolean server)
{
	return serverDLL.HOOKED_PM_PlayerMove_Func(server);
}

void __stdcall ServerDLL::HOOKED_GiveFnptrsToDll(enginefuncs_t* pEngfuncsFromEngine, const void* pGlobals)
{
	return serverDLL.HOOKED_GiveFnptrsToDll_Func(pEngfuncsFromEngine, pGlobals);
}

edict_t* __cdecl ServerDLL::HOOKED_CmdStart(const edict_t* player, const usercmd_s* cmd, int random_seed)
{
	return serverDLL.HOOKED_CmdStart_Func(player, cmd, random_seed);
}

float __cdecl ServerDLL::HOOKED_UTIL_SharedRandomFloat(unsigned int seed, float low, float high)
{
	return serverDLL.HOOKED_UTIL_SharedRandomFloat_Func(seed, low, high);
}

// Linux hooks.
#ifndef _WIN32
extern "C" edict_t* __cdecl _Z8CmdStartPK7edict_sPK9usercmd_sj(const edict_t* player, const usercmd_s* cmd, int random_seed)
{
	return ServerDLL::HOOKED_CmdStart(player, cmd, random_seed);
}
#endif

void ServerDLL::Hook(const std::wstring& moduleName, void* moduleHandle, void* moduleBase, size_t moduleLength, bool needToIntercept)
{
	Clear(); // Just in case.

	m_Handle = moduleHandle;
	m_Base = moduleBase;
	m_Length = moduleLength;
	m_Name = moduleName;
	m_Intercepted = needToIntercept;

	MemUtils::ptnvec_size ptnNumber;

	void *pPMJump, *pPMPreventMegaBunnyJumping, *pPMPlayerMove;

	std::future<MemUtils::ptnvec_size> fPMPreventMegaBunnyJumping, fPMPlayerMove;

	pPMPreventMegaBunnyJumping = MemUtils::GetSymbolAddress(moduleHandle, "PM_PreventMegaBunnyJumping");
	if (pPMPreventMegaBunnyJumping)
	{
		ORIG_PM_PreventMegaBunnyJumping = reinterpret_cast<_PM_PreventMegaBunnyJumping>(pPMPreventMegaBunnyJumping);
		EngineDevMsg("[server dll] Found PM_PreventMegaBunnyJumping at %p.\n", pPMPreventMegaBunnyJumping);
	}
	else
		fPMPreventMegaBunnyJumping = std::async(std::launch::async, MemUtils::FindUniqueSequence, moduleBase, moduleLength, Patterns::ptnsPMPreventMegaBunnyJumping, &pPMPreventMegaBunnyJumping);

	pPMPlayerMove = MemUtils::GetSymbolAddress(moduleHandle, "PM_PlayerMove");
	if (pPMPlayerMove)
	{
		ORIG_PM_PlayerMove = reinterpret_cast<_PM_PlayerMove>(pPMPlayerMove);
		EngineDevMsg("[server dll] Found PM_PlayerMove at %p.\n", pPMPlayerMove);
		offPlayerIndex = 0;
		offVelocity = 92;
		offOrigin = 56;
		offAngles = 68;
	}
	else
		fPMPlayerMove = std::async(std::launch::async, MemUtils::FindUniqueSequence, moduleBase, moduleLength, Patterns::ptnsPMPlayerMove, &pPMPlayerMove);

	pPMJump = MemUtils::GetSymbolAddress(moduleHandle, "PM_Jump");
	if (pPMJump)
	{
		if (*reinterpret_cast<byte*>(pPMJump) == 0xA1)
		{
			ORIG_PM_Jump = reinterpret_cast<_PM_Jump>(pPMJump);
			EngineDevMsg("[server dll] Found PM_Jump at %p.\n", pPMJump);
			ppmove = *reinterpret_cast<void***>(reinterpret_cast<uintptr_t>(pPMJump) + 1); // Linux
			offPlayerIndex = 0;
			offOldbuttons = 200;
			offOnground = 224;

			void *bhopcapAddr;
			ptnNumber = MemUtils::FindUniqueSequence(moduleBase, moduleLength, Patterns::ptnsBhopcap, &bhopcapAddr);
			if (ptnNumber != MemUtils::INVALID_SEQUENCE_INDEX)
			{
				EngineDevMsg("Found the bhopcap pattern at %p.\n", bhopcapAddr);
				offBhopcap = reinterpret_cast<ptrdiff_t>(bhopcapAddr) - reinterpret_cast<ptrdiff_t>(pPMJump) + 27;
				memcpy(originalBhopcapInsn, reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(bhopcapAddr) + 27), sizeof(originalBhopcapInsn));
			}
		}
		else
			pPMJump = nullptr; // Try pattern searching.
	}
	
	if (!pPMJump)
	{
		ptnNumber = MemUtils::FindUniqueSequence(moduleBase, moduleLength, Patterns::ptnsPMJump, &pPMJump);
		if (ptnNumber != MemUtils::INVALID_SEQUENCE_INDEX)
		{
			ORIG_PM_Jump = reinterpret_cast<_PM_Jump>(pPMJump);
			EngineDevMsg("[server dll] Found PM_Jump at %p (using the %s pattern).\n", pPMJump, Patterns::ptnsPMJump[ptnNumber].build.c_str());

			switch (ptnNumber)
			{
			case 0:
			case 1:
				ppmove = *reinterpret_cast<void***>(reinterpret_cast<uintptr_t>(pPMJump) + 2);
				offPlayerIndex = 0;
				offOldbuttons = 200;
				offOnground = 224;
				break;

			case 2:
			case 3: // AG-Client, shouldn't happen here but who knows.
				ppmove = *reinterpret_cast<void***>(reinterpret_cast<uintptr_t>(pPMJump) + 3);
				offPlayerIndex = 0;
				offOldbuttons = 200;
				offOnground = 224;
				break;
			}
		}
		else
		{
			EngineDevWarning("[server dll] Could not find PM_Jump!\n");
			EngineWarning("Autojump is not available.\n");
		}
	}

	if (!ORIG_PM_PreventMegaBunnyJumping)
	{
		ptnNumber = fPMPreventMegaBunnyJumping.get();
		if (ptnNumber != MemUtils::INVALID_SEQUENCE_INDEX)
		{
			ORIG_PM_PreventMegaBunnyJumping = reinterpret_cast<_PM_PreventMegaBunnyJumping>(pPMPreventMegaBunnyJumping);
			EngineDevMsg("[server dll] Found PM_PreventMegaBunnyJumping at %p (using the %s pattern).\n", pPMPreventMegaBunnyJumping, Patterns::ptnsPMPreventMegaBunnyJumping[ptnNumber].build.c_str());
		}
		else
		{
			EngineDevWarning("[server dll] Could not find PM_PreventMegaBunnyJumping!\n");
			EngineWarning("Bhopcap disabling is not available.\n");
		}
	}

	if (!ORIG_PM_PlayerMove)
	{
		ptnNumber = fPMPlayerMove.get();
		if (ptnNumber != MemUtils::INVALID_SEQUENCE_INDEX)
		{
			ORIG_PM_PlayerMove = reinterpret_cast<_PM_PlayerMove>(pPMPlayerMove);
			EngineDevMsg("[server dll] Found PM_PlayerMove at %p (using the %s pattern).\n", pPMPlayerMove, Patterns::ptnsPMPlayerMove[ptnNumber].build.c_str());

			switch (ptnNumber)
			{
			case 0:
				offPlayerIndex = 0;
				offVelocity = 92;
				offOrigin = 56;
				offAngles = 68;
				break;
			}
		}
		else
		{
			EngineDevWarning("[server dll] Could not find PM_PlayerMove!\n");
		}
	}

	// This has to be the last thing to check and hook.
	pEngfuncs = reinterpret_cast<enginefuncs_t*>(MemUtils::GetSymbolAddress(moduleHandle, "g_engfuncs"));
	if (pEngfuncs)
	{
		EngineDevMsg("[server dll] pEngfuncs is %p.\n", pEngfuncs);
		if (*reinterpret_cast<uintptr_t*>(pEngfuncs))
			RegisterCVarsAndCommands();
		else
		{
			ORIG_GiveFnptrsToDll = reinterpret_cast<_GiveFnptrsToDll>(MemUtils::GetSymbolAddress(moduleHandle, "GiveFnptrsToDll"));
			if (!ORIG_GiveFnptrsToDll)
			{
				EngineDevWarning("[server dll] Couldn't get the address of GiveFnptrsToDll!\n");
				EngineWarning("Serverside CVars and commands are not available.\n");
			}
		}
	}
	else
	{
		_GiveFnptrsToDll pGiveFnptrsToDll = reinterpret_cast<_GiveFnptrsToDll>(MemUtils::GetSymbolAddress(moduleHandle, "GiveFnptrsToDll"));
		if (pGiveFnptrsToDll)
		{
			// Find "mov edi, offset dword; rep movsd" inside GiveFnptrsToDll. The pointer to g_engfuncs is that dword.
			const byte pattern[] = { 0xBF, '?', '?', '?', '?', 0xF3, 0xA5 };
			auto addr = MemUtils::FindPattern(reinterpret_cast<void*>(pGiveFnptrsToDll), 40, pattern, "x????xx");

			// Linux version: mov offset dword[eax], esi; mov [ecx+eax+4], ebx
			if (!addr)
			{
				const byte pattern_[] = { 0x89, 0xB0, '?', '?', '?', '?', 0x89, 0x5C, 0x01, 0x04 };
				addr = MemUtils::FindPattern(reinterpret_cast<void*>(pGiveFnptrsToDll), 40, pattern_, "xx????xxxx");
				if (addr)
					addr = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(addr) + 1); // So we're compatible with the previous pattern.
			}

			if (addr)
			{
				pEngfuncs = *reinterpret_cast<enginefuncs_t**>(reinterpret_cast<uintptr_t>(addr) + 1);
				EngineDevMsg("[server dll] pEngfuncs is %p.\n", pEngfuncs);

				// If we have engfuncs, do stuff right away. Otherwise wait till the engine gives us engfuncs.
				if (*reinterpret_cast<uintptr_t*>(pEngfuncs))
					RegisterCVarsAndCommands();
				else
					ORIG_GiveFnptrsToDll = pGiveFnptrsToDll;
			}
			else
			{
				EngineDevWarning("[server dll] Couldn't find the pattern in GiveFnptrsToDll!\n");
				EngineWarning("Serverside CVars and commands are not available.\n");
			}
		}
		else
		{
			EngineDevWarning("[server dll] Couldn't get the address of GiveFnptrsToDll!\n");
			EngineWarning("Serverside CVars and commands are not available.\n");
		}
	}

	ORIG_CmdStart = reinterpret_cast<_CmdStart>(MemUtils::GetSymbolAddress(moduleHandle, "_Z8CmdStartPK7edict_sPK9usercmd_sj"));
	if (ORIG_CmdStart)
	{
		EngineDevMsg("[server dll] CmdStart is located at %p.\n", ORIG_CmdStart);
	}
	else
	{
		EngineDevWarning("[server dll] Couldn't get the address of CmdStart!\n");
	}

	ORIG_UTIL_SharedRandomFloat = reinterpret_cast<_UTIL_SharedRandomFloat>(MemUtils::GetSymbolAddress(moduleHandle, "_Z22UTIL_SharedRandomFloatjff"));
	if (ORIG_UTIL_SharedRandomFloat)
	{
		EngineDevMsg("[server dll] UTIL_SharedRandomFloat is located at %p.\n", ORIG_UTIL_SharedRandomFloat);
	}
	else
	{
		EngineDevWarning("[server dll] Couldn't get the address of UTIL_SharedRandomFloat!\n");
	}

	MemUtils::AddSymbolLookupHook(moduleHandle, reinterpret_cast<void*>(ORIG_GiveFnptrsToDll), reinterpret_cast<void*>(HOOKED_GiveFnptrsToDll));

	if (needToIntercept)
		MemUtils::Intercept(moduleName, {
			{ reinterpret_cast<void**>(&ORIG_PM_Jump), reinterpret_cast<void*>(HOOKED_PM_Jump) },
			{ reinterpret_cast<void**>(&ORIG_PM_PreventMegaBunnyJumping), reinterpret_cast<void*>(HOOKED_PM_PreventMegaBunnyJumping) },
			{ reinterpret_cast<void**>(&ORIG_PM_PlayerMove), reinterpret_cast<void*>(HOOKED_PM_PlayerMove) },
			{ reinterpret_cast<void**>(&ORIG_GiveFnptrsToDll), reinterpret_cast<void*>(HOOKED_GiveFnptrsToDll) },
			{ reinterpret_cast<void**>(&ORIG_CmdStart), reinterpret_cast<void*>(HOOKED_CmdStart) }
		});
}

void ServerDLL::Unhook()
{
	if (m_Intercepted)
		MemUtils::RemoveInterception(m_Name, {
			{ reinterpret_cast<void**>(&ORIG_PM_Jump), reinterpret_cast<void*>(HOOKED_PM_Jump) },
			{ reinterpret_cast<void**>(&ORIG_PM_PreventMegaBunnyJumping), reinterpret_cast<void*>(HOOKED_PM_PreventMegaBunnyJumping) },
			{ reinterpret_cast<void**>(&ORIG_PM_PlayerMove), reinterpret_cast<void*>(HOOKED_PM_PlayerMove) },
			{ reinterpret_cast<void**>(&ORIG_GiveFnptrsToDll), reinterpret_cast<void*>(HOOKED_GiveFnptrsToDll) },
			{ reinterpret_cast<void**>(&ORIG_CmdStart), reinterpret_cast<void*>(HOOKED_CmdStart) }
		});

	MemUtils::RemoveSymbolLookupHook(m_Handle, reinterpret_cast<void*>(ORIG_GiveFnptrsToDll));

	Clear();
}

void ServerDLL::Clear()
{
	IHookableDirFilter::Clear();
	ORIG_PM_Jump = nullptr;
	ORIG_PM_PreventMegaBunnyJumping = nullptr;
	ORIG_PM_PlayerMove = nullptr;
	ORIG_GiveFnptrsToDll = nullptr;
	ORIG_CmdStart = nullptr;
	ORIG_UTIL_SharedRandomFloat = nullptr;
	ppmove = nullptr;
	offPlayerIndex = 0;
	offOldbuttons = 0;
	offOnground = 0;
	offVelocity = 0;
	offOrigin = 0;
	offAngles = 0;
	offBhopcap = 0;
	memset(originalBhopcapInsn, 0, sizeof(originalBhopcapInsn));
	pEngfuncs = nullptr;
	cantJumpNextTime.clear();
	m_Intercepted = false;
}

bool ServerDLL::CanHook(const std::wstring& moduleFullName)
{
	if (!IHookableDirFilter::CanHook(moduleFullName))
		return false;

	// Filter out addons like metamod which may be located into a "dlls" folder under addons.
	std::wstring pathToLiblist = moduleFullName.substr(0, moduleFullName.rfind(GetFolderName(moduleFullName))).append(L"liblist.gam");

	// If liblist.gam exists in the parent directory, then we're (hopefully) good.
	struct wrapper {
		wrapper(FILE* f) : file(f) {};
		~wrapper() {
			if (file)
				fclose(file);
		}
		operator FILE*() const
		{
			return file;
		}

		FILE* file;
	} liblist(fopen(Convert(pathToLiblist).c_str(), "r"));

	if (liblist)
		return true;

	return false;
}

void ServerDLL::RegisterCVarsAndCommands()
{
	if (!pEngfuncs || !*reinterpret_cast<uintptr_t*>(pEngfuncs))
		return;

	if (ORIG_PM_Jump)
		pEngfuncs->pfnCVarRegister(bxt_autojump.GetPointer());

	if (ORIG_PM_PreventMegaBunnyJumping)
		pEngfuncs->pfnCVarRegister(bxt_bhopcap.GetPointer());

	if (ORIG_PM_PlayerMove)
		pEngfuncs->pfnCVarRegister(_bxt_taslog.GetPointer());

	pEngfuncs->pfnCVarRegister(_bxt_msec.GetPointer());
	pEngfuncs->pfnCVarRegister(_bxt_lockva.GetPointer());

	EngineDevMsg("[server dll] Registered CVars.\n");
}

void __cdecl ServerDLL::HOOKED_PM_Jump_Func()
{
	auto pmove = reinterpret_cast<uintptr_t>(*ppmove);
	int playerIndex = *reinterpret_cast<int*>(pmove + offPlayerIndex);

	int *onground = reinterpret_cast<int*>(pmove + offOnground);
	int orig_onground = *onground;

	int *oldbuttons = reinterpret_cast<int*>(pmove + offOldbuttons);
	int orig_oldbuttons = *oldbuttons;

	if (bxt_autojump.GetBool())
	{
		if ((orig_onground != -1) && !cantJumpNextTime[playerIndex])
			*oldbuttons &= ~IN_JUMP;
	}

	cantJumpNextTime[playerIndex] = false;

	if (offBhopcap)
	{
		auto pPMJump = reinterpret_cast<ptrdiff_t>(ORIG_PM_Jump);
		if (bxt_bhopcap.GetBool())
		{
			if (*reinterpret_cast<byte*>(pPMJump + offBhopcap) == 0x90
				&& *reinterpret_cast<byte*>(pPMJump + offBhopcap + 1) == 0x90)
				MemUtils::ReplaceBytes(reinterpret_cast<void*>(pPMJump + offBhopcap), 6, originalBhopcapInsn);
		}
		else if (*reinterpret_cast<byte*>(pPMJump + offBhopcap) == 0x0F
				&& *reinterpret_cast<byte*>(pPMJump + offBhopcap + 1) == 0x82)
				MemUtils::ReplaceBytes(reinterpret_cast<void*>(pPMJump + offBhopcap), 6, reinterpret_cast<const byte*>("\x90\x90\x90\x90\x90\x90"));
	}

	ORIG_PM_Jump();

	if ((orig_onground != -1) && (*onground == -1))
		cantJumpNextTime[playerIndex] = true;

	if (bxt_autojump.GetBool())
		*oldbuttons = orig_oldbuttons;
}

void __cdecl ServerDLL::HOOKED_PM_PreventMegaBunnyJumping_Func()
{
	if (bxt_bhopcap.GetBool())
		return ORIG_PM_PreventMegaBunnyJumping();
}

void __cdecl ServerDLL::HOOKED_PM_PlayerMove_Func(qboolean server)
{
	if (!ppmove)
		return ORIG_PM_PlayerMove(server);

	auto pmove = reinterpret_cast<uintptr_t>(*ppmove);

	int playerIndex = *reinterpret_cast<int*>(pmove + offPlayerIndex);

	float *velocity, *origin, *angles;
	velocity = reinterpret_cast<float*>(pmove + offVelocity);
	origin =   reinterpret_cast<float*>(pmove + offOrigin);
	angles =   reinterpret_cast<float*>(pmove + offAngles);

	#define ALERT(at, format, ...) pEngfuncs->pfnAlertMessage(at, const_cast<char*>(format), ##__VA_ARGS__)

	if (_bxt_taslog.GetBool())
	{
		ALERT(at_console, "-- BXT TAS Log Start: Server --\n");
		ALERT(at_console, "Player index: %d; msec: %u (%Lf)\n", playerIndex, reinterpret_cast<usercmd_s*>(pmove + 0x45458)->msec, static_cast<long double>(reinterpret_cast<usercmd_s*>(pmove + 0x45458)->msec) * 0.001);
		ALERT(at_console, "Velocity: %.8f; %.8f; %.8f; origin: %.8f; %.8f; %.8f\n", velocity[0], velocity[1], velocity[2], origin[0], origin[1], origin[2]);
	}

	ORIG_PM_PlayerMove(server);

	if (_bxt_taslog.GetBool())
	{
		ALERT(at_console, "Angles: %.8f; %.8f; %.8f\n", angles[0], angles[1], angles[2]);
		ALERT(at_console, "New velocity: %.8f; %.8f; %.8f; new origin: %.8f; %.8f; %.8f\n", velocity[0], velocity[1], velocity[2], origin[0], origin[1], origin[2]);
		ALERT(at_console, "-- BXT TAS Log End --\n");
	}

	#undef ALERT

	CustomHud::UpdatePlayerInfo(velocity, origin);
}

void __stdcall ServerDLL::HOOKED_GiveFnptrsToDll_Func(enginefuncs_t* pEngfuncsFromEngine, const void* pGlobals)
{
	ORIG_GiveFnptrsToDll(pEngfuncsFromEngine, pGlobals);

	RegisterCVarsAndCommands();
}

edict_t* __cdecl ServerDLL::HOOKED_CmdStart_Func(const edict_t* player, const usercmd_s* cmd, int random_seed)
{
	#define ALERT(at, format, ...) pEngfuncs->pfnAlertMessage(at, const_cast<char*>(format), ##__VA_ARGS__)
	//#define ALERT(at, format, ...) EngineDevMsg(format, ##__VA_ARGS__)
	//if (_bxt_taslog.GetBool())
	{
		ALERT(at_console, "-- CmdStart Start --\n");
		ALERT(at_console, "Lerp_msec %hd; msec %u (%Lf)\n", cmd->lerp_msec, cmd->msec, static_cast<long double>(cmd->msec) * 0.001);
		ALERT(at_console, "Viewangles: %.8f %.8f %.8f; forwardmove: %f; sidemove: %f; upmove: %f\n", cmd->viewangles[0], cmd->viewangles[1], cmd->viewangles[2], cmd->forwardmove, cmd->sidemove, cmd->upmove);
		ALERT(at_console, "Buttons: %hu\n", cmd->buttons);
		ALERT(at_console, "Random seed: %d\n", random_seed);
		ALERT(at_console, "-- CmdStart End --\n");
	}
	#undef ALERT

	return ORIG_CmdStart(player, cmd, random_seed);
}

float __cdecl ServerDLL::HOOKED_UTIL_SharedRandomFloat_Func(unsigned int seed, float low, float high)
{
	auto res = ORIG_UTIL_SharedRandomFloat(seed, low, high);
	#define ALERT(at, format, ...) pEngfuncs->pfnAlertMessage(at, const_cast<char*>(format), ##__VA_ARGS__)
	ALERT(at_console, "UTIL_SharedRandomFloat(%u, %f, %f) => %f\n", seed, low, high, res);
	#undef ALERT
	//EngineDevMsg("UTIL_SharedRandomFloat(%u, %f, %f) => %f\n", seed, low, high, res);
	return res;
}
