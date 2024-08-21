#include "bg/bg_pmove.hpp"
#include "bg/bg_pmove_simulation.hpp"

#include "cg/cg_client.hpp"
#include "cg/cg_local.hpp"
#include "cg/cg_offsets.hpp"
#include "cg/cg_trace.hpp"

#include "cl/cl_utils.hpp"
#include "cl/cl_input.hpp"

#include "com/com_channel.hpp"


#include "airmove/eb_airmove.hpp"
#include "eb_ground.hpp"
#include "eb_main.hpp"
#include "eb_standup.hpp"

#include "net/nvar_table.hpp"

#include "utils/typedefs.hpp"
#include "utils/functions.hpp"

#include <algorithm>
#include <cmath>
#include <windows.h>
#include <cassert>
#include <array>
#include <ranges>

#include "_Modules/aMovementRecorder/movement_recorder/mr_playback.hpp"

#if(DEBUG_SUPPORT)
#include "_Modules/aMovementRecorder/movement_recorder/mr_main.hpp"
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

CElebotBase::CElebotBase(const playerState_s* ps, const init_data& init) :
	m_iAxis(init.m_iAxis), 
	m_fTargetPosition(init.m_fTargetPosition),
	m_fInitialYaw(ps->viewangles[YAW]),
	m_bPrintf(init.m_bPrintf)
{
	m_fTotalDistance = std::fabsf(m_fTargetPosition - ps->origin[m_iAxis]);
	m_fDistanceTravelled = 0.f;

	if (ps->origin[m_iAxis] != m_fTargetPosition) {
		m_iDirection = GetCardinalDirection(ps->origin[m_iAxis], m_iAxis, m_fTargetPosition);
		m_fTargetYaw = (float)m_iDirection;
	} else {
		ResolveYawConflict(init.m_vecTargetNormals);
	}

}
CElebotBase::~CElebotBase() = default;

