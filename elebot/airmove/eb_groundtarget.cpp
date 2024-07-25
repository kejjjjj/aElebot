#include "eb_groundtarget.hpp"

#include "bg/bg_pmove_simulation.hpp"

#include "cg/cg_local.hpp"

#include "utils/functions.hpp"

#include <windows.h>
#include <algorithm>
#include <ranges>

using FailCondition = CPmoveSimulation::StopPositionInput_t::FailCondition_e;
using TargetAxes = CPmoveSimulation::StopPositionInput_t::TargetAxes_e;

CElebotGroundTarget::CElebotGroundTarget(CElebotBase& base) 
	: CAirElebotVariation(base) 
{

}
CElebotGroundTarget::~CElebotGroundTarget() = default;

bool CElebotGroundTarget::Update([[maybe_unused]] const playerState_s* ps, [[maybe_unused]] usercmd_s* cmd, [[maybe_unused]] usercmd_s* oldcmd)
{
	//it might give more airtime when crouched (hitting head)
	cmd->buttons |= cmdEnums::crouch;

	//todo: add movement corrections when distance > 2.f
	//^^^^^ will do after I find a use case for it ^^^^^

	if (CanStep(ps, cmd, oldcmd)) {
		Step(ps, cmd);
		return true;
	}



	return m_oRefBase.HasFinished(ps) == false;
}

bool CElebotGroundTarget::CanStep(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const
{
	auto& base = m_oRefBase;

	//todo: fix the magic number to something that works with all g_speed values
	if (base.GetRemainingDistance() > 2.f || std::fabsf(ps->velocity[base.m_iAxis]) != 0.f)
		return false;


	//limit the amount so that fps doesn't become an issue (this should never be the case, but just to be sure)
	constexpr auto MAX_ITERATIONS = ELEBOT_FPS;
	for([[maybe_unused]]const auto i : std::views::iota(0, MAX_ITERATIONS)){
		
		if (WASD_PRESSED())
			break;

		playerState_s local_ps = *ps;

		usercmd_s ccmd = *cmd;
		ccmd.forwardmove = 0;
		ccmd.rightmove = 127;

		ccmd.angles[YAW] = ANGLE2SHORT(AngleDelta(base.m_fTargetYaw + base.m_fTargetYawDelta, ps->delta_angles[YAW]));

		const auto cmds = CPmoveSimulation::PredictStopPositionAdvanced(&local_ps, &ccmd, oldcmd, {
				.iFPS = ELEBOT_FPS,
				.uNumRepetitions = 1u,
				.eFailCondition = FailCondition::groundmove,
				.eTargetAxes = (base.m_iAxis == X ? TargetAxes::x : TargetAxes::y),
			});

		const auto absTarget = std::fabsf(base.m_fTargetPosition);
		const auto minAllowedDelta = std::fabsf(absTarget - GetNextRepresentableValue(absTarget));


		//we hit the ground (it's joever now)
		if (!cmds) {
			return false;
		}


		const auto predictedPosition = cmds.value().back().origin[base.m_iAxis];
		const bool tooFar = base.IsPointTooFar(predictedPosition);

		if (predictedPosition == ps->origin[base.m_iAxis]) {
			base.m_fTargetYawDelta = 45.f;
			return false;
		}

		if (!tooFar)
			return true;


		//stepped too far, lower the delta
		if (GetPreviousRepresentableValue(base.m_fTargetYawDelta) > (minAllowedDelta))
			base.m_fTargetYawDelta /= 2;

	}

	return false;

}
void CElebotGroundTarget::Step(const playerState_s* ps, usercmd_s* cmd)
{
	auto& base = m_oRefBase;
	usercmd_s ccmd = *cmd;
	ccmd.forwardmove = 0;
	ccmd.rightmove = 127;

	const auto yaw = base.m_fTargetYaw + base.m_fTargetYawDelta;

	ccmd.angles[YAW] = ANGLE2SHORT(AngleDelta(yaw, ps->delta_angles[YAW]));
	base.EmplacePlaybackCommand(ps, &ccmd);
}
