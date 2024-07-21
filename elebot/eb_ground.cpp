#include "bg/bg_pmove_simulation.hpp"

#include "cg/cg_local.hpp"
#include "cg/cg_offsets.hpp"
#include "cg/cg_client.hpp"

#include "eb_ground.hpp"
#include "eb_main.hpp"

#include "utils/functions.hpp"

//the debug module cannot access the movement recorder really so....
#if(DEBUG_SUPPORT)
#include "_Modules/aMovementRecorder/movement_recorder/mr_main.hpp"
#include "_Modules/aMovementRecorder/movement_recorder/mr_playback.hpp"
#endif
#include <com/com_channel.hpp>
#include <iostream>

CGroundElebot::CGroundElebot(const playerState_s* ps, axis_t axis, float targetPosition) 
	: CElebotBase(ps, axis, targetPosition)
{

}

CGroundElebot::~CGroundElebot() = default;

bool CGroundElebot::Update(const playerState_s* ps, usercmd_s* cmd, [[maybe_unused]] usercmd_s* oldcmd)
{
	if (HasFinished(ps))
		return false;

	if (CanSprint(ps)) {
		Sprint(ps, cmd);
		return true;
	} 
	
	if ((ps->pm_flags & PMF_SPRINTING) != 0) {
		StopSprinting(cmd);
		return true;
	}

	if (CanWalk(ps, cmd, oldcmd)) {
		Walk(ps, cmd);
		return true;
	}

	if (CanStepForward(ps, cmd, oldcmd)) {
		StepForward(ps, cmd);
		return true;
	}

	if (CanStepSideways(ps, cmd, oldcmd)) {
		StepSideways(ps, cmd);
		return true;
	}

#if(WORSE_IMPLEMENTATION)
	if (m_fTargetYawDelta <= 0.f) {
		return false;
	}
#endif

	return !HasFinished(ps);
}

bool CGroundElebot::CanSprint(const playerState_s* ps) const noexcept
{
	return ps->viewHeightTarget == 60 && GetRemainingDistance() > CG_GetSpeed(ps);
}
void CGroundElebot::Sprint(const playerState_s* ps, const usercmd_s* cmd)
{
	usercmd_s ccmd = *cmd;
	ccmd.forwardmove = 127;
	ccmd.buttons |= cmdEnums::sprint;
	ccmd.angles[YAW] = ANGLE2SHORT(AngleDelta(m_fTargetYaw, ps->delta_angles[YAW]));
	EmplacePlaybackCommand(ps, &ccmd);
}
void CGroundElebot::StopSprinting(usercmd_s* cmd) const noexcept
{
	cmd->forwardmove = 0;
	cmd->buttons &= ~cmdEnums::sprint;	
}

bool CGroundElebot::CanWalk(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) noexcept
{
	if (m_bWalkingIsTooDangerous)
		return false;

	playerState_s local_ps = *ps;

	usercmd_s ccmd = *cmd;
	ccmd.forwardmove = 127;

	//go two steps into the future and see where we end up
	//two is safer than one, because we can't always trust prediction to be fully accurate!
	const auto cmds = CPmoveSimulation::PredictStopPositionAfterNumRepetitions(&local_ps, &ccmd, oldcmd, ELEBOT_FPS, 2u);
	const auto predictedPosition = cmds.back().origin[m_iAxis];
	
	m_bWalkingIsTooDangerous = IsPointTooFar(predictedPosition);

	return !m_bWalkingIsTooDangerous;
}
void CGroundElebot::Walk(const playerState_s* ps, usercmd_s* cmd)
{
	usercmd_s ccmd = *cmd;
	ccmd.forwardmove = 127;
	ccmd.angles[YAW] = ANGLE2SHORT(AngleDelta(m_fTargetYaw, ps->delta_angles[YAW]));
	EmplacePlaybackCommand(ps, &ccmd);
}
bool CGroundElebot::CanStepForward(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) noexcept
{
	//only step when the player doesn't have any velocity
	if (m_bSteppingForwardIsTooDangerous || std::fabsf(ps->velocity[m_iAxis]) != 0.f)
		return false;

	playerState_s local_ps = *ps;

	usercmd_s ccmd = *cmd;
	ccmd.forwardmove = 127;
	ccmd.rightmove = 0;
	ccmd.angles[YAW] = ANGLE2SHORT(AngleDelta(m_fTargetYaw, ps->delta_angles[YAW]));

	const auto cmds = CPmoveSimulation::PredictStopPositionAfterNumRepetitions(&local_ps, &ccmd, oldcmd, ELEBOT_FPS, 1u);
	const auto predictedPosition = cmds.back().origin[m_iAxis];

	m_bSteppingForwardIsTooDangerous = IsPointTooFar(predictedPosition);

	return !m_bSteppingForwardIsTooDangerous;
}
void CGroundElebot::StepForward(const playerState_s* ps, usercmd_s* cmd)
{
	usercmd_s ccmd = *cmd;
	ccmd.forwardmove = 127;
	ccmd.rightmove = 0;
	ccmd.angles[YAW] = ANGLE2SHORT(AngleDelta(m_fTargetYaw, ps->delta_angles[YAW]));
	EmplacePlaybackCommand(ps, &ccmd);
}