void CElebotBase::ResolveYawConflict(const fvec3& normals)
{
	if (m_iAxis == X) 
		m_fTargetYaw = (normals[X] == 1.f ? 180.f : 0.f);
	else
		m_fTargetYaw = (normals[Y] == 1.f ? -90.f : 90.f);

	m_iDirection = CG_RoundAngleToCardinalDirection(m_fTargetYaw);

}
void CElebotBase::PushPlayback()
{
	if (m_oVecCmds.empty())
		return;

#if(DEBUG_SUPPORT)
	CStaticMovementRecorder::PushPlayback(m_oVecCmds, 
		{ 
			.m_eJumpSlowdownEnable = slowdown_t::both,
			.m_bIgnorePitch = true,
			.m_bIgnoreWASD = true,
			.m_bSetComMaxfps = true,
			.m_bRenderExpectationVsReality = false
		}
	);
#else

	CMain::Shared::GetFunctionOrExit("AddPlayback")->As<void, std::vector<playback_cmd>&&, const CPlaybackSettings&>()->Call(
		std::move(m_oVecCmds),
		{
			.m_eJumpSlowdownEnable = slowdown_t::both,
			.m_bIgnorePitch = true,
			.m_bIgnoreWASD = true,
			.m_bSetComMaxfps = true,
			.m_bRenderExpectationVsReality = false
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
			.m_eJumpSlowdownEnable = slowdown_t::both,
			.m_bIgnorePitch = true,
			.m_bIgnoreWASD = true,
			.m_bSetComMaxfps = true,
			.m_bRenderExpectationVsReality = false
		}
	);
#else
	CMain::Shared::GetFunctionOrExit("AddPlaybackC")->As<void, const std::vector<playback_cmd>&, const CPlaybackSettings&>()->Call(
		cmds,
		{
			.m_eJumpSlowdownEnable = slowdown_t::both,
			.m_bIgnorePitch = true,
			.m_bIgnoreWASD = true,
			.m_bSetComMaxfps = true,
			.m_bRenderExpectationVsReality = false
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
bool CElebotBase::IsVelocityBeingClippedAdvanced(velocityClipping_t& init) const
{

	auto pm = PM_Create(const_cast<playerState_s*>(init.ps), init.cmd, init.oldcmd);
	fvec3 start = pm.ps->origin; 
	fvec3 end = start;

	end[m_iAxis] += IsUnsignedDirection() ? init.boundsDelta : -init.boundsDelta;
	pm.mins[Z] = (pm.ps->pm_flags & PMF_PRONE) != 0 ? 10.f : 18.f;

	trace_t trace = CG_TracePoint(pm.mins, pm.maxs, pm.ps->origin, end, pm.tracemask);
	init.clipPos = (start + (end - start) * trace.fraction)[m_iAxis];
	if (init.trace) 
		*init.trace = trace;

	return (trace.fraction < 1.f || trace.startsolid || trace.allsolid || std::abs(trace.normal[m_iAxis]) == 1.f && std::abs(trace.normal[Z]) == 0.f);

}
bool CElebotBase::IsVelocityBeingClipped(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const
{
	if (ps->velocity[m_iAxis] != 0.f)
		return false;

	auto clipping = velocityClipping_t{ .ps = ps, .cmd = cmd, .oldcmd = oldcmd };
	return IsVelocityBeingClippedAdvanced(clipping);
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
CElebot::CElebot(const playerState_s* ps, const init_data& init) : m_oInit(init)
{
	auto& f = m_oInit.m_iElebotFlags;
	auto& axis = m_oInit.m_iAxis;
	auto& targetPosition = m_oInit.m_fTargetPosition;



	if((f & groundmove) != 0)
		m_pGroundMove = std::make_unique<CGroundElebot>(ps, m_oInit);

	if ((f & airmove) != 0) {
		m_pAirMove = std::make_unique<CAirElebot>(ps, m_oInit);


		//if we start from the target coordinate, detach from the wall and ele again to move sideways
		//but don't automatically detach if we're only targeting airmove
		if (!CG_IsOnGround(ps) && ps->origin[axis] == targetPosition && ps->velocity[Z] == 0.f && f != airmove)
			m_pAirMove->SetToDetach();
		else 
			m_pAirMove->SetGroundTarget();
	}

	if (m_oInit.m_bAutoStandUp && f != airmove)
		m_pStandup = std::make_unique<CElebotStandup>(*GetMove(ps));


}
CElebot::~CElebot() = default;

CElebotBase* CElebot::GetMove(const playerState_s* ps)
{
	return CG_IsOnGround(ps) ? reinterpret_cast<CElebotBase*>(m_pGroundMove.get()) : reinterpret_cast<CElebotBase*>(m_pAirMove.get());
}
bool CElebot::TryAgain() const noexcept
{
	return m_pGroundMove && m_pGroundMove->TryAgain() || m_pAirMove && m_pAirMove->TryAgain();
}
bool CElebot::Update(const playerState_s* ps, usercmd_s* cmd, usercmd_s* oldcmd)
{
	assert(ps != nullptr);
	assert(cmd != nullptr);
	assert(oldcmd != nullptr);

	CElebotBase* base = GetMove(ps);

	if (!base)
		return false;

	//sorry you can't move anymore LOL
	cmd->forwardmove = 0;
	cmd->rightmove = 0;
	
	base->OnFrameStart(ps);
	if (!base->Update(ps, cmd, oldcmd)) {

		if (base->HasFinished(ps) && m_pStandup && m_pStandup->Update(ps, cmd, oldcmd))
			return true;

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

	init_data data{
		.m_iAxis = axis,
		.m_fTargetPosition = hitpos[axis],
		.m_iElebotFlags = all_flags,
		.m_vecTargetNormals = trace.normal,
		.m_bAutoStandUp = NVar_FindMalleableVar<bool>("Auto standup")->Get(),
		.m_bPrintf = NVar_FindMalleableVar<bool>("Printf")->Get()
	};

	Instance = std::make_unique<CElebot>(&cgs->predictedPlayerState, data);

}
void CStaticElebot::EB_CenterYaw()
{
	auto ps = &cgs->predictedPlayerState;
	clients->viewangles[YAW] = AngleDelta(CG_GetNearestCardinalAngle(ps->viewangles[YAW]), ps->delta_angles[YAW]);
}
void CStaticElebot::EB_StartAutomatically(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd)
{

	if (auto ptr = EB_VelocityGotClippedInTheAir(ps, cmd, oldcmd)) {
		//only airmove movements allowed!
		Instance = std::make_unique<CElebot>(ps, init_data{ 
			.m_iAxis = ptr->axis, 
			.m_fTargetPosition = ptr->hitpos, 
			.m_iElebotFlags = airmove, 
			.m_vecTargetNormals = ptr->trace.normal,
			.m_bPrintf = NVar_FindMalleableVar<bool>("Printf")->Get() 
		});
	}
	


}
traceResults_t CG_TraceCardinalDirection(pmove_t& pm, axis_t axis, float threshold)
{

	fvec3 start = pm.ps->origin;
	fvec3 end = start;
	end[axis] += threshold;

	const auto allSolidDelta = threshold < 0 ? 0.250f : -0.250f; //0.250 = (14.125 + 0.125) - 14  

	pm.mins[axis] = -1;
	pm.maxs[axis] = 1;


	auto trace = CG_TracePoint(pm.mins, pm.maxs, start, end, pm.tracemask);
	const auto hitpos = (start + (end - start) * trace.fraction) - (threshold + allSolidDelta); 
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

	constexpr std::array<float, 2> values = { 14.125f, -14.125f };

	for (const auto& value : values) {
		for (const auto i : std::views::iota(int(X), int(Z))) {
			const auto axis = axis_t(i);

			pm.mins[axis] = -15;
			pm.maxs[axis] = 15;

			traceResults_t trace = CG_TraceCardinalDirection(pm, axis, value);
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
	

	if (CG_IsOnGround(ps) || CG_HasFlag(ps, PMF_LADDER | PMF_MANTLE)) {
		m_fOldAutoPosition = 0;
		return 0;
	}

	return EB_VelocityGotClipped(ps, cmd, oldcmd);
}
