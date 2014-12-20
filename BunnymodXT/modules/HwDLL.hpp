#pragma once

#include "../sptlib-wrapper.hpp"
#include <SPTLib/IHookableNameFilter.hpp>

typedef void(__cdecl *_Cbuf_Execute) ();

class HwDLL : public IHookableNameFilter
{
public:
	HwDLL() : IHookableNameFilter({ L"hw.dll", L"hw.so" }) {};
	virtual void Hook(const std::wstring& moduleName, void* moduleHandle, void* moduleBase, size_t moduleLength, bool needToIntercept);
	virtual void Unhook();
	virtual void Clear();

	static void __cdecl HOOKED_Cbuf_Execute();
	void __cdecl HOOKED_Cbuf_Execute_Func();

protected:
	_Cbuf_Execute ORIG_Cbuf_Execute;
};
