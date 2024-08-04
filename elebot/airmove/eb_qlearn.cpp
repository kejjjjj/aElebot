
#if(USE_QLEARNING)

#include "eb_qlearn.hpp"

#include "bg/bg_pmove.hpp"
#include "bg/bg_pmove_simulation.hpp"

#include "cmd/cmd.hpp"

#include "cg/cg_local.hpp"

#include "utils/functions.hpp"

#include <cmath>
#include <ranges>
#include <cassert>
#include <iostream>
#include <com/com_channel.hpp>


constexpr auto MAX_ATTEMPTS_PER_FRAME = 50u;
constexpr auto MAX_INPUTS = 10u;


CElebotState::CElebotState()
{
	m_oArrActions[0].m_eDirection = N;
	m_oArrActions[1].m_eDirection = E;
	m_oArrActions[2].m_eDirection = S;
	m_oArrActions[3].m_eDirection = W;
}
std::uint32_t CElebotState::ChooseAction() noexcept
{

	//explore (20% chance)
	if (random(0.0f, 1.0f) <= 0.2f) {
		const auto iaction = random(QLEARN_NUM_ACTIONS);

		assert(iaction < QLEARN_NUM_ACTIONS);

		//set random delta (30% chance)
		if (random(1.f) <= 0.30f) {
			m_oArrActions[iaction].m_fYawDelta = random(-45.5f, 45.5f);
		}

		return iaction;
	}

	auto best_action = 0u;
	for (const auto action :  std::views::iota(0u, QLEARN_NUM_ACTIONS))
	{
		if (m_dArrValue[action] > m_dArrValue[best_action]) {
			best_action = action;
		}
	}

	return best_action;

}
constexpr CElebotAction& CElebotState::GetAction(const std::uint32_t action)
{
	assert(action < QLEARN_NUM_ACTIONS);
	return m_oArrActions[action];
}
//this action is sucky wucky, make it less likely to happen again
void CElebotState::OnFatalMistake(std::uint32_t action) noexcept
{
	assert(action < QLEARN_NUM_ACTIONS);

	m_dArrValue[action] = -1.0;
}

/***********************************************************************
 >							CQLearnElebot
***********************************************************************/

CQLearnElebot::CQLearnElebot(CElebotBase& base, const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd, float minHeight) :
	CAirElebotVariation(base), 
	m_fMinHeight(minHeight),
	m_pPlayerState(std::make_unique<playerState_s>(*ps)),
	m_pMove(std::make_unique<pmove_t>(PM_Create(m_pPlayerState.get(), cmd, oldcmd))),
	m_pPerformer(std::make_unique<CQLearnElebotPerformer>())
{
	CreateQTable(ps->origin[base.m_iAxis], base.m_fTargetPosition);
}
CQLearnElebot::~CQLearnElebot() {
}

