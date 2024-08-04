
#if(DEBUG_SUPPORT)

#include "eb_debug.hpp"

#include "bg/bg_pmove_simulation.hpp"

#include "cg/cg_local.hpp"
#include "r/r_utils.hpp"
#include "r/backend/rb_endscene.hpp"
#include "utils/typedefs.hpp"
#include "utils/hook.hpp"

void RB_DrawDebug([[maybe_unused]] GfxViewParms* viewParms)
{

	if (R_NoRender())
#if(DEBUG_SUPPORT)
		return hooktable::find<void, GfxViewParms*>(HOOK_PREFIX(__func__))->call(viewParms);
#else
		return;
#endif

#if(DEBUG_SUPPORT)
	hooktable::find<void, GfxViewParms*>(HOOK_PREFIX(__func__))->call(viewParms);
#endif


}

//void adapsdsadj(const pmove_t* pm)
//{
//	if (!pm || !pm->ps)
//		return;
//
//	const fvec3 org = pm->ps->origin;
//
//
//	RB_DrawBoxEdges(org + pm->mins, org + pm->maxs, true, vec4_t{1,1,0,0.5f});
//
//
//}

#endif