#include "bg/bg_pmove.hpp"
#include "bg/bg_pmove_simulation.hpp"

#include "cg/cg_client.hpp"
#include "cg/cg_local.hpp"
#include "cg/cg_offsets.hpp"
#include "cg/cg_trace.hpp"

#include "cl/cl_utils.hpp"
#include "cl/cl_input.hpp"

#include "com/com_channel.hpp"

#include "eb_ground.hpp"
#include "airmove/eb_airmove.hpp"
#include "eb_main.hpp"

#include "utils/typedefs.hpp"
#include "utils/functions.hpp"

#include <algorithm>
#include <cmath>
#include <windows.h>
#include <cassert>
#include <array>
#include <ranges>

#if(DEBUG_SUPPORT)
#include "_Modules/aMovementRecorder/movement_recorder/mr_main.hpp"
#include "_Modules/aMovementRecorder/movement_recorder/mr_playback.hpp"
#else
#include "shared/sv_shared.hpp"
#endif

[[nodiscard]] static constexpr cardinal_dir GetCardinalDirection(float origin, axis_t axis, float targetPosition)
{
	assert(axis == X || axis == Y);

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
	m_fTargetPosition(targetPosition),
	m_fInitialYaw(ps->viewangles[YAW])
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
	ApplyMovementDirectionCorrections(ps);

}
[[nodiscard]] bool CElebotBase::HasFinished(const playerState_s* ps) const noexcept
{
	return ps->origin[m_iAxis] == m_fTargetPosition;
}
constexpr float CElebotBase::GetRemainingDistance() const noexcept
{
	if (m_fDistanceTravelled < 0.f)
		return m_fDistanceTravelled;

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
[[nodiscard]] constexpr bool CElebotBase::IsCorrectVelocityDirection(float velocity) const noexcept
{
	const auto isUnsigned = IsUnsignedDirection();
	return isUnsigned && velocity > 0 || !isUnsigned && velocity < 0;

}
constexpr float CElebotBase::GetTargetYawForSidewaysMovement() const noexcept
{
	return !IsMovingBackwards() ?
		((m_cRightmove < 0) ? m_fTargetYaw - m_fTargetYawDelta : m_fTargetYaw + m_fTargetYawDelta) :
		(m_cRightmove > 0) ? m_fTargetYaw - m_fTargetYawDelta : m_fTargetYaw + m_fTargetYawDelta;

}
void CElebotBase::ApplyMovementDirectionCorrections(const playerState_s* ps) noexcept
{
	const bool hasOverStepped = IsPointTooFar(ps->origin[m_iAxis]);

	if (hasOverStepped)
		m_cForwardMove = -m_cForwardDirection;
	else
		m_cForwardMove = m_cForwardDirection;
}
bool CElebotBase::IsVelocityBeingClipped(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const
{

	if (ps->velocity[m_iAxis] != 0.f)
		return false;

	//playerstate won't get modified
	auto pm = PM_Create(const_cast<playerState_s*>(ps), cmd, oldcmd);

	fvec3 end = pm.ps->origin;

	end[m_iAxis] += IsUnsignedDirection() ? 0.125f : -0.125f;

	//velocity does NOT get clipped when the step is this low
	pm.mins[Z] = (pm.ps->pm_flags & PMF_PRONE) != 0 ? 10.f : 18.f;


	trace_t trace = CG_TracePoint(pm.mins, pm.maxs, pm.ps->origin, end, pm.tracemask);
	
	//didn't hit anything
	if ((fvec3&)trace.normal == fvec3(0,0,0) && !trace.material)
		return false;

	return std::abs(trace.normal[m_iAxis]) == 1.f && std::abs(trace.normal[Z]) == 0.f;

}
playback_cmd CElebotBase::StateToPlayback(const playerState_s* ps, const usercmd_s* cmd, std::uint32_t fps) const
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
	pcmd.serverTime = pcmd.oldTime + (1000 / fps);

	pcmd.cmd_angles = cmd->angles;
	pcmd.delta_angles = ps->delta_angles;
	return pcmd;
}
void CElebotBase::EmplacePlaybackCommand(const playerState_s* ps, const usercmd_s* cmd)
{
	m_oVecCmds.emplace_back(StateToPlayback(ps, cmd));
}
CElebot::CElebot(const playerState_s* ps, axis_t axis, float targetPosition, elebot_flags f)
{
	if((f & groundmove) != 0)
		m_pGroundMove = std::make_unique<CGroundElebot>(ps, axis, targetPosition);

	if ((f & airmove) != 0) {
		m_pAirMove = std::make_unique<CAirElebot>(ps, axis, targetPosition);

		//todo: remove when world target is implemented!
		m_pAirMove->SetGroundTarget();
	}

}
CElebot::~CElebot() = default;

CElebotBase* CElebot::GetMove(const playerState_s* ps)
{
	return CG_IsOnGround(ps) ? reinterpret_cast<CElebotBase*>(m_pGroundMove.get()) : reinterpret_cast<CElebotBase*>(m_pAirMove.get());
}
bool CElebot::Update(const playerState_s* ps, usercmd_s* cmd, usercmd_s* oldcmd)
{
	assert(ps != nullptr);
	assert(cmd != nullptr);
	assert(oldcmd != nullptr);

	CElebotBase* base = GetMove(ps);

	if (!base)
		return false;

	base->OnFrameStart(ps);
	if (!base->Update(ps, cmd, oldcmd)) {

		//in case there is a pending finisher playback
 		base->PushPlayback();
		return false;
	}

	base->m_fOldOrigin = ps->origin[base->m_iAxis];

	//don't want our player to start moving when we're testing something else
	if(ps == &cgs->predictedPlayerState)
		base->PushPlayback();

	return true;

}

float CStaticElebot::m_fOldAutoPosition{};

struct traceResults_t
{
	trace_t trace{};
	axis_t axis{};
	float hitpos{};
};



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

	if (!std::int8_t(trace.normal[X]) && !std::int8_t(trace.normal[Y]))
		return Com_Printf("^1invalid surface\n");
	
	const auto hitpos = (start + (end - start) * trace.fraction) + (fvec3(trace.normal) * (HITBOX_SIZE - TRACE_SIZE - TRACE_CORRECTION));
	
	const axis_t axis = std::fabs(trace.normal[X]) > std::fabs(trace.normal[Y]) ? X : Y;
	Instance = std::make_unique<CElebot>(&cgs->predictedPlayerState, axis, hitpos[axis]);
}
void CStaticElebot::EB_StartAutomatically(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd)
{

	//if ( (ps->pm_flags & PMF_DUCKED) != 0) {
		if(auto ptr = EB_VelocityGotClippedInTheAir(ps, cmd, oldcmd))
			//only airmove movements allowed!
			Instance = std::make_unique<CElebot>(ps, ptr->axis, ptr->hitpos, airmove);
	//}


}
traceResults_t CG_TraceCardinalDirection(const pmove_t& pm, axis_t axis, float threshold)
{
	const float TRACE_SIZE = threshold >= 0 ? pm.maxs[axis] : -pm.mins[axis];
	constexpr float HITBOX_SIZE = 15.f;
	constexpr float TRACE_CORRECTION = 0.125f;

	fvec3 start = pm.ps->origin;
	fvec3 end = start;
	end[axis] += threshold;

	auto trace = CG_TracePoint(pm.mins, pm.maxs, pm.ps->origin, end, pm.tracemask);
	const auto hitpos = (start + (end - start) * trace.fraction) + (fvec3(trace.normal) * (HITBOX_SIZE - TRACE_SIZE - TRACE_CORRECTION));
	return { trace, axis, hitpos[axis] };
}