bool CQLearnElebot::Update([[maybe_unused]] const playerState_s* ps, [[maybe_unused]] usercmd_s* cmd, [[maybe_unused]] usercmd_s* oldcmd)
{

	if (m_oRefBase.HasFinished(ps)) {
		Com_Printf("^1stand up at %.6f\n", ps->origin[m_oRefBase.m_iAxis]);
		//cmd->buttons = 0;
		//CBuf_Addtext("+gostand; wait; wait; -gostand");
		return false;
	}

	if (ps->origin[Z] <= m_fMinHeight) {
		return false;
	}

	//finished!
	if (m_pSuccessfulPerformance) {

		if (!WaitForExit(ps)) {
			//push the playback
			m_oRefBase.m_oVecCmds = m_pSuccessfulPerformance->m_oInputs;
			m_pSuccessfulPerformance->m_oInputs.clear();
		}

		//keep waiting
		return true;
	}

	if (Simulate()) {

		if (!WaitForExit(ps)) {
			//push the playback
			m_oRefBase.m_oVecCmds = m_pSuccessfulPerformance->m_oInputs;
			m_pSuccessfulPerformance->m_oInputs.clear();
		}
	}

	return true;
}
const CQLearnElebotPerformer* CQLearnElebot::GetResults() const noexcept
{
	return m_pSuccessfulPerformance.get();
}
bool CQLearnElebot::Simulate()
{
	const auto& base = m_oRefBase;

	CSimulationController c;
	c.viewangles = CSimulationController::CAngles{
		.viewangles = m_pPlayerState->viewangles,
		.angle_enum = EViewAngle::FixedTurn, .smoothing = 0.f
	};

	playerState_s ps_local = *m_pPlayerState;
	m_pCurrentPmove = std::make_unique<pmove_t>(PM_Create(&ps_local, &m_pMove->cmd, &m_pMove->oldcmd));
	auto& pm = *m_pCurrentPmove;

	CPmoveSimulation sim(&pm, c);

	for ([[maybe_unused]]const auto attempt : std::views::iota(0u, MAX_ATTEMPTS_PER_FRAME)) {
		for ([[maybe_unused]]const auto input : std::views::iota(0u, MAX_INPUTS)) {

			const auto oldOrigin = pm.ps->origin[base.m_iAxis];
			auto& state = FindQ(oldOrigin);
			const auto iaction = state.ChooseAction();
			const auto& action = state.GetAction(iaction);

			if (!PerformAction(sim, action)) {
				
				//if the bot performed poorly, penalize it
				state.OnFatalMistake(iaction);
				OnNewAttempt(&pm);
				break;
			}

			const float newOrigin = pm.ps->origin[base.m_iAxis];

			const auto reward = GetReward(pm.ps);
			UpdateState(state, iaction, reward, FindQ(newOrigin));

			if (base.HasFinished(pm.ps)) {

				if (!ValidateResult(sim)) {
					//woops false positive
					
					auto& performer = m_pBestPerformer ? m_pBestPerformer : m_pPerformer;

					if (performer->m_uStepsTaken > 0) {
						//notify that the best steps we can take should be lower than this, because this amount was too slow
						performer->m_uStepsTaken--;
						performer->m_oInputs.pop_back();
					}
					state.OnFatalMistake(iaction);
					OnNewAttempt(&pm);

					Com_Printf("^1noppies\n");

					break;
				}
				Com_Printf("^2yappies\n");
				m_pSuccessfulPerformance = std::make_unique<CQLearnElebotPerformer>(*m_pPerformer);

				return true;
			}
		}
		OnNewAttempt(&pm);

	}

	return false;

}
bool CQLearnElebot::PerformAction(CPmoveSimulation& sim, const CElebotAction& action)
{

	if (m_pBestPerformer && m_pPerformer->m_uStepsTaken > m_pBestPerformer->m_uStepsTaken)
		return false; //too many steps!!!

	const auto& base = m_oRefBase;

	auto pm = sim.GetPM();
	auto ps = pm->ps;

	sim.FPS = ELEBOT_FPS;
	sim.forwardmove = action.m_eDirection == N ? 127 : -127;
	sim.rightmove = action.m_eDirection == E ? 127 : -127;
	sim.buttons = cmdEnums::crouch;
	sim.GetAngles().right = action.m_fYawDelta;

	if(!sim.Simulate())
		return false;

	m_pPerformer->m_uStepsTaken++;
	m_pPerformer->m_oInputs.emplace_back(base.StateToPlayback(ps, &pm->cmd));
	
	const auto distTravelled = base.GetDistanceTravelled(ps->origin[base.m_iAxis], base.IsUnsignedDirection());
	memcpy(&pm->oldcmd, &pm->cmd, sizeof(usercmd_s));

	//returns false if: 
	//	either moved to the wrong direction or went over the target
	//	didn't get the right inputs in time
	return ps->origin[Z] >= m_fMinHeight && distTravelled > 0.f; 

}

