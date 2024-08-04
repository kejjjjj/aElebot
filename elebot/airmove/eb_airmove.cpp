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
	: CElebotBase(ps, axis, targetPosition) {}
CAirElebot::~CAirElebot() = default;

bool CAirElebot::Update(const playerState_s* ps, usercmd_s* cmd, [[maybe_unused]] usercmd_s* oldcmd)
{
	assert(m_pElebotVariation != nullptr);


	if (!m_pElebotVariation->Update(ps, cmd, oldcmd)) {
		return false;
	}

	return !HasFinished(ps);
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

