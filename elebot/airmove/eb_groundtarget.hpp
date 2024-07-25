#pragma once

#include "eb_airmove.hpp"

class CElebotGroundTarget : public CAirElebotVariation
{
public:
	CElebotGroundTarget(CElebotBase& base);
	~CElebotGroundTarget();

	[[nodiscard]] ElebotUpdate_f Update override;

protected:
	constexpr eElebotVariation type() const noexcept override { return ground; };

private:
	[[nodiscard]] bool CanStep(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const;
	void Step(const playerState_s* ps, usercmd_s* cmd);
};
