#pragma once

#include <memory>
#include <vector>

#include "cg/cg_angles.hpp"


constexpr auto ELEBOT_FPS = 333u;


class CGroundElebot;
class CAirElebot;

struct playback_cmd;

using ElebotUpdate_f = bool(const playerState_s* ps, usercmd_s* cmd, usercmd_s* oldcmd);

class CElebotBase
{
	friend class CElebot;
	friend class CAirElebotVariation;
	friend class CElebotGroundTarget;
	friend class CElebotWorldTarget;
	friend class CQLearnElebot;
	friend class CBlockElebot;

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
	[[nodiscard]] constexpr float GetDistanceTravelled(const float p, bool isUnSignedDirection) const noexcept;
	[[nodiscard]] inline constexpr bool IsUnsignedDirection() const noexcept { return m_iDirection == N || m_iDirection == W; }

	[[nodiscard]] virtual constexpr bool IsPointTooFar(const float p) const noexcept;
	[[nodiscard]] virtual constexpr bool IsPointInWrongDirection(const float p) const noexcept;

	[[nodiscard]] virtual constexpr bool IsMovingBackwards() const noexcept;
	[[nodiscard]] virtual constexpr bool IsCorrectVelocityDirection(float velocity) const noexcept;

	[[nodiscard]] virtual constexpr float GetTargetYawForSidewaysMovement() const noexcept;

	virtual void ApplyMovementDirectionCorrections(const playerState_s* ps) noexcept;

	[[nodiscard]] bool IsVelocityBeingClipped(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const;

	playback_cmd StateToPlayback(const playerState_s* ps, const usercmd_s* cmd, std::uint32_t fps = ELEBOT_FPS) const;
	void EmplacePlaybackCommand(const playerState_s* ps, const usercmd_s* cmd);

	axis_t m_iAxis = {};
	cardinal_dir m_iDirection = {};


	float m_fTargetPosition = {};
	float m_fTotalDistance = {};
	float m_fDistanceTravelled = {};

	float m_fOldOrigin = {};

	float m_fTargetYaw = {};
	float m_fTargetYawDelta = 45.f;

	std::int8_t m_cRightmove = -127;
	std::int8_t m_cForwardMove = 127;

	std::int8_t m_cForwardDirection = 127;

private:


	std::vector<playback_cmd> m_oVecCmds;

};


class CElebot
{
	NONCOPYABLE(CElebot);

public:

	CElebot(const playerState_s* ps, axis_t axis, float targetPosition);
	~CElebot();
	[[nodiscard]] ElebotUpdate_f Update;
private:

	std::unique_ptr<CGroundElebot> m_pGroundMove;
	std::unique_ptr<CAirElebot> m_pAirMove;

};



class CStaticElebot
{
public:

	//this only has a value when the elebot is doing something, otherwise nullptr
	static std::unique_ptr<CElebot> Instance;

	static void EB_MoveToCursor();

private:

};
