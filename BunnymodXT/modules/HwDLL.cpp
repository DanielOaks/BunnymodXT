#include "../stdafx.hpp"

#include "../sptlib-wrapper.hpp"
#include <SPTLib/MemUtils.hpp>
#include <SPTLib/Hooks.hpp>
#include "../modules.hpp"
#include "../patterns.hpp"

void __cdecl HwDLL::HOOKED_Cbuf_Execute()
{
	return hwDLL.HOOKED_Cbuf_Execute_Func();
}

// Linux hooks.
#ifndef _WIN32
extern "C" void __cdecl Cbuf_Execute()
{
	return HwDLL::HOOKED_Cbuf_Execute();
}
#endif

void HwDLL::Hook(const std::wstring& moduleName, void* moduleHandle, void* moduleBase, size_t moduleLength, bool needToIntercept)
{
	Clear(); // Just in case.

	m_Handle = moduleHandle;
	m_Base = moduleBase;
	m_Length = moduleLength;
	m_Name = moduleName;
	m_Intercepted = needToIntercept;

	ORIG_Cbuf_Execute = reinterpret_cast<_Cbuf_Execute>(MemUtils::GetSymbolAddress(moduleHandle, "Cbuf_Execute"));
	if (ORIG_Cbuf_Execute)
	{
		EngineDevMsg("[hw dll] Cbuf_Execute is located at %p.\n", ORIG_Cbuf_Execute);
	}
	else
	{
		EngineDevWarning("[hw dll] Couldn't get the address of Cbuf_Execute!\n");
	}

	ORIG_Cbuf_InsertText = reinterpret_cast<_Cbuf_InsertText>(MemUtils::GetSymbolAddress(moduleHandle, "Cbuf_InsertText"));
	if (ORIG_Cbuf_InsertText)
	{
		EngineDevMsg("[hw dll] Cbuf_InsertText is located at %p.\n", ORIG_Cbuf_InsertText);
	}
	else
	{
		EngineDevWarning("[hw dll] Couldn't get the address of Cbuf_InsertText!\n");
	}

	cls = MemUtils::GetSymbolAddress(moduleHandle, "cls");
	if (cls)
	{
		EngineDevMsg("[hw dll] Cls is located at %p.\n", cls);
	}
	else
	{
		EngineDevWarning("[hw dll] Couldn't get the address of cls!\n");
	}

	sv = MemUtils::GetSymbolAddress(moduleHandle, "sv");
	if (sv)
	{
		EngineDevMsg("[hw dll] Sv is located at %p.\n", sv);
	}
	else
	{
		EngineDevWarning("[hw dll] Couldn't get the address of sv!\n");
	}
}

void HwDLL::Unhook()
{
	Clear();
}

void HwDLL::Clear()
{
	ORIG_Cbuf_Execute = nullptr;
	ORIG_Cbuf_InsertText = nullptr;
	cls = nullptr;
	sv = nullptr;
}

void __cdecl HwDLL::HOOKED_Cbuf_Execute_Func()
{
	static bool dontPauseNextCycle = false;

	int state = *reinterpret_cast<int*>(cls);
	int paused = *(reinterpret_cast<int*>(sv) + 1);

	if (clientDLL.pEngfuncs)
		clientDLL.pEngfuncs->Con_Printf("Cbuf_Execute() begin; cls.state: %d; sv.paused: %d; time: %f\n", state, paused, *reinterpret_cast<double*>(reinterpret_cast<uintptr_t>(sv) + 0xC));

	// If cls.state == 4 and the game isn't paused, execute "pause" right now.
	// This case happens when loading a savegame.
	if (state == 4 && !paused)
		ORIG_Cbuf_InsertText("pause");

	ORIG_Cbuf_Execute();

	// If cls.state == 3 and the game isn't paused, execute "pause" on the next cycle.
	// This case happens when starting a map.
	if (!dontPauseNextCycle && state == 3 && !paused)
	{
		ORIG_Cbuf_InsertText("pause");
		dontPauseNextCycle = true;
	}
	else
		dontPauseNextCycle = false;
}
