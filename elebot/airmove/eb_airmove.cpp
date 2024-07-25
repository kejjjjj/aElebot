#include "eb_airmove.hpp"
#include "eb_groundtarget.hpp"
#include "eb_worldtarget.hpp"
#include "../eb_main.hpp"

#include "bg/bg_pmove.hpp"
#include "bg/bg_pmove_simulation.hpp"

#include "cg/cg_local.hpp"
#include "cg/cg_client.hpp"

#include "com/com_channel.hpp"

#include <ranges>
#include <algorithm>
#include <cassert>

CAirElebotVariation::CAirElebotVariation(CElebotBase& base) : m_oRefBase(base){}
CAirElebotVariation::~CAirElebotVariation() = default;

CAirElebot::CAirElebot(const playerState_s* ps, axis_t axis, float targetPosition)
	: CElebotBase(ps, axis, targetPosition)
{


}

CAirElebot::~CAirElebot() = default;

bool CAirElebot::Update(const playerState_s* ps, usercmd_s* cmd, [[maybe_unused]] usercmd_s* oldcmd)
{
	assert(m_pElebotVariation != nullptr);

	if (HasFinished(ps))
		return false;

	if (!m_pElebotVariation->Update(ps, cmd, oldcmd)) {
		return false;
	}

	return true;
}
void CAirElebot::SetGroundTarget()
{ 
	m_pElebotVariation = std::make_unique<CElebotGroundTarget>(*this); 
}
constexpr void CAirElebot::SetWorldTarget(const cbrush_t* brush, const sc_winding_t& winding)
{ 
	assert(brush != nullptr);
	m_pElebotVariation = std::make_unique<CElebotWorldTarget>(*this, brush, winding); 
}

float CAirElebot::GetSteepestYawDeltaForSteps(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const
{
	//hardcoding this so that this calculation doesn't need to be performed
	if (ps->speed <= 220)
		return 45.f;

	assert(CG_IsOnGround(ps) == false);

	playerState_s local_ps;
	auto pm = PM_Create(&local_ps, cmd, oldcmd);

	CSimulationController c;
	c.forwardmove = 0;
	c.rightmove = 127;
	c.buttons = cmd->buttons;
	c.FPS = ELEBOT_FPS;
	c.weapon = cmd->weapon;
	c.offhand = cmd->offHandIndex;

	float fDelta = 45.f;

	CPmoveSimulation simulation(&pm, c);

	do {
		local_ps = *ps;

		const fvec3 viewangles = { ps->viewangles[PITCH], m_fTargetYaw + fDelta, ps->viewangles[ROLL] };
		simulation.viewangles = CSimulationController::CAngles{ .viewangles = viewangles, .angle_enum = EViewAngle::FixedTurn, .smoothing = 0.f };

		for ([[maybe_unused]] const auto i : std::views::iota(0u, 2u)) {
			simulation.Simulate();
		}

		fDelta -= 1.f;

	} while (fDelta > 0.f && local_ps.velocity[m_iAxis] != 0.f);

	Com_Printf("steepest: ^1%.1f\n", fDelta);
	return fDelta + 1.f;
	
}

