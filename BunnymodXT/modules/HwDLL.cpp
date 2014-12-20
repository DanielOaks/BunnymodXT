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
}

void HwDLL::Unhook()
{
	Clear();
}

void HwDLL::Clear()
{
	ORIG_Cbuf_Execute = nullptr;
}

void __cdecl HwDLL::HOOKED_Cbuf_Execute_Func()
{
	if (clientDLL.pEngfuncs)
		clientDLL.pEngfuncs->Con_Printf("Cbuf_Execute();\n");

	return ORIG_Cbuf_Execute();
}
