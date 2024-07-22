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

using FailCondition = CPmoveSimulation::StopPositionInput_t::FailCondition_e;


CGroundElebot::CGroundElebot(const playerState_s* ps, axis_t axis, float targetPosition) 
	: CElebotBase(ps, axis, targetPosition)
{

	const auto d = static_cast<float>(CG_RoundAngleToCardinalDirection(ps->viewangles[YAW]));

	//move backwards if we are moving towards an edge
	if (AngularDistance(d, m_fTargetYaw) == 180) {
		m_cForwardMove *= -1;
		m_fTargetYaw = AngleNormalize180(m_fTargetYaw + 180);
	}


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

	//CanWalk returned false presumably because a failing condition was met (airborne)
	//even though walking is still OK
	if (!m_bWalkingIsTooDangerous)
		return !HasFinished(ps);


	if (CanStepLongitudinally(ps, cmd, oldcmd)) {
		StepLongitudinally(ps, cmd);
		return true;
	}

	if (CanStepSideways(ps, cmd, oldcmd)) {
		StepSideways(ps, cmd);
		return true;
	}

	return !HasFinished(ps);
}

bool CGroundElebot::CanSprint(const playerState_s* ps) const noexcept
{
	return ps->viewHeightTarget == 60 && GetRemainingDistance() > CG_GetSpeed(ps);
}
void CGroundElebot::Sprint(const playerState_s* ps, const usercmd_s* cmd)
{
	usercmd_s ccmd = *cmd;
	ccmd.forwardmove = m_cForwardMove;
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
	ccmd.forwardmove = m_cForwardMove;

	//go two steps into the future and see where we end up
	//two is safer than one, because we can't always trust prediction to be fully accurate!
	const auto cmds = CPmoveSimulation::PredictStopPositionAdvanced(&local_ps, &ccmd, oldcmd, {
		.iFPS = ELEBOT_FPS,
		.uNumRepetitions = 2u,
		.eFailCondition = FailCondition::airmove
		});

	if (!cmds) {
		m_bWalkingIsTooDangerous = true;
		return false;
	}
	const auto predictedPosition = cmds.value().back().origin[m_iAxis];
	
	m_bWalkingIsTooDangerous = IsPointTooFar(predictedPosition);
	return !m_bWalkingIsTooDangerous;
}
void CGroundElebot::Walk(const playerState_s* ps, usercmd_s* cmd)
{
	usercmd_s ccmd = *cmd;
	ccmd.forwardmove = m_cForwardMove;
	ccmd.angles[YAW] = ANGLE2SHORT(AngleDelta(m_fTargetYaw, ps->delta_angles[YAW]));
	EmplacePlaybackCommand(ps, &ccmd);
}
bool CGroundElebot::CanStepLongitudinally(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) noexcept
{
	//only step when the player doesn't have any velocity
	//and only execute this when walking is no longer an option
	//because otherwise this will take AGES
	if ((m_bSteppingLongitudinallyIsTooDangerous || std::fabsf(ps->velocity[m_iAxis]) != 0.f))
		return false;

	playerState_s local_ps = *ps;

	usercmd_s ccmd = *cmd;
	ccmd.forwardmove = m_cForwardMove;
	ccmd.rightmove = 0;
	ccmd.angles[YAW] = ANGLE2SHORT(AngleDelta(m_fTargetYaw, ps->delta_angles[YAW]));

	const auto cmds = CPmoveSimulation::PredictStopPositionAdvanced(&local_ps, &ccmd, oldcmd, {
		.iFPS = ELEBOT_FPS,
		.uNumRepetitions = 1u,
		.eFailCondition = FailCondition::airmove
		});

	if (!cmds) {
		m_bSteppingLongitudinallyIsTooDangerous = true;
		return false;
	}

	const auto predictedPosition = cmds.value().back().origin[m_iAxis];

	m_bSteppingLongitudinallyIsTooDangerous = IsPointTooFar(predictedPosition);
	return !m_bSteppingLongitudinallyIsTooDangerous;
}
void CGroundElebot::StepLongitudinally(const playerState_s* ps, usercmd_s* cmd)
{
	usercmd_s ccmd = *cmd;
	ccmd.forwardmove = m_cForwardMove;
	ccmd.rightmove = 0;
	ccmd.angles[YAW] = ANGLE2SHORT(AngleDelta(m_fTargetYaw, ps->delta_angles[YAW]));
	ccmd.buttons |= (IsMovingBackwards() ? cmdEnums::crouch : 0);
	EmplacePlaybackCommand(ps, &ccmd);
}


constexpr float CGroundElebot::GetTargetYawForSidewaysMovement() const noexcept
{
	return !IsMovingBackwards() ?
		((m_cRightmove < 0) ? m_fTargetYaw - m_fTargetYawDelta : m_fTargetYaw + m_fTargetYawDelta) :
		(m_cRightmove > 0) ? m_fTargetYaw - m_fTargetYawDelta : m_fTargetYaw + m_fTargetYawDelta;

}
bool CGroundElebot::CanStepSideways(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) noexcept
{
	//only step when the player doesn't have any velocity
	//and only when stepping longitudinally is no longer an option
	//because otherwise this will take AGES
	if (!m_bSteppingLongitudinallyIsTooDangerous || std::fabsf(ps->velocity[m_iAxis]) != 0.f)
		return false;

	playerState_s local_ps = *ps;

	//start at 90 degrees, keep halving if overstepping
	m_fTargetYawDelta = 90.f;

	while (!WASD_PRESSED()) {

		const auto yaw = GetTargetYawForSidewaysMovement();

		local_ps = *ps;

		usercmd_s ccmd = *cmd;
		ccmd.forwardmove = 0;
		ccmd.rightmove = m_cRightmove;
		ccmd.angles[YAW] = ANGLE2SHORT(AngleDelta(yaw, ps->delta_angles[YAW]));

		const auto absTarget = std::fabsf(m_fTargetPosition);
		const auto minAllowedDelta = std::fabsf(absTarget - GetNextRepresentableValue(absTarget));

		const auto cmds = CPmoveSimulation::PredictStopPositionAdvanced(&local_ps, &ccmd, oldcmd, {
			.iFPS = ELEBOT_FPS,
			.uNumRepetitions = 1u,
			.eFailCondition = FailCondition::airmove
			});

		if (!cmds) {
			//for whatever reason the player went airborne
			if (GetPreviousRepresentableValue(m_fTargetYawDelta) > (minAllowedDelta))
				m_fTargetYawDelta /= 2;

			continue;
		}

		const auto predictedPosition = cmds.value().back().origin[m_iAxis];
		const bool tooFar = IsPointTooFar(predictedPosition);

		if (!tooFar)
			break;

		//stepped too far, lower the delta
		if (GetPreviousRepresentableValue(m_fTargetYawDelta) > (minAllowedDelta))
			m_fTargetYawDelta /= 2;

	}

	return true;

}

void CGroundElebot::StepSideways(const playerState_s* ps, usercmd_s* cmd)
{
	usercmd_s ccmd = *cmd;
	ccmd.forwardmove = 0;
	ccmd.rightmove = m_cRightmove;
	ccmd.buttons |= (IsMovingBackwards() ? cmdEnums::crouch : 0);

	const auto yaw = GetTargetYawForSidewaysMovement();

	ccmd.angles[YAW] = ANGLE2SHORT(AngleDelta(yaw, ps->delta_angles[YAW]));
	EmplacePlaybackCommand(ps, &ccmd);
}

