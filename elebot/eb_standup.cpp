#include "eb_standup.hpp"
#include "eb_main.hpp"

#include "bg/bg_pmove.hpp"
#include "bg/bg_pmove_simulation.hpp"

#include "cg/cg_angles.hpp"
#include "cg/cg_local.hpp"
#include "cg/cg_client.hpp"

#include "com/com_channel.hpp"

#include "utils/engine.hpp"

#include <ranges>

bool PM_CanStand(const playerState_s* ps, const pmove_t* pm)
{
	constexpr static auto f = 0x40E640;
	bool result = false;
	__asm
	{
		mov edx, pm;
		mov ecx, ps;
		call f;
		mov result, al;
	}
	return result;
}

void PM_PlayerTrace(pmove_t* pm, trace_t* trace, const fvec3& start, const fvec3& mins, const fvec3& maxs, const fvec3& end, int passEntityNum, int contentMask)
{
	constexpr static auto f = 0x40E160;

	__asm
	{
		mov esi, pm;
		push contentMask;
		push passEntityNum;
		push end;
		push maxs;
		push mins;
		push start;
		push trace;
		call f;
		add esp, 28;
	}
}

bool CElebotStandup::StandupWillElevate(pmove_t& pmOrg)
{
	
	playerState_s ps_local = *pmOrg.ps;
	auto pm = PM_Create(&ps_local, &pmOrg.cmd, &pmOrg.oldcmd);

	pm.cmd.buttons = 0;
	const auto oldOriginZ = pm.ps->origin[Z];

	CPmoveSimulation sim(&pm);
	sim.FPS = ELEBOT_FPS;

	for ([[maybe_unused]] const auto i : std::views::iota(0u, 2u)) {
		sim.Simulate(&pm.cmd, &pm.oldcmd);
		memcpy(&pm.oldcmd, &pm.cmd, sizeof(usercmd_s));

		m_oRefBase.EmplacePlaybackCommand(pm.ps, &pm.cmd);
	}

	if (oldOriginZ < pm.ps->origin[Z]) {
		return true;
	}

	m_oRefBase.m_oVecCmds.pop_back();
	m_oRefBase.m_oVecCmds.pop_back();

	return false;
}

CElebotStandup::CElebotStandup(CElebotBase& base) : m_oRefBase(base)
{

}

bool CElebotStandup::Update(const playerState_s* ps, usercmd_s* cmd, const usercmd_s* oldcmd)
{

	if (WASD_PRESSED())
		return false;

	cmd->forwardmove = 0;
	cmd->rightmove = 0;

	if (CG_IsOnGround(ps) && !m_bCeilingExists && CG_HasFlag(ps, PMF_DUCKED | PMF_PRONE))
		return GroundMove(ps, cmd, oldcmd);

	return AirMove(ps, cmd, oldcmd);
}

bool CElebotStandup::GroundMove(const playerState_s* ps, usercmd_s* cmd, const usercmd_s* oldcmd)
{
	playerState_s ps_local = *ps;
	auto pm = PM_Create(&ps_local, cmd, oldcmd);

	CPmoveSimulation sim(&pm);
	sim.FPS = ELEBOT_FPS;

	if (StandupWillElevate(pm)) {
		cmd->buttons = 0;
		return false;
	}

	return true;
}
bool CElebotStandup::AirMove([[maybe_unused]] const playerState_s* ps, [[maybe_unused]] usercmd_s* cmd, [[maybe_unused]] const usercmd_s* oldcmd)
{
	if (!m_bCeilingExists && !CeilingExists(ps, cmd, oldcmd))
		return false;

	return false;
}
playerState_s next_ps;
bool CElebotStandup::CeilingExists(const playerState_s* ps, usercmd_s* cmd, const usercmd_s* oldcmd)
{
	playerState_s ps_local = *ps;

	//standup
	auto pm = PM_Create(&ps_local, cmd, oldcmd);
	pm.cmd.buttons = CG_IsOnGround(pm.ps) ? cmdEnums::jump : cmdEnums::crouch;

	CPmoveSimulation sim(&pm);
	sim.FPS = ELEBOT_FPS;

	constexpr auto MAX_ITERATIONS = 10000u;
	auto iteration = 0u;
	do {
		m_oRefBase.EmplacePlaybackCommand(pm.ps, &pm.cmd);

		sim.Simulate(&pm.cmd, &pm.oldcmd);
		memcpy(&pm.oldcmd, &pm.cmd, sizeof(usercmd_s));

		pm.cmd.buttons = CG_IsOnGround(pm.ps) ? cmdEnums::jump : cmdEnums::crouch;


		next_ps = ps_local;
		CPmoveSimulation::PredictNextPosition(&next_ps, &pm.cmd, &pm.oldcmd, ELEBOT_FPS);

		if (++iteration > MAX_ITERATIONS) //avoid game freezing
			return true;

	} while (next_ps.velocity[Z] > 0.f);

	m_bCeilingExists = StandupWillElevate(pm);

	if (!m_bCeilingExists)
		m_oRefBase.m_oVecCmds.clear();
	else
		cmd->buttons = cmdEnums::crouch_hold;

	return m_bCeilingExists;
}
