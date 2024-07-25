#include "eb_worldtarget.hpp"

CElebotWorldTarget::CElebotWorldTarget(CElebotBase& base, const cbrush_t* brush, const sc_winding_t& winding)
	: CAirElebotVariation(base), m_pBrush(brush), m_oWinding(winding)
{

}
CElebotWorldTarget::~CElebotWorldTarget() = default;

bool CElebotWorldTarget::Update([[maybe_unused]] const playerState_s* ps, [[maybe_unused]] usercmd_s* cmd, [[maybe_unused]] usercmd_s* oldcmd)
{
	return false;
}
