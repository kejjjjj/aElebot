#pragma once

struct playerState_s;
struct usercmd_s;
struct pmove_t;

class CElebotBase;

class CElebotStandup
{
public:
	CElebotStandup(CElebotBase& base);
	[[nodiscard]] bool Update(const playerState_s* ps, usercmd_s* cmd, const usercmd_s* oldcmd);

private:
	[[nodiscard]] bool GroundMove(const playerState_s* ps, usercmd_s* cmd, const usercmd_s* oldcmd);
	[[nodiscard]] bool AirMove(const playerState_s* ps, usercmd_s* cmd, const usercmd_s* oldcmd);

	[[nodiscard]] bool CeilingExists(const playerState_s* ps, usercmd_s* cmd, const usercmd_s* oldcmd);
	[[nodiscard]] bool StandupWillElevate(pmove_t& pmOrg);


	bool m_bCeilingExists = {};
	CElebotBase& m_oRefBase;
};


