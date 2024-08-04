#include "eb_block.hpp"

#include "bg/bg_pmove.hpp"
#include "bg/bg_pmove_simulation.hpp"

#include "cg/cg_local.hpp"
#include "cg/cg_client.hpp"

#include <cassert>
#include <iostream>
#include <ranges>
#include <com/com_channel.hpp>
#include <iomanip>
#include <algorithm>

// Set a tolerance for floating-point comparison
constexpr float tolerance = 0.01f;

CElebotInput::CElebotInput(const pmove_t& pm, std::int32_t fps) :
	m_oPMove(std::make_unique<pmove_t>(pm)),
	m_oPlayerstate(std::make_unique<playerState_s>(*pm.ps)),
	m_iFPS(fps) {
	m_oPMove->ps = m_oPlayerstate.get();
}
CElebotInput::CElebotInput(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd, std::int32_t fps) :
	m_oPlayerstate(std::make_unique<playerState_s>(*ps)),
	m_iFPS(fps)  {
	m_oPMove = std::make_unique<pmove_t>(PM_Create(m_oPlayerstate.get(), cmd, oldcmd));
}


CBlockElebot::CBlockElebot(CElebotBase& base, const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd, float minHeight) :
	CAirElebotVariation(base),
	m_fMinHeight(minHeight),
	m_pPlayerState(std::make_unique<playerState_s>(*ps)),
	m_pPMove(std::make_unique<pmove_t>(PM_Create(m_pPlayerState.get(), cmd, oldcmd))) {
}

CBlockElebot::~CBlockElebot() = default;

bool CBlockElebot::Update([[maybe_unused]] const playerState_s* ps, [[maybe_unused]] usercmd_s* cmd, [[maybe_unused]] usercmd_s* oldcmd)
{

	if (!m_oFirstStep && !(m_oFirstStep = GetFirstStep()))
		return false;

	assert(m_oFirstStep != nullptr);

	if (HasFinished()) {

		//looks like it's gg -> now wait until we can playback the cmds
		if (WaitForExit(ps, cmd, oldcmd))
			return true;

		//ok we can now do the playback
		m_oRefBase.PushPlayback(m_oPerformer.m_oInputs);
		return false;
	}

	if (ps->origin[Z] <= m_fMinHeight) {
		//bro fell off 
		return false;
	}

	if (!FindInputs(*m_oFirstStep)) {
		//didn't find the inputs -> give up to avoid spam
		return false;
	}

	//keep goink
	return true;
}
pmove_t CBlockElebot::GetInitialState() const
{
	//would be unfortunate if this got destructed when this function ends, right guys lol oh wait no one will ever read this
	static playerState_s ps_local;
	
	ps_local = *m_pPlayerState;	
	auto pm = PM_Create(&ps_local, &m_pPMove->cmd, &m_pPMove->oldcmd);

	return pm;
}

CSimulationController CBlockElebot::GenericCrouchedForwardmoveController(const usercmd_s* cmd) const
{
	CSimulationController c;

	c.forwardmove = 127;
	c.rightmove = 0;
	c.weapon = cmd->weapon;
	c.offhand = cmd->offHandIndex;

	//only crouch
	c.buttons = cmdEnums::crouch | cmdEnums::crouch_hold;

	c.viewangles = CSimulationController::CAngles{
		.viewangles = {
			m_pPlayerState->viewangles[PITCH],
			m_oRefBase.m_fTargetYaw,
			m_pPlayerState->viewangles[ROLL]
		},
		.angle_enum = EViewAngle::FixedAngle, .smoothing = 0.f
	};

	return c;
}