bool CG_TraceHitCardinalSurface(const traceResults_t& results, axis_t axis)
{
	return results.trace.fraction != 1.f && std::abs(results.trace.normal[axis]) == 1.f;
}

std::unique_ptr<traceResults_t> CStaticElebot::EB_VelocityGotClipped(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd)
{
	//avoid unnecessary calculations

	if (CStaticElebot::Instance)
		return 0;

	if (ps->velocity[X] != 0.f && ps->velocity[Y] != 0.f)
		return 0;

	auto pm = PM_Create(const_cast<playerState_s*>(ps), cmd, oldcmd);	
	traceResults_t trace;

	constexpr std::array<float, 2> values = { 0.125f, -0.125f };

	for (const auto& value : values) {
		for (const auto i : std::views::iota(int(X), int(Z))) {
			const auto axis = axis_t(i);

			trace = CG_TraceCardinalDirection(pm, axis, value);
			if (CG_TraceHitCardinalSurface(trace, axis)) {

				//don't calculate the same thing again...
				if (m_fOldAutoPosition == ps->origin[axis])
					return 0;

				m_fOldAutoPosition = ps->origin[axis];
				return std::make_unique<traceResults_t>(trace);
			}

		}

	}

	return 0;
}
std::unique_ptr<traceResults_t> CStaticElebot::EB_VelocityGotClippedInTheAir(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd)
{
	if (CG_IsOnGround(ps)) {
		m_fOldAutoPosition = 0;
		return 0;
	}

	return EB_VelocityGotClipped(ps, cmd, oldcmd);
}
