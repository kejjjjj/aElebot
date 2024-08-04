#include "bg/bg_pmove_simulation.hpp"
#include "cg/cg_local.hpp"
#include "cg/cg_offsets.hpp"
#include "cl/cl_utils.hpp"
#include "cl_move.hpp"
#include "com/com_channel.hpp"
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

	if (ps->pm_type == PM_NORMAL && CStaticElebot::Instance) {
		try {
			if (!CStaticElebot::Instance->Update(ps, cmd, oldcmd))
				CStaticElebot::Instance.reset();

		}
		catch (...) {
			Com_Printf("^1internal error lol\n");
			CStaticElebot::Instance.reset();

		}
	}
#if(DEBUG_SUPPORT)
	CStaticMovementRecorder::Instance->Update(ps, cmd, oldcmd);
#endif

	return;
}