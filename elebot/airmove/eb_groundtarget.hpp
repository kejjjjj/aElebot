#pragma once

#include "eb_airmove.hpp"

class CPmoveSimulation;
class CBlockElebot;

using playback_cmds = std::vector<playback_cmd>;


class CElebotGroundTarget : public CAirElebotVariation
{
public:
	CElebotGroundTarget(CElebotBase& base);
	~CElebotGroundTarget();

	[[nodiscard]] ElebotUpdate_f Update override;

protected:
	constexpr eElebotVariation type() const noexcept override { return ground; };

private:
	bool UpdateBlocker(const playerState_s* ps, usercmd_s* cmd, usercmd_s* oldcmd);
	bool GetBlocker(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd);
	std::unique_ptr<pmove_t> TryGoingUnderTheBlocker(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const;


	[[nodiscard]] bool CanStep(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const;
	void Step(const playerState_s* ps, usercmd_s* cmd);



	//represents the playerstate when the player is at a state where it can move under the blocker
	//only holds a value when the player is being velocity clipped by a wall
	std::unique_ptr<pmove_t> m_pBlockerAvoidState = 0;
	mutable std::unique_ptr<playerState_s> m_pBlockerAvoidPlayerState = 0;

	//finds the correct inputs given enough iterations
	std::unique_ptr<CBlockElebot> m_pBruteForcer;

	//the inputs that take the player to the point where it's no longer being blocked
	//and if the brute forcer finds the inputs, then  
	mutable playback_cmds m_oCmds;

	//if the player's origin is less than this, we know that it's too late to standup
	float m_fBlockerMinHeight = {};
	mutable float m_fBestDistance = FLT_MAX;
};
