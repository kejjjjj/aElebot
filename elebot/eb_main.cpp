#include "bg/bg_pmove_simulation.hpp"

#include "cg/cg_client.hpp"
#include "cg/cg_local.hpp"
#include "cg/cg_offsets.hpp"
#include "cg/cg_trace.hpp"

#include "cl/cl_utils.hpp"

#include "com/com_channel.hpp"

#include "eb_ground.hpp"
#include "eb_main.hpp"

#include "utils/typedefs.hpp"

#include <algorithm>
#include <cmath>
#include <windows.h>

#if(DEBUG_SUPPORT)
#include "_Modules/aMovementRecorder/movement_recorder/mr_main.hpp"
#include "_Modules/aMovementRecorder/movement_recorder/mr_playback.hpp"
#else
#include "shared/sv_shared.hpp"
#endif

[[nodiscard]] static constexpr cardinal_dir GetCardinalDirection(float origin, axis_t axis, float targetPosition)
{
	switch (axis) {
	case X:
		return origin < targetPosition ? N : S;
	case Y:
		return origin < targetPosition ? W : E;

	[[unlikely]]
	default:
		throw std::exception("GetCardinalDirection(): axis == Z");
	}
}
std::unique_ptr<CElebot> CStaticElebot::Instance;

CElebotBase::CElebotBase(const playerState_s* ps, axis_t axis, float targetPosition) :
	m_iAxis(axis), 
	m_fTargetPosition(targetPosition) 
{
	m_fTotalDistance = std::fabsf(m_fTargetPosition - ps->origin[axis]);
	m_fDistanceTravelled = 0.f;

	m_iDirection = GetCardinalDirection(ps->origin[axis], axis, m_fTargetPosition);
	m_fTargetYaw = (float)m_iDirection;

}
CElebotBase::~CElebotBase() = default;

void CElebotBase::PushPlayback()
{
	if (m_oVecCmds.empty())
		return;



#if(DEBUG_SUPPORT)
	CStaticMovementRecorder::PushPlayback(m_oVecCmds, 
		{ 
			.jump_slowdownEnable = CPlayback::slowdown_t::both, 
			.ignorePitch = true 
		}
	);
#else

	CMain::Shared::GetFunctionOrExit("AddPlayback")->As<void, std::vector<playback_cmd>&&, const PlaybackInitializer&>()->Call(
		std::move(m_oVecCmds),
		{
			.jump_slowdownEnable = 2, //2 is for both
			.ignorePitch = true
		}

	);


#endif

	m_oVecCmds.clear();


}
void CElebotBase::PushPlayback(const std::vector<playback_cmd>& cmds) const
{
	if (cmds.empty())
		return;
#if(DEBUG_SUPPORT)

	CStaticMovementRecorder::PushPlayback(cmds,
		{
			.jump_slowdownEnable = CPlayback::slowdown_t::both,
			.ignorePitch = true
		}
	);
#else
	CMain::Shared::GetFunctionOrExit("AddPlaybackC")->As<void, const std::vector<playback_cmd>&, const PlaybackInitializer&>()->Call(
		cmds,
		{
			.jump_slowdownEnable = 2, //2 is for both
			.ignorePitch = true
		}

	);
#endif

}
void CElebotBase::OnFrameStart(const playerState_s* ps) noexcept
{
	m_fDistanceTravelled = GetDistanceTravelled(ps->origin[m_iAxis], IsUnsignedDirection());

}
[[nodiscard]] bool CElebotBase::HasFinished(const playerState_s* ps) const noexcept
{
	return ps->origin[m_iAxis] == m_fTargetPosition;
}
constexpr float CElebotBase::GetRemainingDistance() const noexcept
{
	return m_fTotalDistance - m_fDistanceTravelled;
}
constexpr float CElebotBase::GetRemainingDistance(const float p) const noexcept
{
	return m_fTotalDistance - p;
}

