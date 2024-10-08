#include "eb_airmove.hpp"
#include "eb_groundtarget.hpp"
#include "eb_worldtarget.hpp"
#include "eb_detach.hpp"
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

CAirElebot::CAirElebot(const playerState_s* ps, const init_data& init)
	: CElebotBase(ps, init) {}
CAirElebot::~CAirElebot() = default;

bool CAirElebot::Update(const playerState_s* ps, usercmd_s* cmd, [[maybe_unused]] usercmd_s* oldcmd)
{
	assert(m_pElebotVariation != nullptr);
	return m_pElebotVariation->Update(ps, cmd, oldcmd);
}
void CAirElebot::SetGroundTarget()
{ 
	m_pElebotVariation = std::make_unique<CElebotGroundTarget>(*this); 
}
void CAirElebot::SetToDetach()
{
	m_pElebotVariation = std::make_unique<CDetachElebot>(*this);
}
constexpr void CAirElebot::SetWorldTarget(const cbrush_t* brush, const sc_winding_t& winding)
{ 
	assert(brush != nullptr);
	m_pElebotVariation = std::make_unique<CElebotWorldTarget>(*this, brush, winding); 
}

