#include "stdafx.hpp"

#include "..\sptlib-wrapper.hpp"
#include <SPTLib\memutils.hpp>
#include <SPTLib\detoursutils.hpp>
#include <SPTLib\hooks.hpp>
#include "ServerDLL.hpp"
#include "..\modules.hpp"
#include "..\patterns.hpp"

using std::uintptr_t;
using std::size_t;

void __cdecl ServerDLL::HOOKED_PM_Jump()
{
	return serverDLL.HOOKED_PM_Jump_Func();
}

void ServerDLL::Hook(const std::wstring& moduleName, HMODULE hModule, uintptr_t moduleStart, size_t moduleLength)
{
	Clear(); // Just in case.

	this->hModule = hModule;
	this->moduleStart = moduleStart;
	this->moduleLength = moduleLength;
	this->moduleName = moduleName;

	MemUtils::ptnvec_size ptnNumber;

	uintptr_t pPMJump = NULL;
	ptnNumber = MemUtils::FindUniqueSequence(moduleStart, moduleLength, Patterns::ptnsPMJump, &pPMJump);
	if (ptnNumber != MemUtils::INVALID_SEQUENCE_INDEX)
	{
		ORIG_PM_Jump = (_PM_Jump)pPMJump;
		EngineDevMsg("[server dll] Found PM_Jump at %p (using the build %s pattern).\n", pPMJump, Patterns::ptnsPMJump[ptnNumber].build.c_str());

		switch (ptnNumber)
		{
		case 0:
			ppmove = *(uintptr_t *)(pPMJump + 2);
			offOldbuttons = 200;
			offOnground = 224;
		}
	}
	else
	{
		EngineDevWarning("[server dll] Could not find PM_Jump!\n");
		EngineWarning("y_bxt_autojump has no effect.\n");
	}

	DetoursUtils::AttachDetours(moduleName, {
		{ (PVOID *)(&ORIG_PM_Jump), HOOKED_PM_Jump }
	});
}

void ServerDLL::Unhook()
{
	DetoursUtils::DetachDetours(moduleName, {
		{ (PVOID *)(&ORIG_PM_Jump), HOOKED_PM_Jump }
	});

	Clear();
}

void ServerDLL::Clear()
{
	IHookableDirFilter::Clear();
	ORIG_PM_Jump = nullptr;
	ppmove = 0;
	offOldbuttons = 0;
	offOnground = 0;
	cantJumpNextTime = false;
}

void __cdecl ServerDLL::HOOKED_PM_Jump_Func()
{
	const int IN_JUMP = (1 << 1);

	int *onground = (int *)(*(uintptr_t *)ppmove + offOnground);
	int orig_onground = *onground;

	int *oldbuttons = (int *)(*(uintptr_t *)ppmove + offOldbuttons);
	int orig_oldbuttons = *oldbuttons;

	if ((orig_onground != -1) && !cantJumpNextTime)
		*oldbuttons &= ~IN_JUMP;

	cantJumpNextTime = false;

	ORIG_PM_Jump();

	if ((orig_onground != -1) && (*onground == -1))
		cantJumpNextTime = true;

	*oldbuttons = orig_oldbuttons;
}