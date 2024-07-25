#pragma once
#include "eb_airmove.hpp"
#include "../aWorld/cm/cm_typedefs.hpp"

class CElebotWorldTarget : public CAirElebotVariation
{
public:
	CElebotWorldTarget(CElebotBase& base, const cbrush_t* brush, const sc_winding_t& winding);
	~CElebotWorldTarget();

	[[nodiscard]] ElebotUpdate_f Update override;


protected:
	constexpr eElebotVariation type() const noexcept override { return ground; };

private:
	const cbrush_t* m_pBrush = 0;
	const sc_winding_t m_oWinding{};
};