/* find the lowest fps we can use to move forward without overstepping */
std::unique_ptr<CElebotInput> CBlockElebot::GetFirstStep() const
{
	constexpr auto INITIAL_FRAMETIME = 1000u / ELEBOT_FPS;
	pmove_t pm{};
	const auto& base = m_oRefBase;
	CPmoveSimulation sim(&pm, GenericCrouchedForwardmoveController(&m_pPMove->cmd));

	//start at ELEBOT_FPS
	auto frameTime = INITIAL_FRAMETIME;

	do {
		pm = GetInitialState();
		sim.FPS = 1000 / frameTime++;

		if (!sim.FPS || !sim.Simulate())
			return 0;

	} while (!IsPointTooFar(pm.ps->origin[base.m_iAxis]));

	if ((frameTime - 1) == INITIAL_FRAMETIME) {
		//this means that the elebot fps is already overstepping
		//to fix this, the bot will do one step instead of two
		m_bOneStep = true;
		frameTime--;

	} else {
		//great, now we know this fps takes us too far
		//go to the previous fps
		frameTime -= 2;
	}

	assert(frameTime > 0);

	return std::make_unique<CElebotInput>(GetInitialState(), 1000 / frameTime);

}
constexpr float CBlockElebot::BinarySearchForFloat(CElebotInput& input) const{

	float& min = input.m_fMinYawDelta;
	float& max = input.m_fMaxYawDelta;

	// Get midpoint
	const auto delta = min + (max - min) / 2.0f;

	if (IsPointTooFar(input.m_oPlayerstate->origin[m_oRefBase.m_iAxis])) 
		min = delta;
	else 
		max = delta;
	

	// Return the midpoint
	return min + (max - min) / 2.0f;
}

