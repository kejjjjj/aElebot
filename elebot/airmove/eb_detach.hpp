#pragma once
#include "eb_airmove.hpp"

struct CSimulationController;

//yep this fella will detach you from the wall while you're elevating while still
//preserving a coordinate you can re-elevate from
class CDetachElebot : public CAirElebotVariation
{
public:
	CDetachElebot(CElebotBase& base);
	~CDetachElebot();

	[[nodiscard]] ElebotUpdate_f Update override;

protected:
	[[nodiscard]] constexpr eElebotVariation type() const noexcept override { return detach; };

private:
	[[nodiscard]] CSimulationController GenericCrouchedBackwardMoveController(const playerState_s* ps, const usercmd_s* cmd) const;
	[[nodiscard]] bool CanMoveAwayFromTheWall(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const;
	void MoveAwayFromTheWall(const playerState_s* ps, const usercmd_s* cmd) const;

	[[nodiscard]] bool FindDetachAngle(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const;
	void Detach(const playerState_s* ps, const usercmd_s* cmd) const;

};