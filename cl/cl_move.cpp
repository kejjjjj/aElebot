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

void CL_CreateNewCommands([[maybe_unused]] int localClientNum)
{
	auto ps = &cgs->predictedPlayerState;
	auto cmd = &clients->cmds[clients->cmdNumber & 0x7F];
	auto oldcmd = &clients->cmds[(clients->cmdNumber - 1) & 0x7F];
	auto& elebot = CStaticElebot::Instance;

	if (ps->pm_type != PM_NORMAL)
		return;

	try {

		if (NVar_FindMalleableVar<bool>("Elevate everything")->Get()) {
			CStaticElebot::EB_StartAutomatically(ps, cmd, oldcmd);
		}

		if (elebot && !elebot->Update(ps, cmd, oldcmd)) {

			//yea try again
			if (elebot->TryAgain()) {
				ps->viewangles[YAW] = elebot->GetMove(ps)->m_fInitialYaw;
				elebot = std::make_unique<CElebot>(ps, elebot->m_oInit);

				return;
			}

			if (const auto base = elebot->GetMove(ps)) {
				if (base->HasFinished(ps)) {
					CL_SetPlayerAngles(cmd, ps->delta_angles, { ps->viewangles[PITCH], base->m_fInitialYaw, 0.f });
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
	

	return;

}
