#include "cg/cg_local.hpp"
#include "cg/cg_offsets.hpp"
#include "cl/cl_utils.hpp"
#include "r/gui/r_main_gui.hpp"
#include "r/r_drawtools.hpp"
#include "r/r_utils.hpp"
#include "r_drawactive.hpp"
#include "utils/hook.hpp"
#include "utils/typedefs.hpp"

#include <format>

void CG_DrawActive()
{
	if (R_NoRender())
#if(DEBUG_SUPPORT)
		return hooktable::find<void>(HOOK_PREFIX(__func__))->call();
#else
		return;
#endif

	const std::string text = std::format(
		"x:      {:.6f}\n"
		"y:      {:.6f}\n"
		"z:      {:.6f}\n"
		"yaw: {:.6f}\n"
		"buttons: {}",
		clients->cgameOrigin[0],
		clients->cgameOrigin[1],
		clients->cgameOrigin[2],
		clients->cgameViewangles[YAW],
		CL_GetUserCmd(clients->cmdNumber-1)->buttons
		);

	R_AddCmdDrawTextWithEffects(text, "fonts/normalFont", fvec2{ 310, 400 }, {0.4f, 0.5f}, 0.f, 3, vec4_t{1,1,1,1}, vec4_t{1,0,0,0});


#if(DEBUG_SUPPORT)
	return hooktable::find<void>(HOOK_PREFIX(__func__))->call();
#endif

}