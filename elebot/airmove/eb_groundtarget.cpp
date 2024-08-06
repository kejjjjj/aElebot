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

using FailCondition = CPmoveSimulation::StopPositionInput_t::FailCondition_e;
using TargetAxes = CPmoveSimulation::StopPositionInput_t::TargetAxes_e;

CElebotGroundTarget::CElebotGroundTarget(CElebotBase& base) 
	: CAirElebotVariation(base) 
{

}
CElebotGroundTarget::~CElebotGroundTarget() = default;

bool CElebotGroundTarget::Update([[maybe_unused]] const playerState_s* ps, [[maybe_unused]] usercmd_s* cmd, [[maybe_unused]] usercmd_s* oldcmd)
{
	auto& base = m_oRefBase;


	//todo: add movement corrections when distance > 2.f
	//^^^^^ will do after I find a use case for it ^^^^^

	//if stuck against a wall while falling
	const auto beingClipped = base.IsVelocityBeingClipped(ps, cmd, oldcmd);
	if (m_pBlockerAvoidState || ps->velocity[Z] < 0 && beingClipped) {
		return UpdateBlocker(ps, cmd, oldcmd);
	}

	//the code below doesn't work when being clipped..
	if (beingClipped)
		return true;

	//it might give more airtime when crouched (hitting head)
	cmd->buttons |= cmdEnums::crouch;

	//tries to CAREFULLY step midair
	//which means this is not super quick!
	if (CanStep(ps, cmd, oldcmd)) {
		Step(ps, cmd);
		return true;
	}



	return m_oRefBase.HasFinished(ps) == false;
}
bool CElebotGroundTarget::UpdateBlocker(const playerState_s* ps, usercmd_s* cmd, usercmd_s* oldcmd)
{
	if (!GetBlocker(ps, cmd, oldcmd))
		return true;

	assert(m_pBruteForcer != nullptr);
	return m_pBruteForcer->Update(ps, cmd, oldcmd);
}
bool CElebotGroundTarget::GetBlocker(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd)
{
	if (m_pBlockerAvoidState) {
		return true;
	}

	//see if we can get under the brush 
	m_pBlockerAvoidState = TryGoingUnderTheBlocker(ps, cmd, oldcmd);

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
	//at this point we know we are stuck hugging a wall

	const auto& base = m_oRefBase;
	auto ps_local = *ps;

	auto pm = std::make_unique<pmove_t>(PM_Create(&ps_local, cmd, oldcmd));

	CSimulationController c;

	c.FPS = 333;
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
			ps->viewangles[ROLL] 
		}, 
		.angle_enum = EViewAngle::FixedAngle, .smoothing = 0.f 
	};

	CPmoveSimulation sim(pm.get(), c);

	do {

		if (!sim.Simulate())
			break;

		memcpy(&pm->oldcmd, &pm->cmd, sizeof(usercmd_s));

		m_oCmds.emplace_back(base.StateToPlayback(pm->ps, &pm->cmd));

		//wow would you look at that we can move under the target
		if (ps->velocity[Z] <= 0 && !base.IsVelocityBeingClipped(pm->ps, &pm->cmd, &pm->oldcmd)) {
			//sim.Simulate();
			m_pBlockerAvoidPlayerState = std::make_unique<playerState_s>(ps_local);
			pm->ps = m_pBlockerAvoidPlayerState.get();
			return pm;
		}


	} while (pm->ps->groundEntityNum == 1023);

	//tough luck bud
	m_oCmds.clear();
	return nullptr;

}
bool CElebotGroundTarget::CanStep(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const
{
	auto& base = m_oRefBase;

	//todo: fix the magic number to something that works with all g_speed values
	if (base.GetRemainingDistance() > 2.f || std::fabsf(ps->velocity[base.m_iAxis]) != 0.f)
		return false;


	//limit the amount so that fps doesn't become an issue (this should never be the case, but just to be sure)
	constexpr auto MAX_ITERATIONS = ELEBOT_FPS;
	for([[maybe_unused]]const auto i : std::views::iota(0u, MAX_ITERATIONS)){
		
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
