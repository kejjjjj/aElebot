#pragma once

#include "eb_main.hpp"

class CGroundElebot : public CElebotBase
{
public:
	CGroundElebot(const playerState_s* ps, axis_t axis, float targetPosition);
	~CGroundElebot();

	[[nodiscard]] ElebotUpdate_f Update override;

private:
	[[nodiscard]] bool CanSprint(const playerState_s* ps) const noexcept;
	void Sprint(const playerState_s* ps, const usercmd_s* cmd);
	void StopSprinting(usercmd_s* cmd) const noexcept;

	[[nodiscard]] bool CanWalk(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) noexcept;
	void Walk(const playerState_s* ps, usercmd_s* cmd);

	[[nodiscard]] bool CanStepLongitudinally(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) noexcept;
	void StepLongitudinally(const playerState_s* ps, usercmd_s* cmd);


	[[nodiscard]] bool CanStepSideways(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) noexcept;
	void StepSideways(const playerState_s* ps, usercmd_s* cmd);

	[[nodiscard]] constexpr bool ReadyToJump(const playerState_s* ps) const noexcept;
	[[nodiscard]] bool IsElevatorOnAStep(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const noexcept;
	void Jump(usercmd_s* cmd) noexcept;

	bool m_bWalkingIsTooDangerous = false;
	bool m_bSteppingLongitudinallyIsTooDangerous = false;
};
