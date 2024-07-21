//#include "cg_hooks.hpp"
#include "cg_init.hpp"

#include "cl/cl_move.hpp"
#include "utils/hook.hpp"

#include "bg/bg_pmove.hpp"
#include "cg/cg_local.hpp"
#include "cg/cg_offsets.hpp"
#include "cl/cl_main.hpp"
#include "cod4x/cod4x.hpp"
#include "r/backend/rb_endscene.hpp"
#include "r/r_drawactive.hpp"
#include "r/gui/r_gui.hpp"

#include "shared/sv_shared.hpp"
#include "wnd/wnd.hpp"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

#if(DEBUG_SUPPORT)
#include <r/gui/r_gui.hpp>
#endif

static void CG_CreateHooks();
void CG_CreatePermaHooks()
{
#if(DEBUG_SUPPORT)
	hooktable::initialize();
#else
	hooktable::initialize(CMain::Shared::GetFunctionOrExit("GetHookTable")->As<std::vector<phookbase>*>()->Call());
#endif

	CG_CreateHooks();
}

void CG_CreateHooks()
{
	hooktable::preserver<void, int>(HOOK_PREFIX("CL_Disconnect"), 0x4696B0, CL_Disconnect);

	if (COD4X::get()) {
		BG_WeaponNames = reinterpret_cast<WeaponDef**>(COD4X::get() + 0x443DDE0);
	}

#if(DEBUG_SUPPORT)
	hooktable::preserver<void>(HOOK_PREFIX("PM_Weapon_FinishWeaponChangeASM"), 0x41A5F7, PM_Weapon_FinishWeaponChangeASM);

	hooktable::preserver<void, usercmd_s*>(HOOK_PREFIX("CL_FinishMove"), 0x463A60u, CL_FinishMove);
	hooktable::preserver<void>(HOOK_PREFIX("CG_DrawActive"), COD4X::get() ? COD4X::get() + 0x5464 : 0x42F7F0, CG_DrawActive);

	hooktable::preserver<void>(HOOK_PREFIX("R_RecoverLostDevice"), 0x5F5360, R_RecoverLostDevice);
	hooktable::preserver<void>(HOOK_PREFIX("CL_ShutdownRenderer"), 0x46CA40, CL_ShutdownRenderer);
	hooktable::preserver<LRESULT, HWND, UINT, WPARAM, LPARAM>(HOOK_PREFIX("WndProc"), COD4X::get() ? COD4X::get() + 0x801D6 : 0x57BB20, WndProc);

	hooktable::preserver<void>(HOOK_PREFIX("CG_Cleanup"), 0x477210, CG_Cleanup);
	//hooktable::preserver<void, GfxViewParms*>(HOOK_PREFIX("RB_DrawDebug"), 0x658860, RB_DrawDebug);

	//hooktable::preserver<void>(HOOK_PREFIX("ClientThink_realASM"), 0x4A8500, ClientThink_realASM);
	//hooktable::preserver<void>(HOOK_PREFIX("Pmove"), 0x414D10, Pmove);
	//hooktable::preserver<void>(HOOK_PREFIX("SetClientViewAngleASM"), 0x4AA550, SetClientViewAngleASM);


	if (COD4X::get()) {
		hooktable::preserver<int, msg_t*>(HOOK_PREFIX("MSG_ParseServerCommand"), COD4X::get() + 0x12D6B, COD4X::MSG_ParseServerCommand);
	}

	if (dx && dx->device)
		hooktable::preserver<long, IDirect3DDevice9*>(HOOK_PREFIX("R_EndScene"),
			reinterpret_cast<size_t>((*reinterpret_cast<PVOID**>(dx->device))[42]), R_EndScene);

#endif

}