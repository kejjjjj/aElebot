#pragma once

#include <memory>
#include <vector>
#include <format>
#include <sstream>

#include "cg/cg_angles.hpp"
#include "utils/typedefs.hpp"
#include "com/com_channel.hpp"

constexpr auto ELEBOT_FPS = 333u;


class CGroundElebot;
class CAirElebot;
class CElebotStandup;

struct playback_cmd;
struct trace_t;

using ElebotUpdate_f = bool(const playerState_s* ps, usercmd_s* cmd, usercmd_s* oldcmd);

enum elebot_flags
{
	groundmove = 1 << 1,
	airmove = 1 << 2,
	all_flags = 0xFFFFFFFF
};

struct init_data {
	axis_t m_iAxis{};
	float m_fTargetPosition{};
	elebot_flags m_iElebotFlags = all_flags;
	fvec3 m_vecTargetNormals;
	bool m_bAutoStandUp = true;
	bool m_bPrintf = false;
};

class CElebotBase
{
	friend class CElebot;
	friend class CAirElebotVariation;
	friend class CElebotGroundTarget;
	friend class CElebotWorldTarget;
	friend class CQLearnElebot;
	friend class CBlockElebot;
	friend class CDetachElebot;
	friend class CElebotStandup;

	friend void CL_FinishMove(usercmd_s* cmd);

public:
	CElebotBase(const playerState_s* ps, const init_data& init);
	virtual ~CElebotBase();

	[[nodiscard]] virtual ElebotUpdate_f Update = 0;

	void PushPlayback();
	void PushPlayback(const std::vector<playback_cmd>& cmds) const;

	void OnFrameStart(const playerState_s* ps) noexcept;

	[[nodiscard]] virtual bool HasFinished(const playerState_s* ps) const noexcept;

	//something silly happened and it's probably better if you reinitialize the elebot with the same data
	[[nodiscard]] constexpr inline bool TryAgain() const noexcept { return m_bTryAgain; }

protected:
	void ResolveYawConflict(const fvec3& normals);

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

	template<typename ... Args>
	void Elebot_Printf(const Args... args) {

		std::stringstream ss;
		((ss << std::format("{}", args)), ...);

		const auto str = ss.str();

		if (m_bPrintf)
			Com_Printf(str.c_str());
	}

	struct velocityClipping_t
	{
		const playerState_s* ps;
		const usercmd_s* cmd;
		const usercmd_s* oldcmd;
		float boundsDelta = 0.125f;
		float clipPos = {};
		trace_t* trace = nullptr;
	};

	[[nodiscard]] bool IsVelocityBeingClipped(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const;
	[[nodiscard]] bool IsVelocityBeingClippedAdvanced(velocityClipping_t& init) const;

	[[nodiscard]] playback_cmd StateToPlayback(const playerState_s* ps, const usercmd_s* cmd, std::uint32_t fps = ELEBOT_FPS) const;
	void EmplacePlaybackCommand(const playerState_s* ps, const usercmd_s* cmd);

	axis_t m_iAxis = {};
	cardinal_dir m_iDirection = {};

	float m_fTargetPosition = {};
	float m_fTotalDistance = {};
	float m_fDistanceTravelled = {};

	float m_fOldOrigin = {};

	float m_fInitialYaw = {};
	float m_fTargetYaw = {};
	float m_fTargetYawDelta = 45.f;

	std::int8_t m_cRightmove = -127;
	std::int8_t m_cForwardMove = 127;
	std::int8_t m_cForwardDirection = 127;

	bool m_bTryAgain = false;
	bool m_bPrintf = false;
private:


	std::vector<playback_cmd> m_oVecCmds;

};


class CElebot
{
	NONCOPYABLE(CElebot);
	friend void CL_FinishMove(usercmd_s* cmd);

public:


	CElebot(const playerState_s* ps, const init_data& init);
	~CElebot();
	[[nodiscard]] ElebotUpdate_f Update;
	[[nodiscard]] bool TryAgain() const noexcept;
private:

	init_data m_oInit{};

	CElebotBase* GetMove(const playerState_s* ps);

	std::unique_ptr<CGroundElebot> m_pGroundMove;
	std::unique_ptr<CAirElebot> m_pAirMove;
	std::unique_ptr<CElebotStandup> m_pStandup;

};

struct traceResults_t;

class CStaticElebot
{
public:

	//this only has a value when the elebot is doing something, otherwise nullptr
	static std::unique_ptr<CElebot> Instance;

	static void EB_MoveToCursor();
	static void EB_CenterYaw();

	static void EB_StartAutomatically(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd);

	[[nodiscard]] static std::unique_ptr<traceResults_t> EB_VelocityGotClipped(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd);
	[[nodiscard]] static std::unique_ptr<traceResults_t> EB_VelocityGotClippedInTheAir(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd);


private:
	static float m_fOldAutoPosition;
	

};
