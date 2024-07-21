#pragma once

#include <memory>
#include <vector>

#include "cg/cg_angles.hpp"

//a bad method
#define WORSE_IMPLEMENTATION 0

constexpr std::int32_t ELEBOT_FPS = 333;

class CGroundElebot;
struct playback_cmd;

using ElebotUpdate_f = bool(const playerState_s* ps, usercmd_s* cmd, usercmd_s* oldcmd);

class CElebotBase
{
	friend class CElebot;

public:
	CElebotBase(const playerState_s* ps, axis_t axis, float targetPosition);
	virtual ~CElebotBase();
	[[nodiscard]] virtual ElebotUpdate_f Update = 0;

	void PushPlayback();
	void PushPlayback(const std::vector<playback_cmd>& cmds) const;

	void OnFrameStart(const playerState_s* ps) noexcept;

	[[nodiscard]] virtual bool HasFinished(const playerState_s* ps) const noexcept;
protected:
	[[nodiscard]] virtual constexpr float GetRemainingDistance() const noexcept;
	[[nodiscard]] virtual constexpr float GetRemainingDistance(const float p) const noexcept;

	[[nodiscard]] virtual constexpr bool IsPointTooFar(const float p) const noexcept;
	[[nodiscard]] constexpr float GetDistanceTravelled(const float p, bool isSignedDirection) const noexcept;

	[[nodiscard]] inline constexpr bool IsUnsignedDirection() const noexcept { return m_iDirection == N || m_iDirection == W; }

	void EmplacePlaybackCommand(const playerState_s* ps, const usercmd_s* cmd);

	axis_t m_iAxis = {};
	cardinal_dir m_iDirection = {};


	float m_fTargetPosition = {};
	float m_fTotalDistance = {};
	float m_fDistanceTravelled = {};
	float m_fTargetYaw = {};

	float m_fTargetYawDelta = 45.f;

private:

	std::vector<playback_cmd> m_oVecCmds;

};

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

	[[nodiscard]] bool CanStepForward(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) noexcept;
	void StepForward(const playerState_s* ps, usercmd_s* cmd);

	[[nodiscard]] bool CanStepSideways(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) noexcept;
	void StepSideways(const playerState_s* ps, usercmd_s* cmd);

	bool m_bWalkingIsTooDangerous = false;
	bool m_bSteppingForwardIsTooDangerous = false;
};

class CElebot
{
	NONCOPYABLE(CElebot);

public:

	CElebot(const playerState_s* ps, axis_t axis, float targetPosition);

	[[nodiscard]] ElebotUpdate_f Update;

private:

	std::unique_ptr<CGroundElebot> m_pGroundMove;

};



class CStaticElebot
{
public:

	//this only has a value when the elebot is doing something, otherwise nullptr
	static std::unique_ptr<CElebot> Instance;

	static void EB_MoveToCursor();

private:

};
