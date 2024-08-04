#pragma once

#define USE_QLEARNING 0

#if(USE_QLEARNING)

#include "cg/cg_angles.hpp"

#include "eb_airmove.hpp"

#include <array>
#include <memory>
#include <map>

struct CQLearnElebotPerformer;
using playback_cmds = std::vector<playback_cmd>;

constexpr const auto QLEARN_NUM_ACTIONS = 4u;

struct CElebotAction
{
	cardinal_dir m_eDirection{};
	float m_fYawDelta = {};
};

class CElebotState
{
public:
	CElebotState();
	std::uint32_t ChooseAction() noexcept;
	constexpr CElebotAction& GetAction(const std::uint32_t action);
	
	//this action is sucky wucky, make it less likely to happen again
	void OnFatalMistake(std::uint32_t action) noexcept;
	
	std::array<double, QLEARN_NUM_ACTIONS> m_dArrValue = {};
private:

	std::array<CElebotAction, QLEARN_NUM_ACTIONS> m_oArrActions;
};

class CPmoveSimulation;

class CQLearnElebot : public CAirElebotVariation
{

public:
	CQLearnElebot(CElebotBase& base, const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd, float minHeight);
	~CQLearnElebot();

	[[nodiscard]] ElebotUpdate_f Update override;

	[[nodiscard]] const CQLearnElebotPerformer* GetResults() const noexcept;

protected:
	[[nodiscard]] constexpr eElebotVariation type() const noexcept override { return ground; };

	//if the player's origin is less than this, we know that it's too late to standup
	float m_fMinHeight{};

private:
	//simulation
	[[nodiscard]] bool Simulate();
	[[nodiscard]] bool PerformAction(CPmoveSimulation& sim, const CElebotAction& action);
	[[nodiscard]] bool ValidateResult(CPmoveSimulation& sim);
	//evaluations
	[[nodiscard]] double GetReward(const playerState_s* ps) noexcept;
	void UpdateState(CElebotState& state, const std::uint32_t action, double reward, CElebotState& nextState) noexcept;

	void OnNewAttempt(pmove_t* pm = nullptr);

	//QTable
	[[nodiscard]] CElebotState& FindQ(float p);
	void CreateQTable(float from, float to);

	//on finish
	bool WaitForExit(const playerState_s* ps) const;

	std::map<float, CElebotState> m_oQTable;

	std::unique_ptr<playerState_s> m_pPlayerState = 0;

	std::unique_ptr<pmove_t> m_pMove = 0;
	std::unique_ptr<pmove_t> m_pCurrentPmove = 0;

	//used for comparisons
	std::unique_ptr<CQLearnElebotPerformer> m_pPerformer = 0;
	std::unique_ptr<CQLearnElebotPerformer> m_pBestPerformer = 0;
	std::unique_ptr<CQLearnElebotPerformer> m_pSuccessfulPerformance = 0;

};

struct CQLearnElebotPerformer
{
	std::uint32_t m_uStepsTaken{};
	playback_cmds m_oInputs{};
	float m_fClosestDistance = 10000.f;
};

#endif