#include "bg/bg_pmove_simulation.hpp"
#include "cg/cg_local.hpp"
#include "cg/cg_offsets.hpp"
#include "cl/cl_utils.hpp"
#include "cl_move.hpp"
#include "com/com_channel.hpp"
#include "net/nvar_table.hpp"
#include "utils/hook.hpp"
#include "utils/typedefs.hpp"

#include "elebot/eb_main.hpp"

#if(DEBUG_SUPPORT)
#include "_Modules/aMovementRecorder/movement_recorder/mr_main.hpp"
#include "_Modules/aMovementRecorder/movement_recorder/mr_playback.hpp"
#endif

void CL_FinishMove(usercmd_s* cmd)
{
#if(DEBUG_SUPPORT)
	hooktable::find<void, usercmd_s*>(HOOK_PREFIX(__func__))->call(cmd);
#endif

	auto ps = &cgs->predictedPlayerState;
	auto oldcmd = CL_GetUserCmd(clients->cmdNumber - 1);

	auto& elebot = CStaticElebot::Instance;

	if (ps->pm_type == PM_NORMAL) {

		try {
			if (NVar_FindMalleableVar<bool>("Elevate everything")->Get()) {
				CStaticElebot::EB_StartAutomatically(ps, cmd, oldcmd);
			}

			if (elebot && !elebot->Update(ps, cmd, oldcmd)) {
				if (const auto base = elebot->GetMove(ps)) {
					if (base->HasFinished(ps)) {
						CL_SetPlayerAngles(cmd, ps->delta_angles, { ps->viewangles[PITCH], base->m_fTargetYaw, ps->viewangles[ROLL] });
					}
				}
				elebot.reset();
			}
		}
		catch (...) {
			Com_Printf("^1internal error lol\n");
			elebot.reset();

		}

#if(DEBUG_SUPPORT)
		CStaticMovementRecorder::Instance->Update(ps, cmd, oldcmd);
#endif

	}

	return;
}