#if(WORSE_IMPLEMENTATION)
bool CGroundElebot::CanStepSideways(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) noexcept
{
	//only step when the player doesn't have any velocity
	if (std::fabsf(ps->velocity[m_iAxis]) != 0.f)
		return false;

	playerState_s local_ps = *ps;

	usercmd_s ccmd = *cmd;
	ccmd.forwardmove = 0;
	ccmd.rightmove = -127;
	ccmd.angles[YAW] = ANGLE2SHORT(AngleDelta(m_fTargetYaw - m_fTargetYawDelta, ps->delta_angles[YAW]));

	const auto cmds = CPmoveSimulation::PredictStopPositionAfterNumRepetitions(&local_ps, &ccmd, oldcmd, ELEBOT_FPS, 1u);
	const auto predictedPosition = cmds.back().origin[m_iAxis];

	const bool tooFar = IsPointTooFar(predictedPosition);

	if (tooFar) {

		//stepped too far, lower the delta
		m_fTargetYawDelta -= 0.5f;
		return false;
	}

	return true;

}
#else
bool CGroundElebot::CanStepSideways(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) noexcept
{
	//only step when the player doesn't have any velocity
	if (std::fabsf(ps->velocity[m_iAxis]) != 0.f)
		return false;

	playerState_s local_ps = *ps;

	//start at 90 degrees, keep halving if overstepping
	m_fTargetYawDelta = 90.f;

	while (true) {

		local_ps = *ps;

		usercmd_s ccmd = *cmd;
		ccmd.forwardmove = 0;
		ccmd.rightmove = -127;
		ccmd.angles[YAW] = ANGLE2SHORT(AngleDelta(m_fTargetYaw - m_fTargetYawDelta, ps->delta_angles[YAW]));

		const auto cmds = CPmoveSimulation::PredictStopPositionAfterNumRepetitions(&local_ps, &ccmd, oldcmd, ELEBOT_FPS, 1u);
		const auto predictedPosition = cmds.back().origin[m_iAxis];

		const bool tooFar = IsPointTooFar(predictedPosition);

		if (tooFar) {

			//stepped too far, lower the delta
			if (GetPreviousRepresentableValue(m_fTargetYawDelta) != 0.000000f)
				m_fTargetYawDelta /= 2;
			else //this should never happen
				return false;

			continue;
		}

		break;


	}

	return true;

}
#endif
void CGroundElebot::StepSideways(const playerState_s* ps, usercmd_s* cmd)
{
	usercmd_s ccmd = *cmd;
	ccmd.forwardmove = 0;
	ccmd.rightmove = -127;
	ccmd.angles[YAW] = ANGLE2SHORT(AngleDelta(m_fTargetYaw - m_fTargetYawDelta, ps->delta_angles[YAW]));
	EmplacePlaybackCommand(ps, &ccmd);
}