bool CQLearnElebot::ValidateResult(CPmoveSimulation& sim)
{
	//the algorithm thinks we did good? let's test

	playback_cmds new_cmds;

	sim.forwardmove = 0;
	sim.rightmove = 0;

	//gostand
	sim.buttons &= ~(cmdEnums::crouch | cmdEnums::crouch_hold);
	sim.buttons |= cmdEnums::jump;

	auto pm = sim.GetPM();
	auto ps = pm->ps;

	const auto oldHeight = ps->origin[Z];

	for ([[maybe_unused]]const auto iter : std::views::iota(0, 10)) {
		sim.Simulate();

		new_cmds.emplace_back(m_oRefBase.StateToPlayback(ps, &pm->cmd));

		memcpy(&pm->oldcmd, &pm->cmd, sizeof(usercmd_s));
	}

	if (ps->origin[Z] >= oldHeight) {
		//if height increased then we can assume that the elevator was indeed successful
		m_pPerformer->m_oInputs.insert(m_pPerformer->m_oInputs.end(), new_cmds.begin(), new_cmds.end());
		m_pPerformer->m_uStepsTaken = m_pPerformer->m_oInputs.size();
		return true;
	}

	return false;

}
double CQLearnElebot::GetReward(const playerState_s* ps) noexcept
{
	//this does not get called when the client moved incorrectly
	
	const auto& base = m_oRefBase;

	constexpr const auto stepPenalty = -0.1f;
	const auto distTravelled = base.GetDistanceTravelled(ps->origin[base.m_iAxis], base.IsUnsignedDirection());
	const float distance = base.m_fTotalDistance - distTravelled;

	const auto distanceReward = (base.m_fTotalDistance - distance) / base.m_fTotalDistance;


	//the more the client moved, the better the reward
	//const auto moveDelta = std::abs(static_cast<double>(currentOrigin - oldOrigin));
	auto distancePenalty = 0.0;

	if (distance >= m_pPerformer->m_fClosestDistance) {
		distancePenalty = 1.0;
	}
	else {
		m_pPerformer->m_fClosestDistance = distance;
	}
	auto stepReward = 0.0;

	//first iteration
	if (m_pBestPerformer) {

		if (m_pBestPerformer->m_uStepsTaken != m_pPerformer->m_uStepsTaken) {
			//penalize heavily if the bot took too many steps
			stepReward = static_cast<double>(m_pBestPerformer->m_uStepsTaken - m_pPerformer->m_uStepsTaken);
			//stepReward /= static_cast<double>(m_pBestPerformer->m_uStepsTaken);
			stepReward /= 5;
			//stepReward = std::clamp(stepReward, -1.0, 1.0);
		}

		if (m_pBestPerformer->m_uStepsTaken >= m_pPerformer->m_uStepsTaken) {

			//make sure it gets a positive reward for this epic maneuver
			if (stepReward <= 0.f)
				stepReward = 0.2; 

		}
	}

	const auto reward = distanceReward - distancePenalty + stepReward;

	if (reward < stepPenalty)
		return stepPenalty;

	return reward;

}
void CQLearnElebot::UpdateState(CElebotState& state, const std::uint32_t action, double reward, CElebotState& nextState) noexcept
{
	double learning_rate = 0.1;
	double discount_factor = 0.25; //keep the discount low so that there is more emphasis on the current rewards

	const auto nextAction = nextState.ChooseAction();
	const auto delta = learning_rate * (reward + discount_factor * nextState.m_dArrValue[nextAction] - state.m_dArrValue[action]);

	//state.m_dArrValue[action] = std::clamp(state.m_dArrValue[action] + delta, -1.0, 1.0);
	state.m_dArrValue[action] += delta;
}

void CQLearnElebot::OnNewAttempt(pmove_t* pm)
{
	if (pm) {
		*pm->ps = *m_pPlayerState;
		*pm = PM_Create(pm->ps, &m_pMove->cmd, &m_pMove->oldcmd);
	}

	if (m_pBestPerformer) {
		
		const bool hasFinished = m_pPerformer->m_fClosestDistance == 0.f;

	
		if (hasFinished) {
			//only take steps into account if finished
			if (m_pBestPerformer->m_uStepsTaken >= m_pPerformer->m_uStepsTaken) {
				*m_pBestPerformer = *m_pPerformer;
			}
		}
		else if (m_pPerformer->m_fClosestDistance < m_pBestPerformer->m_fClosestDistance) {
			*m_pBestPerformer = *m_pPerformer;
			//Com_Printf("%.6f -> %u\n", m_pBestPerformer->m_fClosestDistance, m_pBestPerformer->m_uStepsTaken);

		}

	}else
		m_pBestPerformer = std::make_unique<CQLearnElebotPerformer>(*m_pPerformer);

	m_pPerformer = std::make_unique<CQLearnElebotPerformer>();
}

CElebotState& CQLearnElebot::FindQ(float p)
{
	float delta = std::abs(p - m_oRefBase.m_fTargetPosition);

	if (!m_oQTable.contains(delta))
		std::cout << delta << '\n';

	assert(m_oQTable.contains(delta));

	try {
		return m_oQTable.at(delta);
	}
	catch ([[maybe_unused]] std::out_of_range& ex) {
		throw std::exception("!m_oQTable.contains(delta)");
	}

	throw std::exception("unknown exception");
}

void CQLearnElebot::CreateQTable(float from, float to)
{
	assert(from != to);

	const auto max = std::max(to, from);
	const auto min = std::min(from, to);
	const auto values = GetAllRepresentableValues(min, max);

	for (const auto& value : values) {
		const auto delta = std::abs(value - min);
		m_oQTable[delta] = CElebotState();
	}
}


bool CQLearnElebot::WaitForExit(const playerState_s* ps) const
{
	assert(m_pSuccessfulPerformance != nullptr);

	usercmd_s c, o;
	return m_oRefBase.IsVelocityBeingClipped(ps, &c, &o);

}

#endif