#include "eb_groundtarget.hpp"
#include "eb_block.hpp"

#include "bg/bg_pmove.hpp"
#include "bg/bg_pmove_simulation.hpp"

#include "cg/cg_local.hpp"

#include "com/com_channel.hpp"

#include "utils/functions.hpp"

#include <windows.h>
#include <algorithm>
#include <ranges>
#include <cassert>
#include <iostream>
#include <cg/cg_client.hpp>

using FailCondition = CPmoveSimulation::StopPositionInput_t::FailCondition_e;
using TargetAxes = CPmoveSimulation::StopPositionInput_t::TargetAxes_e;

constexpr float CAREFUL_THRESHOLD_DISTANCE = 0.125f;

CElebotGroundTarget::CElebotGroundTarget(CElebotBase& base) 
	: CAirElebotVariation(base) 
{

}
CElebotGroundTarget::~CElebotGroundTarget() = default;

bool CElebotGroundTarget::Update([[maybe_unused]] const playerState_s* ps, [[maybe_unused]] usercmd_s* cmd, [[maybe_unused]] usercmd_s* oldcmd)
{
	auto& base = m_oRefBase;

	if (m_oRefBase.HasFinished(ps))
		return false;

	//if stuck against a wall while falling
	const auto beingClipped = base.IsVelocityBeingClipped(ps, cmd, oldcmd);
	if (m_pBlockerAvoidState || ps->velocity[Z] < 0 && beingClipped) {
		return UpdateBlocker(ps, cmd, oldcmd, reinterpret_cast<BlockFunc>(&CElebotGroundTarget::TryGoingUnderTheBlocker));
	}

	//the code below doesn't work when being clipped..
	if (beingClipped)
		return true;

	if (ResetVelocity(ps, cmd, oldcmd)) {
		if (CanStep(ps, cmd, oldcmd)) {
			Step(ps, cmd);
			return true;
		}
	}

	return m_oRefBase.HasFinished(ps) == false;
}
bool CElebotGroundTarget::UpdateBlocker(const playerState_s* ps, usercmd_s* cmd, usercmd_s* oldcmd, BlockFunc func)
{
	if (!GetBlocker(ps, cmd, oldcmd, func))
		return true;

	assert(m_pBruteForcer != nullptr);
	return m_pBruteForcer->Update(ps, cmd, oldcmd);
}
bool CElebotGroundTarget::GetBlocker(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd, 
	const BlockFunc& func)
{
	if (m_pBlockerAvoidState) {
		return true;
	}

	//see if we can get under the brush 
	m_pBlockerAvoidState = (this->*func)(ps, cmd, oldcmd);

	if (m_pBlockerAvoidState) {
		m_fBlockerMinHeight = m_pBlockerAvoidState->ps->origin[Z];

		//if we are crouched, we have 20 units of space where we are unable to standup
		if (m_pBlockerAvoidState->ps->viewHeightTarget == 40)
			m_fBlockerMinHeight -= 20;

		//this thing should do the trick
		m_pBruteForcer = std::make_unique<CBlockElebot>(m_oRefBase, m_pBlockerAvoidState->ps, 
			&m_pBlockerAvoidState->cmd, &m_pBlockerAvoidState->oldcmd, m_fBlockerMinHeight);

	}

	return m_pBlockerAvoidState != nullptr;
}
std::unique_ptr<pmove_t> CElebotGroundTarget::TryGoingUnderTheBlocker(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const
{
	//at this point we know that we are stuck hugging a wall

	const auto& base = m_oRefBase;
	auto ps_local = *ps;

	auto pm = std::make_unique<pmove_t>(PM_Create(&ps_local, cmd, oldcmd));

	CSimulationController c;

	c.FPS = ELEBOT_FPS;
	c.forwardmove = 0;
	c.rightmove = 0;
	c.weapon = cmd->weapon;
	c.offhand = cmd->offHandIndex;

	//only crouch
	c.buttons = cmdEnums::crouch;

	c.viewangles = CSimulationController::CAngles{
		.viewangles = {
			ps->viewangles[PITCH], 
			base.m_fTargetYaw, 
			0.f
		}, 
		.angle_enum = EViewAngle::FixedAngle, .smoothing = 0.f 
	};

	CPmoveSimulation sim(pm.get(), c);

	constexpr auto MAX_ITERATIONS = 10000u;
	auto iteration = 0u;
	do {

		if (!sim.Simulate() || ++iteration > MAX_ITERATIONS)
			break;

		memcpy(&pm->oldcmd, &pm->cmd, sizeof(usercmd_s));

		if ((ps->pm_flags & PMF_LADDER) != 0 || (ps->pm_flags & PMF_MANTLE) != 0)
			break;

		//wow would you look at that we can move under the target
		if (ps->velocity[Z] <= 0 && !base.IsVelocityBeingClipped(pm->ps, &pm->cmd, &pm->oldcmd)) {
			m_pBlockerAvoidPlayerState = std::make_unique<playerState_s>(ps_local);
			pm->ps = m_pBlockerAvoidPlayerState.get();
			return pm;
		}


	} while (pm->ps->groundEntityNum == 1023);

	//tough luck bud
	return nullptr;

}

pmove_t previousPmove;
playerState_s previousPlayerState;
std::unique_ptr<pmove_t> CElebotGroundTarget::TryGoingUnderTheBlockerFromDistance(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const
{
	//at this point we know that we are some units away from the wall and we want to get under it without touching it

	const auto& base = m_oRefBase;
	auto ps_local = *ps;
	auto pm = std::make_unique<pmove_t>(PM_Create(&ps_local, cmd, oldcmd));

	CSimulationController c;

	c.FPS = ELEBOT_FPS;
	c.forwardmove = 127;
	c.rightmove = 0;
	c.weapon = cmd->weapon;
	c.offhand = cmd->offHandIndex;

	//only crouch
	c.buttons = cmdEnums::crouch;

	c.viewangles = CSimulationController::CAngles{
		.viewangles = {
			ps->viewangles[PITCH],
			base.m_fTargetYaw,
			0.f
		},
		.angle_enum = EViewAngle::FixedAngle, .smoothing = 0.f
	};

	CPmoveSimulation sim(pm.get(), c);
	constexpr auto MAX_ITERATIONS = 10000u;
	auto iteration = 0u;

	previousPmove = *pm;
	previousPlayerState = *pm->ps;

	//simulate holding W and see if we no longer get blocked
	while (!base.IsPointTooFar(pm->ps->origin[base.m_iAxis])) {

		sim.forwardmove = 127;

		if (!sim.Simulate() || ++iteration > MAX_ITERATIONS)
			return nullptr;

		memcpy(&pm->oldcmd, &pm->cmd, sizeof(usercmd_s));

		//oops we hit a wall...
		if (base.IsVelocityBeingClipped(pm->ps, &pm->cmd, &pm->oldcmd)) {
			*pm = previousPmove;
			ps_local = previousPlayerState;
			pm->ps = &ps_local;
			
			//ok then just keep falling without doing anything
			sim.forwardmove = 0;
			sim.Simulate();
			memcpy(&pm->oldcmd, &pm->cmd, sizeof(usercmd_s));

			//copy state
			previousPmove = *pm;
			previousPlayerState = *pm->ps;
			continue;
		}

		m_pBlockerAvoidPlayerState = std::make_unique<playerState_s>(previousPlayerState);
		pm->ps = m_pBlockerAvoidPlayerState.get();

		return pm;


	}
	//tough luck bud
	return nullptr;
}

bool CElebotGroundTarget::ResetVelocity(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd)
{
	auto& base = m_oRefBase;
	auto& axis = base.m_iAxis;

	const auto remainingDistance = base.GetRemainingDistance();

	if (remainingDistance >= 0.f && remainingDistance < CAREFUL_THRESHOLD_DISTANCE && ps->velocity[axis] == 0.f)
		return true;

	playerState_s local_ps = *ps;
	usercmd_s ccmd = *cmd;
	ccmd.forwardmove = -127;
	ccmd.angles[YAW] = ANGLE2SHORT(AngleDelta(base.m_fTargetYaw, ps->delta_angles[YAW]));
	auto pm = PM_Create(&local_ps, &ccmd, oldcmd);
	CPmoveSimulation sim(&pm);
	sim.FPS = ELEBOT_FPS;

	//const auto DISTANCE_FROM_WALL = 1.f * (ps->speed / 190.f);
	const auto DISTANCE_FROM_WALL = 0.f;

	auto bVelocityClipped = false;
	auto bGrounded = false;
	auto bIsCorrectVelocityDirection = true;

	trace_t clipTrace{};
	CElebotBase::velocityClipping_t clipping{
		.ps = pm.ps,
		.cmd = &pm.cmd,
		.oldcmd = &pm.oldcmd,
		.boundsDelta = DISTANCE_FROM_WALL,
		.trace = &clipTrace
	};


	while (!bGrounded && bIsCorrectVelocityDirection)  {

		sim.Simulate(&pm.cmd, &pm.oldcmd);
		memcpy(&pm.oldcmd, &pm.cmd, sizeof(usercmd_s));

		bVelocityClipped = base.IsVelocityBeingClippedAdvanced(clipping);
		bGrounded = CG_IsOnGround(pm.ps);
		bIsCorrectVelocityDirection = base.IsCorrectVelocityDirection(pm.ps->velocity[axis]);
	}

	if (bGrounded)
		return false;

	const auto distTravelled = base.GetDistanceTravelled(ps->origin[base.m_iAxis], base.IsUnsignedDirection());
	const float distance = base.m_fTotalDistance - distTravelled;

	if (pm.ps->velocity[axis] == 0.f && !bVelocityClipped && distance < CAREFUL_THRESHOLD_DISTANCE) {
		//calling TryGoingUnderTheBlockerFromDistance from here gives a pretty good 
		//start to trying to move under the target from a distance
		//but I don't want to work on that now....
		return true;
	}

	//at this point velocity has flipped
	if (bVelocityClipped || base.IsPointTooFar(pm.ps->origin[axis])) {
		//too much velocity and we overstepped :(
		pm.cmd.forwardmove = -127;
	} else {

		//looks like it's still safe to move forward
		pm.cmd.forwardmove = 127;

		//simulate one more to the future
		sim.Simulate(&sim.pm->cmd, &sim.pm->oldcmd);
		memcpy(&sim.pm->oldcmd, &sim.pm->cmd, sizeof(usercmd_s));

		if(base.IsVelocityBeingClippedAdvanced(clipping) || base.IsPointTooFar(pm.ps->origin[axis])) {
			//woops looks like we are edging, go back
			pm.cmd.forwardmove = -127;
		}
	}

	base.EmplacePlaybackCommand(pm.ps, &pm.cmd);
	return false;

}

bool CElebotGroundTarget::CanStep(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const
{
	auto& base = m_oRefBase;

	const auto remainingDistance = base.GetRemainingDistance();

	if (remainingDistance > CAREFUL_THRESHOLD_DISTANCE || std::fabsf(ps->velocity[base.m_iAxis]) != 0.f)
		return false;

	const auto resetDelta = remainingDistance >= 0.f ? 45.f : -45.f;


	//limit the amount so that fps doesn't become an issue (this should never be the case, but just to be sure)
	constexpr auto MAX_ITERATIONS = ELEBOT_FPS;
	for([[maybe_unused]]const auto i : std::views::iota(0u, MAX_ITERATIONS)){
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
			base.m_fTargetYawDelta = resetDelta;
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
