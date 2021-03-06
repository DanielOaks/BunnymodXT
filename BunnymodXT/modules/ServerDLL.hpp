#pragma once

#include <SPTLib/IHookableDirFilter.hpp>

class ServerDLL : public IHookableDirFilter
{
	HOOK_DECL(void, __cdecl, PM_Jump)
	HOOK_DECL(void, __cdecl, PM_PreventMegaBunnyJumping)
	HOOK_DECL(void, __cdecl, PM_PlayerMove, qboolean server)
	HOOK_DECL(void, __stdcall, GiveFnptrsToDll, enginefuncs_t* pEngfuncsFromEngine, const void* pGlobals)

public:
	static ServerDLL& GetInstance()
	{
		static ServerDLL instance;
		return instance;
	}

	virtual void Hook(const std::wstring& moduleName, void* moduleHandle, void* moduleBase, size_t moduleLength, bool needToIntercept);
	virtual void Unhook();
	virtual void Clear();
	virtual bool CanHook(const std::wstring& moduleFullName);

private:
	ServerDLL() : IHookableDirFilter({ L"dlls" }) {};
	ServerDLL(const ServerDLL&);
	void operator=(const ServerDLL&);

protected:
	void RegisterCVarsAndCommands();

	void **ppmove;
	ptrdiff_t offPlayerIndex;
	ptrdiff_t offOldbuttons;
	ptrdiff_t offOnground;
	ptrdiff_t offVelocity;
	ptrdiff_t offOrigin;
	ptrdiff_t offAngles;

	ptrdiff_t offBhopcap;
	byte originalBhopcapInsn[6];

	enginefuncs_t *pEngfuncs;

	std::unordered_map<int, bool> cantJumpNextTime;
};
