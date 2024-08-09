#pragma once

#include "eb_airmove.hpp"

class CPmoveSimulation;
class CBlockElebot;

using playback_cmds = std::vector<playback_cmd>;


using BlockFunc = std::unique_ptr<pmove_t>(CElebotGroundTarget::*)(const playerState_s*, const usercmd_s*, const usercmd_s*);

class CElebotGroundTarget : public CAirElebotVariation
{
public:
	CElebotGroundTarget(CElebotBase& base);
	~CElebotGroundTarget();

	[[nodiscard]] ElebotUpdate_f Update override;

protected:
	constexpr eElebotVariation type() const noexcept override { return ground; };

private:
	bool UpdateBlocker(const playerState_s* ps, usercmd_s* cmd, usercmd_s* oldcmd, BlockFunc func);
	bool GetBlocker(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd, const BlockFunc& func);
	std::unique_ptr<pmove_t> TryGoingUnderTheBlocker(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const;
	std::unique_ptr<pmove_t> TryGoingUnderTheBlockerFromDistance(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const;


	[[nodiscard]] bool ResetVelocity(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd);


	[[nodiscard]] bool CanStep(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const;
	void Step(const playerState_s* ps, usercmd_s* cmd);



	//represents the playerstate when the player is at a state where it can move under the blocker
	//only holds a value when the player is being velocity clipped by a wall
	std::unique_ptr<pmove_t> m_pBlockerAvoidState = 0;
	mutable std::unique_ptr<playerState_s> m_pBlockerAvoidPlayerState = 0;

	//finds the correct inputs given enough iterations when hugging a wall midair
	std::unique_ptr<CBlockElebot> m_pBruteForcer;

	//if the player's origin is less than this, we know that it's too late to standup
	float m_fBlockerMinHeight = {};
	mutable float m_fBestDistance = FLT_MAX;
};