constexpr float CElebotBase::GetDistanceTravelled(const float p, bool isUnsignedDirection) const noexcept
{
	const auto distTravelled = m_fTotalDistance - (isUnsignedDirection ? (m_fTargetPosition - p) : (p - m_fTargetPosition));

	//if we have travelled too far, make the result negative
	if (distTravelled > m_fTotalDistance) {
		return m_fTotalDistance - distTravelled;
	}

	//if we have travelled to the wrong direction, keep the travel distance at 0
	return std::clamp(distTravelled, 0.f, m_fTotalDistance);
}

constexpr bool CElebotBase::IsPointTooFar(const float p) const noexcept
{
	//if the result is negative, then we have gone too far
	return GetDistanceTravelled(p, IsUnsignedDirection()) < 0.f;
}
constexpr bool CElebotBase::IsPointInWrongDirection(const float p) const noexcept
{
	const auto distTravelled = m_fTotalDistance - (IsUnsignedDirection() ? (m_fTargetPosition - p) : (p - m_fTargetPosition));

	//if the result is negative, then it's behind us
	return distTravelled < 0.f;
}
constexpr bool CElebotBase::IsMovingBackwards() const noexcept
{
	return m_cForwardMove < 0;
}
void CElebotBase::EmplacePlaybackCommand(const playerState_s* ps, const usercmd_s* cmd)
{
	playback_cmd pcmd;
	pcmd.buttons = cmd->buttons;
	pcmd.forwardmove = cmd->forwardmove;
	pcmd.offhand = cmd->offHandIndex;
	pcmd.origin = ps->origin;
	pcmd.rightmove = cmd->rightmove;
	pcmd.velocity = ps->velocity;
	pcmd.weapon = cmd->weapon;

	pcmd.oldTime = cmd->serverTime;
	pcmd.serverTime = pcmd.oldTime + (1000 / ELEBOT_FPS);

	pcmd.cmd_angles = cmd->angles;
	pcmd.delta_angles = ps->delta_angles;

	m_oVecCmds.emplace_back(pcmd);
}
CElebot::CElebot(const playerState_s* ps, axis_t axis, float targetPosition)
{
	m_pGroundMove = std::make_unique<CGroundElebot>(ps, axis, targetPosition);
}


bool CElebot::Update(const playerState_s* ps, usercmd_s* cmd, usercmd_s* oldcmd)
{
	if (WASD_PRESSED())
		return false;


	CElebotBase* base = CG_IsOnGround(ps) ? m_pGroundMove.get() : nullptr; //airmove yet to be implemented

	if (!base)
		return false;

	base->OnFrameStart(ps);
	if (!base->Update(ps, cmd, oldcmd)) {

		if (base->HasFinished(ps))
			cmd->angles[YAW] = ANGLE2SHORT(AngleDelta(base->m_fTargetYaw, ps->delta_angles[YAW]));

		return false;
	}

	base->m_fOldOrigin = ps->origin[base->m_iAxis];
	base->PushPlayback();
	return true;

}



void CStaticElebot::EB_MoveToCursor()
{
	if (CL_ConnectionState() != CA_ACTIVE)
		return;

	constexpr float TRACE_SIZE = 1.f;
	constexpr float HITBOX_SIZE = 15.f;
	constexpr float TRACE_CORRECTION = 0.125f;

	const fvec3 start = rg->viewOrg;
	const fvec3 end = start + fvec3(clients->cgameViewangles).toforward() * 99999;

	const auto trace = CG_TracePoint(-TRACE_SIZE, TRACE_SIZE, start, end, MASK_PLAYERSOLID);

	if (!std::int8_t(trace.normal[X]) && !std::int8_t(trace.normal[Y])) {
		Com_Printf("^1invalid surface\n");
		return;
	}

	const auto hitpos = (start + (end - start) * trace.fraction) + (fvec3(trace.normal) * (HITBOX_SIZE - TRACE_SIZE - TRACE_CORRECTION));
	
	//std::greater either improves readibility or it doesn't
	const axis_t axis = std::greater<float>()(std::fabs(trace.normal[X]), std::fabs(trace.normal[Y])) ? X : Y;
	Instance = std::make_unique<CElebot>(&cgs->predictedPlayerState, axis, hitpos[axis]);
}
