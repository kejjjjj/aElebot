#pragma once

#include "../eb_main.hpp"

class CElebotGroundTarget;
class CElebotWorldTarget;

struct sc_winding_t;
struct cbrush_t;

enum eElebotVariation
{
	world,
	ground,
	qlearn,
	block,
	detach
};

class CAirElebotVariation
{
public:
	CAirElebotVariation(CElebotBase& base);
	virtual ~CAirElebotVariation();
	
	[[nodiscard]] virtual ElebotUpdate_f Update = 0;
	[[nodiscard]] virtual constexpr eElebotVariation type() const noexcept = 0;

protected:
	CElebotBase& m_oRefBase;
};

class CAirElebot : public CElebotBase
{
public:
	CAirElebot(const playerState_s* ps, const init_data& init);
	~CAirElebot();

	void SetGroundTarget();
	void SetToDetach();

	constexpr void SetWorldTarget(const cbrush_t* brush, const sc_winding_t& winding);

	[[nodiscard]] ElebotUpdate_f Update override;

private:
	[[nodiscard]] constexpr inline bool IsTargetingGround() const { return m_pElebotVariation && m_pElebotVariation->type() == ground; };
	[[nodiscard]] constexpr inline bool IsTargetingWorld() const { return m_pElebotVariation && m_pElebotVariation->type() == world; };

	std::unique_ptr<CAirElebotVariation> m_pElebotVariation = 0;

};
