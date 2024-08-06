#pragma once

#include <span>

#include "cg/cg_angles.hpp"
#include "eb_airmove.hpp"


class CPmoveSimulation;
struct CSimulationController;
using playback_cmds = std::vector<playback_cmd>;
struct CElebotInput
{
	NONCOPYABLE(CElebotInput);

	CElebotInput(const pmove_t& pm, std::int32_t fps);
	CElebotInput(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd, std::int32_t fps);

	std::unique_ptr<pmove_t> m_oPMove;
	std::unique_ptr<playerState_s> m_oPlayerstate;

	std::int32_t m_iFPS = 333;
	float m_fMinYawDelta = 0.f;
	float m_fMaxYawDelta = 90.f;
	float m_fYawDelta = 90.f;
};

struct CBlockElebotPerformer
{
	std::uint32_t m_uNumInputs{};
	playback_cmds m_oInputs;
	bool hasCorrectInputs = false;
};

using CElebotInputs = std::vector<CElebotInput>;
using CElebotInputsView = std::span<CElebotInput>;


class CBlockElebot : public CAirElebotVariation
{

public:
	CBlockElebot(CElebotBase& base, const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd, float minHeight);
	~CBlockElebot();

	[[nodiscard]] ElebotUpdate_f Update override;

protected:
	[[nodiscard]] constexpr eElebotVariation type() const noexcept override { return block; };

	//if the player's origin is less than this, we know that it's too late to standup
	float m_fMinHeight{};
private:

	[[nodiscard]] pmove_t GetInitialState() const;
	[[nodiscard]] CSimulationController GenericCrouchedForwardmoveController(const usercmd_s* cmd) const;

	[[nodiscard]] float BinarySearchForFloat(CElebotInput& input) const;
	[[nodiscard]] std::unique_ptr<CElebotInput> GetFirstStep() const;
	[[nodiscard]] bool FindInputs(CElebotInput& firstInput);
	[[nodiscard]] bool FindInputForStep(CPmoveSimulation& sim, CElebotInput& parent, CElebotInput& input, bool isFirstInput);
	[[nodiscard]] constexpr bool IsPointTooFar(float p) const noexcept;

	void FixOverstepForFirstStep(CElebotInput& input);
	void InsertInput(pmove_t& pm);
	void ClearInputs(CPmoveSimulation& sim, CElebotInput& input) noexcept;

	[[nodiscard]] bool OnCoordinateFound(CPmoveSimulation& sim, CElebotInput& input);
	[[nodiscard]] bool ValidateResult(CPmoveSimulation& sim);
	[[nodiscard]] constexpr bool HasFinished() const noexcept { return m_oPerformer.hasCorrectInputs; }

	//on finish
	[[nodiscard]] bool WaitForExit(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const;


	//reference states
	std::unique_ptr<playerState_s> m_pPlayerState = 0;
	std::unique_ptr<pmove_t> m_pPMove = 0;

	CBlockElebotPerformer m_oPerformer;

	std::unique_ptr<CElebotInput> m_oFirstStep = 0;
	std::unique_ptr<CElebotInput> m_oPreciseStep = 0;

	//if true, it means that this elevator can be performed with just one step
	mutable bool m_bOneStep = false;
};