bool CBlockElebot::FindInputs(CElebotInput& firstInput)
{
	assert(firstInput.m_iFPS > 0);

	auto pm = firstInput.m_oPMove.get();

	CPmoveSimulation sim(pm, GenericCrouchedForwardmoveController(&m_pPMove->cmd));

	//keep going as long as we still have a meaningful half
	while (int(firstInput.m_fYawDelta) && (firstInput.m_fMaxYawDelta - firstInput.m_fMinYawDelta > tolerance)) {

		//do first step
		if (FindInputForStep(sim, firstInput, firstInput, true)) {
			if (OnCoordinateFound(sim, firstInput))
				return true;
		}

		if (m_bOneStep) {

			if (IsPointTooFar(firstInput.m_oPlayerstate->origin[m_oRefBase.m_iAxis])) {
				//went too far so fix the overstep
				FixOverstepForFirstStep(firstInput);
			}

			//this is a one step elevator, so no need to go further
			//just reset the state and try again
			ClearInputs(sim, firstInput);
			continue;
		}

		//save first input
		InsertInput(*firstInput.m_oPMove);

		//construct second input (the precise one) using previous state
		auto preciseInput = CElebotInput(*firstInput.m_oPMove, ELEBOT_FPS);

		//yep u guessed it now perform the precise step
		if (FindInputForStep(sim, firstInput, preciseInput, false)) {
			if (OnCoordinateFound(sim, preciseInput))
				return true;
		}

		//it looks like this went to shit, reset the state!
		ClearInputs(sim, firstInput);
	}

	Com_Printf("^1didn't find inputs\n");
	return false;
}
bool CBlockElebot::FindInputForStep(CPmoveSimulation& sim, CElebotInput& parent, CElebotInput& input, bool isFirstInput)
{
	const auto& base = m_oRefBase;
	auto oldPm = sim.pm;
	auto oldState = *oldPm->ps;
	auto& angles = sim.GetAngles();

	auto& min = input.m_fMinYawDelta;
	auto& max = input.m_fMaxYawDelta;
	auto& delta = input.m_fYawDelta;

	std::vector<float> positions;

	//simulation now edits the input state
	sim.pm = input.m_oPMove.get();
	sim.FPS = input.m_iFPS;

	do {

		//reset state
		*input.m_oPlayerstate = oldState;
		*input.m_oPMove = PM_Create(input.m_oPlayerstate.get(), &oldPm->cmd, &oldPm->oldcmd);
		auto* pm = input.m_oPMove.get();


		//use the midpoint as yaw
		delta = BinarySearchForFloat(input);
		angles.right = base.m_fTargetYaw + delta;
		sim.Simulate();
		
		if (base.HasFinished(pm->ps)) {
			return true;
		}

		//first input can only iterate once per precise step loop
		if (isFirstInput) 
			return false;
		
		positions.push_back(pm->ps->origin[base.m_iAxis]);

	} while (int(delta) && (delta < 90.f - tolerance) && (max - min > tolerance));


	//if all predicted positions overstepped, fix the delta for the first step
	if (std::ranges::all_of(positions, [this](float p) { return IsPointTooFar(p); })) 
		FixOverstepForFirstStep(parent);
	
	return false;

}
[[nodiscard]] constexpr bool CBlockElebot::IsPointTooFar(float p) const noexcept
{
	return m_oRefBase.IsPointTooFar(p);
}
void CBlockElebot::FixOverstepForFirstStep(CElebotInput& input)
{
	input.m_fMinYawDelta = input.m_fYawDelta * 1.5f;
	if (input.m_fMinYawDelta > input.m_fMaxYawDelta)
		std::swap(input.m_fMinYawDelta, input.m_fMaxYawDelta);
}
bool CBlockElebot::OnCoordinateFound(CPmoveSimulation& sim, CElebotInput& input)
{
	InsertInput(*input.m_oPMove);

	//test if we elevate when standing up
	if (ValidateResult(sim)) {
		Com_Printf("^2elevator detected\n");
		//gg we even elevated
		return true;
	}

	//too much fall speed!
	Com_Printf("stood up ^1%.6fu^7 too late\n", (m_fMinHeight - sim.pm->ps->origin[Z]));
	return false;

}
void CBlockElebot::InsertInput(pmove_t& pm)
{
	m_oPerformer.m_uNumInputs++;
	m_oPerformer.m_oInputs.emplace_back(playback_cmd::FromPlayerState(pm.ps, &pm.cmd, &pm.oldcmd));
	memcpy(&pm.oldcmd, &pm.cmd, sizeof(usercmd_s));
}
void CBlockElebot::ClearInputs(CPmoveSimulation& sim, CElebotInput& input) noexcept
{

	*input.m_oPlayerstate = *m_pPlayerState;
	*input.m_oPMove = PM_Create(input.m_oPlayerstate.get(), &m_pPMove->cmd, &m_pPMove->oldcmd);
	sim.pm = &*input.m_oPMove;

	m_oPerformer.m_uNumInputs = 0;
	m_oPerformer.m_oInputs.clear();
}
bool CBlockElebot::ValidateResult(CPmoveSimulation& sim)
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

	//do 10 +gostand simulations
	for ([[maybe_unused]] const auto iter : std::views::iota(0, 10)) {
		sim.Simulate();
		new_cmds.emplace_back(playback_cmd::FromPlayerState(pm->ps, &pm->cmd, &pm->oldcmd));
		memcpy(&pm->oldcmd, &pm->cmd, sizeof(usercmd_s));
	}

	if (ps->origin[Z] >= oldHeight) {
		//if height increased then we can assume that the elevator was indeed successful
		m_oPerformer.m_oInputs.insert(m_oPerformer.m_oInputs.end(), new_cmds.begin(), new_cmds.end());
		m_oPerformer.m_uNumInputs = m_oPerformer.m_oInputs.size();
		m_oPerformer.hasCorrectInputs = true;
		return true;
	}

	return false;

}
bool CBlockElebot::WaitForExit(const playerState_s* ps, const usercmd_s* cmd, const usercmd_s* oldcmd) const
{
	assert(m_oPerformer.hasCorrectInputs == true);

	//return ps->origin[Z] > m_oPerformer.m_oInputs[0].origin[Z];


	//playerState_s ps_local = *ps;
	//CPmoveSimulation::PredictNextPosition(&ps_local, cmd, oldcmd, ELEBOT_FPS);

	return m_oRefBase.IsVelocityBeingClipped(ps, cmd, oldcmd);

}