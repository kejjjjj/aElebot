#include "eb_detach.hpp"

#include "bg/bg_pmove.hpp"
#include "bg/bg_pmove_simulation.hpp"

#include "cg/cg_local.hpp"
#include "cg/cg_client.hpp"

#include "com/com_channel.hpp"

CDetachElebot::CDetachElebot(CElebotBase& base) : CAirElebotVariation(base)
{

}
CDetachElebot::~CDetachElebot() = default;

bool CDetachElebot::Update([[maybe_unused]] const playerState_s* ps, [[maybe_unused]] usercmd_s* cmd, [[maybe_unused]] usercmd_s* oldcmd)
{
	auto& base = m_oRefBase;

	//detached it seems
	if (CG_IsOnGround(ps) || ps->origin[base.m_iAxis] != base.m_fTargetPosition)
		return false;

	if (CanMoveAwayFromTheWall(ps, cmd, oldcmd)) {		
		MoveAwayFromTheWall(ps, cmd);
		return true;
	}

	if (FindDetachAngle(ps, cmd, oldcmd)) {
		Detach(ps, cmd);
		return false;
	}

	return true;
}

//keep moving backward while we still stay attached
bool CDetachElebot::CanMoveAwayFromTheWall(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const
{
	auto& base = m_oRefBase;
	playerState_s ps_local = *ps;
	auto pm = PM_Create(&ps_local, cmd, oldcmd);
	CPmoveSimulation sim(&pm, GenericCrouchedBackwardMoveController(pm.ps, &pm.cmd));

	sim.Simulate();

	return pm.ps->origin[base.m_iAxis] == base.m_fTargetPosition;
	//return base.IsCorrectVelocityDirection(pm.ps->velocity[base.m_iAxis]) && ps->velocity[base.m_iAxis] != 0.f;
}

void CDetachElebot::MoveAwayFromTheWall(const playerState_s* ps, const usercmd_s* cmd) const
{
	auto& base = m_oRefBase;
	usercmd_s ccmd = *cmd;
	ccmd.forwardmove = -127;
	ccmd.angles[YAW] = ANGLE2SHORT(AngleDelta(base.m_fTargetYaw, ps->delta_angles[YAW]));
	base.EmplacePlaybackCommand(ps, &ccmd);

}
bool CDetachElebot::FindDetachAngle(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const
{
	auto& base = m_oRefBase;
	playerState_s ps_local = *ps;
	auto pm = PM_Create(&ps_local, cmd, oldcmd);
	CPmoveSimulation sim(&pm, GenericCrouchedBackwardMoveController(pm.ps, &pm.cmd));

	sim.forwardmove = 0;
	sim.rightmove = -127;

	do {
		sim.GetAngles().right = base.m_fTargetYaw + (base.m_fTargetYawDelta += 0.1f);

		if (!sim.Simulate())
			return false;

		memcpy(&pm.oldcmd, &pm.cmd, sizeof(usercmd_s));

	} while (pm.ps->origin[base.m_iAxis] == base.m_fTargetPosition && base.m_fTargetYawDelta < 90.f);

	return base.m_fTargetYawDelta < 90.f;
}

void CDetachElebot::Detach(const playerState_s* ps, const usercmd_s* cmd) const
{
	auto& base = m_oRefBase;
	usercmd_s ccmd = *cmd;
	ccmd.forwardmove = 0;
	ccmd.rightmove = -127;
	ccmd.angles[YAW] = ANGLE2SHORT(AngleDelta(base.m_fTargetYaw + base.m_fTargetYawDelta, ps->delta_angles[YAW]));
	base.EmplacePlaybackCommand(ps, &ccmd);

	//fix yaw
	ccmd.serverTime += (1000 / ELEBOT_FPS);
	ccmd.rightmove = 0;
	ccmd.angles[YAW] = ANGLE2SHORT(AngleDelta(base.m_fTargetYaw, ps->delta_angles[YAW]));
	base.EmplacePlaybackCommand(ps, &ccmd);


}

CSimulationController CDetachElebot::GenericCrouchedBackwardMoveController(const playerState_s* ps, const usercmd_s* cmd) const
{
	CSimulationController c;

	c.forwardmove = -127;
	c.rightmove = 0;
	c.weapon = cmd->weapon;
	c.offhand = cmd->offHandIndex;
	c.FPS = ELEBOT_FPS;

	//only crouch
	c.buttons = cmdEnums::crouch | cmdEnums::crouch_hold;

	c.viewangles = CSimulationController::CAngles{
		.viewangles = {
			ps->viewangles[PITCH],
			m_oRefBase.m_fTargetYaw,
			ps->viewangles[ROLL]
		},
		.angle_enum = EViewAngle::FixedAngle, .smoothing = 0.f
	};

	return c;
}